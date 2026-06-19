// =============================================================================
// AppState.cpp
// =============================================================================
#include "app/AppState.h"
#include "engine/PlatformCommand.h"
#include "engine/diagnostic/DiagnosticEngine.h"
#include "app/NativeService.h"
#include "util/Logger.h"
#include <cstdio>
#include <chrono>
#include <thread>
#include <atomic>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QScrollArea>

AppState::AppState(QObject* parent) : QObject(parent) {
    // Enable all tests by default
    for (auto id : allTestIds()) m_enabledTests.insert(id);

    auto cmd = createPlatformCommand();
    m_engine = new DiagnosticEngine(std::move(cmd), this);

    QObject::connect(m_engine, &DiagnosticEngine::destroyed, this, [this]() { m_engine = nullptr; });

    // ARM64 workaround: C++ QTimer more reliable than QML Timers for polling
    auto* pollTimer = new QTimer(this);
    pollTimer->setInterval(200);
    QObject::connect(pollTimer, &QTimer::timeout, this, [this]() {
        // ARM64 workaround: periodically re-emit ALL NOTIFY signals so QML
        // bindings that may have missed the original signal will re-evaluate.
        emit progressChanged();
        emit runStatusChanged();
        emit targetChanged();
        emit currentTestChanged();
        emit groupChanged();
        emit portScanConfigChanged();
    });
    pollTimer->start();
}

AppState::~AppState() {
    if (m_runStatus == RunStatus::Running) {
        m_runStatus = RunStatus::Cancelled;
    }
}

// ── RFC 952/1123 hostname label validation ────────────────────────────────
static bool isValidHostLabel(const QString& label) {
    if (label.isEmpty() || label.size() > 63) return false;
    for (int i = 0; i < label.size(); ++i) {
        QChar c = label[i];
        if (!c.isLetterOrNumber() && c != '-') return false;
    }
    if (label.startsWith('-') || label.endsWith('-')) return false;
    return true;
}

static bool isValidIPv4(const QString& host) {
    const auto parts = host.split('.');
    if (parts.size() != 4) return false;
    for (const auto& p : parts) {
        bool ok = false;
        int v = p.toInt(&ok);
        if (!ok || v < 0 || v > 255) return false;
        if (p.size() > 1 && p.startsWith('0')) return false;
    }
    return true;
}

static bool looksLikeIPv6(const QString& host) {
    return host.contains(':');
}

// ── Supported URL schemes (must be lowercase) ────────────────────────────
static const QStringList& supportedSchemes() {
    static const QStringList s = {"http", "https", "ftp", "ftps", "ssh", "scp"};
    return s;
}

static bool isValidHostname(const QString& host) {
    if (host.isEmpty() || host.size() > 253) return false;
    if (host.contains("..") || host == ".") return false;
    if (looksLikeIPv6(host)) return true;  // accept all IPv6
    if (isValidIPv4(host)) return true;
    const auto labels = host.split('.');
    for (const auto& label : labels) {
        if (!isValidHostLabel(label)) return false;
    }
    return true;
}

// ── Full URL validation ──────────────────────────────────────────────────
static QString validateUrl(const QString& trimmed) {
    // 1. Parse scheme
    int schemeEnd = trimmed.indexOf("://");
    if (schemeEnd < 0) return QString();  // no scheme → not a URL

    QString scheme = trimmed.left(schemeEnd).toLower();
    if (scheme.isEmpty()) return QStringLiteral("Empty URL scheme");
    if (!supportedSchemes().contains(scheme))
        return QStringLiteral("Unsupported protocol: %1:// — supported schemes: %2")
            .arg(scheme, supportedSchemes().join(", "));

    // 2. Parse authority (host[:port])
    QString afterScheme = trimmed.mid(schemeEnd + 3);
    int pathStart = afterScheme.indexOf('/');
    int queryStart = afterScheme.indexOf('?');
    int fragStart = afterScheme.indexOf('#');

    int authorityEnd = afterScheme.size();
    if (pathStart >= 0) authorityEnd = std::min(authorityEnd, pathStart);
    if (queryStart >= 0) authorityEnd = std::min(authorityEnd, queryStart);
    if (fragStart >= 0) authorityEnd = std::min(authorityEnd, fragStart);

    QString authority = afterScheme.left(authorityEnd);
    if (authority.isEmpty()) return QStringLiteral("URL has no hostname");

    // 3. Separate host and port
    QString host, portStr;
    int portColon = authority.lastIndexOf(':');
    // Check for IPv6 bracket notation [::1]:port
    if (authority.startsWith('[')) {
        int closing = authority.indexOf(']');
        if (closing < 0) return QStringLiteral("Invalid IPv6 bracket notation");
        host = authority.mid(1, closing - 1);
        if (closing + 1 < authority.size()) {
            if (authority[closing + 1] != ':') return QStringLiteral("Expected colon after IPv6 bracket");
            portStr = authority.mid(closing + 2);
        }
    } else if (portColon > 0) {
        host = authority.left(portColon);
        portStr = authority.mid(portColon + 1);
    } else {
        host = authority;
    }

    // 4. Validate hostname
    if (host.isEmpty()) return QStringLiteral("URL has no hostname");
    if (!isValidHostname(host)) {
        if (host.contains("..")) return QStringLiteral("Invalid hostname: consecutive dots");
        return QStringLiteral("Hostname label must be 1-63 alphanumeric chars (a-z, 0-9, -) and cannot start/end with hyphen");
    }

    // 5. Validate port
    if (!portStr.isEmpty()) {
        bool ok = false;
        int port = portStr.toInt(&ok);
        if (!ok) return QStringLiteral("Port must be a number");
        if (port < 1 || port > 65535)
            return QStringLiteral("Port must be between 1 and 65535 (got %1)").arg(port);
    }

    return QString();  // empty = success
}

// ── Target ─────────────────────────────────────────────────────────────────
void AppState::setTarget(const QString& t) {
    if (m_target != t) {
        m_target = t;
        m_targetError.clear();
        fprintf(stderr, "[TRACE] setTarget('%s')\n", m_target.toUtf8().constData());

        const QString trimmed = m_target.trimmed();
        bool has = !trimmed.isEmpty();

        // ── Unified validation ──────────────────────────────────────────
        if (has) {
            if (trimmed.contains("://")) {
                // URL path: validate scheme, hostname, optional port
                m_targetError = validateUrl(trimmed);
            } else {
                // Hostname/IP path
                if (!isValidHostname(trimmed)) {
                    if (trimmed.contains("..")) m_targetError = QStringLiteral("Invalid hostname: consecutive dots");
                    else m_targetError = QStringLiteral("Hostname label must be 1-63 alphanumeric chars (a-z, 0-9, -) and cannot start/end with hyphen");
                }
            }
        }

        bool isUrl = has && trimmed.contains("://");       // any :// → URL type
        bool isHttp = isUrl && isTargetHttpUrl();          // only http/https → G5

        // Deep diagnostic: why isTargetUrl() might return false
        QString scheme = trimmed.contains("://") ? trimmed.section("://", 0, 0).toLower() : QString();
        fprintf(stderr, "[TRACE] setTarget scheme='%s' empty=%d hasScheme=%d isTargetUrl=%d isTargetHttpUrl=%d validateErr='%s'\n",
                scheme.toUtf8().constData(), isTargetEmpty(), trimmed.contains("://"),
                isTargetUrl(), isTargetHttpUrl(), m_targetError.toUtf8().constData());

        // G4: always on when target non-empty (URL or host), G5: only http/https
        setGroupEnabled(3, has);          // G4 on if target non-empty
        setGroupEnabled(4, has && isHttp); // G5 on only for http/https
        fprintf(stderr, "[TRACE] setTarget result: has=%d isUrl=%d isHttp=%d G4=%d G5=%d err='%s'\n",
                has, isUrl, isHttp, has, has && isHttp, m_targetError.toUtf8().constData());
        emit targetChanged();
    }
}

// ── Port scan config ───────────────────────────────────────────────────────
void AppState::setPortScanCommon(bool v) { if (m_portScanCommon != v) { m_portScanCommon = v; emit portScanConfigChanged(); } }
void AppState::setPortScanFrom(int v) { if (m_portScanFrom != v) { m_portScanFrom = v; emit portScanConfigChanged(); } }
void AppState::setPortScanTo(int v) { if (m_portScanTo != v) { m_portScanTo = v; emit portScanConfigChanged(); } }

// ── Group labels ───────────────────────────────────────────────────────────
QStringList AppState::groupLabels() const {
    return { QStringLiteral("System & Adapters"),
             QStringLiteral("Connectivity & Security"),
             QStringLiteral("Internet & DNS"),
             QStringLiteral("Remote Host"),
             QStringLiteral("Website / URL") };
}

// ── Test enable/disable ────────────────────────────────────────────────────
static bool isValidTestId(int id) { return id >= 0 && id < 38; }
static bool isValidGroup(int g) { return g >= 0 && g < 5; }

bool AppState::isTestEnabled(int testIdInt) const {
    if (!isValidTestId(testIdInt)) return false;
    return m_enabledTests.contains(static_cast<TestId>(testIdInt));
}

void AppState::setTestEnabled(int testIdInt, bool enabled) {
    if (!isValidTestId(testIdInt)) return;
    auto id = static_cast<TestId>(testIdInt);
    if (enabled) m_enabledTests.insert(id);
    else m_enabledTests.remove(id);
}

void AppState::setGroupEnabled(int groupInt, bool enabled) {
    if (!isValidGroup(groupInt)) return;
    auto g = static_cast<TestGroup>(groupInt);
    for (auto id : testIdsForGroup(g)) {
        if (enabled) m_enabledTests.insert(id);
        else m_enabledTests.remove(id);
    }
}

bool AppState::isGroupAllEnabled(int groupInt) const {
    if (!isValidGroup(groupInt)) return false;
    auto g = static_cast<TestGroup>(groupInt);
    for (auto id : testIdsForGroup(g)) {
        if (!m_enabledTests.contains(id)) return false;
    }
    return true;
}

bool AppState::isGroupAnyEnabled(int groupInt) const {
    auto g = static_cast<TestGroup>(groupInt);
    for (auto id : testIdsForGroup(g)) {
        if (m_enabledTests.contains(id)) return true;
    }
    return false;
}

// ── Run diagnostics ────────────────────────────────────────────────────────
void AppState::runDiagnostics() {
    if (m_runStatus == RunStatus::Running) return;
    fprintf(stderr, "[TRACE] runDiagnostics start target='%s'\n", m_target.toUtf8().constData());

    // Flutter behaviour: G1-G3 always run (local-only); G4 requires target; G5 requires URL.
    // The group-level filter below (hasTarget/isUrl) handles G4/G5 exclusion automatically.
    // No blanket error on empty target — only block if NO groups would run at all.
    m_errorMessage.clear();

    // Pre-flight: check if any tests are enabled
    bool hasTarget = !isTargetEmpty();
    bool isUrl = isTargetUrl();
    bool anyEnabled = false;
    for (int g = 0; g < 5; ++g) {
        auto group = static_cast<TestGroup>(g);
        for (auto id : testIdsForGroup(group)) {
            if (!m_enabledTests.contains(id)) continue;
            if (group == TestGroup::G4 && !hasTarget) continue;
            if (group == TestGroup::G5 && !isUrl) continue;
            anyEnabled = true;
            break;
        }
        if (anyEnabled) break;
    }
    if (!anyEnabled) {
        m_errorMessage = hasTarget
            ? QStringLiteral("No diagnostic tests are enabled. Check Config.")
            : QStringLiteral("No target specified and no local tests enabled. Enter a target or enable tests in Config.");
        m_runStatus = RunStatus::Error;
        emit runStatusChanged();
        fprintf(stderr, "[TRACE] runDiagnostics blocked: no enabled tests\n");
        return;
    }

    m_runStatus = RunStatus::Running;
    fprintf(stderr, "[TRACE] status=Running, building pending tests\n");
    m_totalCompleted = 0;
    m_totalTests = 0;
    m_results.clear();
    m_completedPerGroup.clear();
    m_totalPerGroup.clear();
    m_currentTestName.clear();
    m_currentGroup.clear();

    // Build groups: group tests by TestGroup (G1→G5 order)
    m_pendingGroups.clear();
    fprintf(stderr, "[TRACE] runDiagnostics: enabledTests=%d hasTarget=%d isUrl=%d\n",
            (int)m_enabledTests.size(), hasTarget, isUrl);
    // Per-group enabled counts (verify checkbox state)
    for (int g = 0; g < 5; ++g) {
        int enabledInGroup = 0;
        int totalInGroup = 0;
        auto group = static_cast<TestGroup>(g);
        for (auto id : testIdsForGroup(group)) {
            totalInGroup++;
            if (m_enabledTests.contains(id)) enabledInGroup++;
        }
        fprintf(stderr, "[TRACE]   G%d: %d/%d enabled\n", g+1, enabledInGroup, totalInGroup);
    }
    for (int g = 0; g < 5; ++g) {
        GroupTask gt;
        gt.group = static_cast<TestGroup>(g);
        for (auto id : testIdsForGroup(gt.group)) {
            if (!m_enabledTests.contains(id)) continue;
            if (gt.group == TestGroup::G4 && !hasTarget) continue;
            if (gt.group == TestGroup::G5 && !isUrl) continue;
            gt.testIds.append(id);
            m_totalPerGroup[gt.group]++;
        }
        if (!gt.testIds.isEmpty()) {
            // Limit per group via env var
            QByteArray maxEnv = qgetenv("ND_MAX_TESTS");
            if (!maxEnv.isEmpty()) {
                int max = maxEnv.toInt();
                if (max > 0 && gt.testIds.size() > max) gt.testIds = gt.testIds.mid(0, max);
            }
            m_pendingGroups.append(gt);
            m_totalTests += gt.testIds.size();
        }
    }
    fprintf(stderr, "[TRACE] %d groups, %d total tests\n", (int)m_pendingGroups.size(), m_totalTests);

    emit runStatusChanged();
    emit progressChanged();
    emit resultsReset();

    Logger::instance().event(QStringLiteral("Starting diagnostic run: %1 tests in %2 groups")
                              .arg(m_totalTests).arg(m_pendingGroups.size()));

    startNextGroup();
}

void AppState::startNextGroup() {
    if (m_runStatus != RunStatus::Running) return;
    if (m_currentGroupIdx >= m_pendingGroups.size()) {
        fprintf(stderr, "[TRACE] All groups complete. Setting runStatus=Completed.\n");
        m_runStatus = RunStatus::Completed;
        m_currentTestName.clear();
        m_currentGroup.clear();
        emit runStatusChanged();
        emit progressChanged();
        Logger::instance().event(QStringLiteral("Diagnostic run complete"));
        return;
    }

    auto& gt = m_pendingGroups[m_currentGroupIdx];
    m_currentGroup = testGroupLabel(gt.group);
    m_activeGroupDone.store(0);
    fprintf(stderr, "[TRACE] startGroup %s (%d tests)\n", m_currentGroup.toUtf8().constData(), (int)gt.testIds.size());

    for (int i = 0; i < gt.testIds.size(); ++i) {
        runTestInGroup(m_currentGroupIdx, i);
    }
}

void AppState::runTestInGroup(int groupIdx, int testIdx) {
    if (m_runStatus != RunStatus::Running) return;
    if (groupIdx >= m_pendingGroups.size()) return;
    auto& gt = m_pendingGroups[groupIdx];
    if (testIdx >= gt.testIds.size()) return;

    TestId id = gt.testIds[testIdx];
    m_currentTestName = staticTestDisplayName(id);
    emit currentTestChanged();
    emit groupChanged();

    fprintf(stderr, "[TRACE] runTest id=%d name='%s' group=%d\n", (int)id, m_currentTestName.toUtf8().constData(), groupIdx);

    // Run test; post result back to main thread via QTimer
     std::thread t([this, id, groupIdx]() {
            try {
                auto start = std::chrono::steady_clock::now();
                auto cmd = createPlatformCommand();
                DiagnosticEngine localEngine(std::move(cmd), nullptr);
                DiagnosticResult result = localEngine.runTest(id, m_target, m_portScanFrom, m_portScanTo, m_portScanCommon).result();
                auto end = std::chrono::steady_clock::now();
                result.durationMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                QTimer::singleShot(0, this, [this, id, result, groupIdx]() {
                onTestFinished(id, result);
                int done = m_activeGroupDone.fetch_add(1) + 1;
                auto& gt = m_pendingGroups[groupIdx];
                if (done >= gt.testIds.size()) {
                    m_currentGroupIdx++;
                    QTimer::singleShot(0, this, &AppState::startNextGroup);
                }
            });
        } catch (...) {
            QTimer::singleShot(0, this, [this, id, groupIdx]() {
                onTestFinished(id, DiagnosticResult::error(id, QStringLiteral("Internal error")));
                int done = m_activeGroupDone.fetch_add(1) + 1;
                auto& gt = m_pendingGroups[groupIdx];
                if (done >= gt.testIds.size()) {
                    m_currentGroupIdx++;
                    QTimer::singleShot(0, this, &AppState::startNextGroup);
                }
            });
        }
    });
    t.detach();
}

void AppState::onTestFinished(TestId id, DiagnosticResult result) {
    fprintf(stderr, "[TRACE] onTestFinished id=%d status=%d\n", (int)id, (int)result.status);
    // Suppress stale results after cancel/reset
    if (m_runStatus != RunStatus::Running) return;
    TestGroup g = testGroup(id);
    m_results[id] = result;
    m_totalCompleted++;
    m_completedPerGroup[g]++;
    m_resultsVersion++;

    emit progressChanged();
    emit testCompleted(static_cast<int>(id));
}

void AppState::cancel() {
    if (m_runStatus != RunStatus::Running) return;
    m_runStatus = RunStatus::Cancelled;
    m_currentTestName.clear();
    emit runStatusChanged();
    emit progressChanged();
    Logger::instance().event("Diagnostic run cancelled");
}

void AppState::reset() {
    cancel();
    m_runStatus = RunStatus::Idle;
    m_totalCompleted = 0; m_totalTests = 0;
    m_results.clear();
    m_completedPerGroup.clear(); m_totalPerGroup.clear();
    m_errorMessage.clear();
    m_pendingGroups.clear();
    m_currentGroupIdx = 0;
    m_activeGroupDone.store(0);
    m_resultsVersion = 0;
    emit runStatusChanged();
    emit progressChanged();
    emit resultsReset();
}

// ── Results for QML ────────────────────────────────────────────────────────
QVariantList AppState::resultsForGroup(int groupInt) const {
    QVariantList list;
    auto g = static_cast<TestGroup>(groupInt);
    for (auto id : testIdsForGroup(g)) {
        if (m_results.contains(id)) {
            const auto& r = m_results[id];
            QVariantMap m;
            m["id"] = static_cast<int>(r.id);
            m["testId"] = static_cast<int>(r.id);
            m["displayName"] = r.displayName.isEmpty() ? staticTestDisplayName(r.id) : r.displayName;
            m["status"] = static_cast<int>(r.status);
            m["statusIcon"] = r.statusIcon();
            m["summary"] = r.summary;
            m["details"] = r.details;
            m["durationMs"] = r.durationMs;
            m["isDone"] = true;
            m["isPending"] = false;
            m["isRunning"] = false;
            list.append(m);
        }
    }
    return list;
}

QVariantList AppState::allTestIdsForGroup(int groupInt) const {
    QVariantList list;
    if (!isValidGroup(groupInt)) return list;
    auto g = static_cast<TestGroup>(groupInt);
    for (auto id : testIdsForGroup(g)) {
        list.append(static_cast<int>(id));
    }
    return list;
}

QVariantList AppState::allTestsForGroup(int groupInt) const {
    QVariantList list;
    if (!isValidGroup(groupInt)) return list;
    auto g = static_cast<TestGroup>(groupInt);
    
    for (auto id : testIdsForGroup(g)) {
        if (!m_enabledTests.contains(id)) continue;
        
        if (m_results.contains(id)) {
            // Completed test
            const auto& r = m_results[id];
            QVariantMap m;
            m["id"] = static_cast<int>(r.id);
            m["testId"] = static_cast<int>(r.id);  // alias for QML access
            m["displayName"] = r.displayName.isEmpty() ? staticTestDisplayName(r.id) : r.displayName;
            m["status"] = static_cast<int>(r.status);
            m["statusIcon"] = r.statusIcon();
            m["summary"] = r.summary;
            m["details"] = r.details;
            m["durationMs"] = r.durationMs;
            QVariantList props;
            for (auto& p : r.properties) {
                QVariantMap pm;
                pm["label"] = p.label;
                pm["value"] = p.value;
                props.append(pm);
            }
            m["properties"] = props;
            m["isDone"] = true;
            m["isPending"] = false;
            m["isRunning"] = false;
            list.append(m);
        } else {
            // Pending test
            bool isCurrent = (m_runStatus == RunStatus::Running) 
                          && (m_currentTestName == staticTestDisplayName(id));
            QVariantMap m;
            m["id"] = static_cast<int>(id);
            m["testId"] = static_cast<int>(id);  // alias for QML access (consistent with completed)
            m["displayName"] = staticTestDisplayName(id);
            m["status"] = -1;
            m["statusIcon"] = QStringLiteral("⊖");
            m["summary"] = QString(isCurrent ? "Running..." : "");
            m["details"] = QString();
            m["durationMs"] = 0;
            m["isDone"] = false;
            m["isPending"] = true;
            m["isRunning"] = isCurrent;
            list.append(m);
        }
    }
    return list;
}

QVariantMap AppState::groupStats(int groupInt) const {
    QVariantMap stats;
    auto g = static_cast<TestGroup>(groupInt);
    int pass = 0, warn = 0, fail = 0, skip = 0, total = 0;
    for (auto id : testIdsForGroup(g)) {
        if (!m_results.contains(id)) continue;
        total++;
        switch (m_results[id].status) {
            case TestStatus::Pass: pass++; break;
            case TestStatus::Warning: warn++; break;
            case TestStatus::Fail: fail++; break;
            case TestStatus::Skipped: skip++; break;
            default: break;
        }
    }
    stats["pass"] = pass; stats["warn"] = warn;
    stats["fail"] = fail; stats["skip"] = skip; stats["total"] = total;
    stats["enabled"] = m_totalPerGroup.value(g, 0);
    return stats;
}

QString AppState::currentTestLabel() const {
    if (m_runStatus == RunStatus::Running)
        return QStringLiteral("%1: %2").arg(m_currentGroup, m_currentTestName);
    return {};
}

QString AppState::testDisplayName(int testIdInt) const {
    return staticTestDisplayName(static_cast<TestId>(testIdInt));
}

QString AppState::staticTestDisplayName(TestId id) {
    switch (id) {
        case TestId::G1NetworkAdapters: return QStringLiteral("Network Adapters");
        case TestId::G1NicAdvanced: return QStringLiteral("NIC Advanced");
        case TestId::G1WifiDiagnostics: return QStringLiteral("WiFi");
        case TestId::G1WiredDiagnostics: return QStringLiteral("Wired");
        case TestId::G1DhcpStatus: return QStringLiteral("DHCP Status");
        case TestId::G1IpConfiguration: return QStringLiteral("IP Config");
        case TestId::G1ActiveConnections: return QStringLiteral("Active Connections");
        case TestId::G2NetworkProfile: return QStringLiteral("Network Profile");
        case TestId::G2TcpSettings: return QStringLiteral("TCP Settings");
        case TestId::G2DefaultGateway: return QStringLiteral("Default Gateway");
        case TestId::G2RoutingTable: return QStringLiteral("Routing Table");
        case TestId::G2ArpTable: return QStringLiteral("ARP Table");
        case TestId::G2ProxySettings: return QStringLiteral("Proxy Settings");
        case TestId::G3NetskopeStatus: return QStringLiteral("Netskope Status");
        case TestId::G3DnsServers: return QStringLiteral("DNS Servers");
        case TestId::G3DnsCache: return QStringLiteral("DNS Cache");
        case TestId::G3DnsPollution: return QStringLiteral("DNS Pollution");
        case TestId::G3InternetConnectivity: return QStringLiteral("Internet");
        case TestId::G3InternetSpeedTest: return QStringLiteral("Speed Test");
        case TestId::G4DnsResolution: return QStringLiteral("DNS Resolution");
        case TestId::G4Ping: return QStringLiteral("Ping");
        case TestId::G4Traceroute: return QStringLiteral("Traceroute");
        case TestId::G4PathPing: return QStringLiteral("PathPing");
        case TestId::G4MtuDiscovery: return QStringLiteral("MTU Discovery");
        case TestId::G4PortScan: return QStringLiteral("Port Scan");
        case TestId::G5UrlParsing: return QStringLiteral("URL Parsing");
        case TestId::G5TcpConnect: return QStringLiteral("TCP Connect");
        case TestId::G5ServiceBanner: return QStringLiteral("Service Banner");
        case TestId::G5CurlVerbose: return QStringLiteral("HTTP Request");
        case TestId::G5HttpHeaders: return QStringLiteral("HTTP Headers");
        case TestId::G5SecurityHeaders: return QStringLiteral("Security Headers");
        case TestId::G5SslCertificate: return QStringLiteral("SSL Certificate");
        case TestId::G5HttpRedirect: return QStringLiteral("HTTP Redirect");
        case TestId::G5HttpCompression: return QStringLiteral("HTTP Compression");
        case TestId::G5HttpTiming: return QStringLiteral("HTTP Timing");
        case TestId::G5FtpDiagnostics: return QStringLiteral("FTP");
        case TestId::G5SshDiagnostics: return QStringLiteral("SSH");
        case TestId::G5EmailDiagnostics: return QStringLiteral("Email");
    }
    return QStringLiteral("Unknown");
}

QVariantList AppState::allGroupStats() const {
    QVariantList list;
    for (int g = 0; g < 5; ++g) list.append(groupStats(g));
    return list;
}

void AppState::showDetailDialog(int testIdInt) {
    if (!isValidTestId(testIdInt)) return;
    auto id = static_cast<TestId>(testIdInt);
    if (!m_results.contains(id)) return;
    
    const auto& r = m_results[id];
    
    // Use heap-allocated dialog with show() instead of exec()
    // exec() creates a nested event loop that crashes QML on ARM64
    auto* dlg = new QDialog(nullptr, Qt::Dialog | Qt::WindowCloseButtonHint);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(r.displayName);
    dlg->setMinimumSize(450, 350);
    dlg->setModal(true);
    dlg->setStyleSheet(QStringLiteral(
        "QDialog { background-color: #1E1E2E; }"
        "QLabel { color: #E0E0E0; font-family: 'JetBrains Mono'; }"
        "QPushButton { color: #E0E0E0; background-color: #252538; border: 1px solid #3A3A5A; padding: 6px 16px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #0078D4; }"
    ));
    
    auto* layout = new QVBoxLayout(dlg);
    layout->setSpacing(8);
    
    // Status line
    QStringList statusNames = {"Pass", "Warning", "Fail", "Skipped", "Error", "Info"};
    auto* statusLabel = new QLabel(QStringLiteral("Status: %1    Duration: %2ms")
        .arg(statusNames.value(static_cast<int>(r.status), "Unknown"))
        .arg(r.durationMs));
    statusLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #A0A0B8;");
    layout->addWidget(statusLabel);
    
    // Summary
    if (!r.summary.isEmpty()) {
        auto* sumLabel = new QLabel(QStringLiteral("Summary: %1").arg(r.summary));
        sumLabel->setStyleSheet("font-size: 12px; color: #E0E0E0;");
        sumLabel->setWordWrap(true);
        layout->addWidget(sumLabel);
    }
    
    // Properties
    if (!r.properties.isEmpty()) {
        auto* propHeader = new QLabel("Properties:");
        propHeader->setStyleSheet("font-size: 11px; font-weight: bold; color: #A0A0B8; margin-top: 8px;");
        layout->addWidget(propHeader);
        
        for (const auto& p : r.properties) {
            auto* propLabel = new QLabel(QStringLiteral("  %1: %2").arg(p.label, p.value));
            propLabel->setStyleSheet("font-size: 11px; color: #E0E0E0;");
            propLabel->setWordWrap(true);
            layout->addWidget(propLabel);
        }
    }
    
    // Raw output (scrollable)
    if (!r.details.isEmpty()) {
        auto* outHeader = new QLabel("Output:");
        outHeader->setStyleSheet("font-size: 11px; font-weight: bold; color: #A0A0B8; margin-top: 8px;");
        layout->addWidget(outHeader);
        
        auto* scrollArea = new QScrollArea();
        scrollArea->setStyleSheet("QScrollArea { background-color: #252538; border: none; border-radius: 4px; }");
        scrollArea->setMaximumHeight(200);
        
        auto* detailText = new QLabel(r.details);
        detailText->setStyleSheet("font-size: 10px; color: #A0A0B8; padding: 8px;");
        detailText->setWordWrap(true);
        scrollArea->setWidget(detailText);
        layout->addWidget(scrollArea);
    }
    
    // Close button
    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Close);
    QObject::connect(btnBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    layout->addWidget(btnBox);
    
    dlg->show();
}

QVariantMap AppState::getDetailResult(int testIdInt) const {
    QVariantMap m;
    if (!isValidTestId(testIdInt)) return m;
    auto id = static_cast<TestId>(testIdInt);
    if (!m_results.contains(id)) return m;
    
    const auto& r = m_results[id];
    m["displayName"] = r.displayName;
    m["status"] = static_cast<int>(r.status);
    m["summary"] = r.summary;
    m["details"] = r.details;
    m["durationMs"] = r.durationMs;
    
    QVariantList props;
    for (const auto& p : r.properties) {
        QVariantMap pm;
        pm["label"] = p.label;
        pm["value"] = p.value;
        props.append(pm);
    }
    m["properties"] = props;
    return m;
}
