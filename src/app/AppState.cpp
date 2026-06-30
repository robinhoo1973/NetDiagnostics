// =============================================================================
// AppState.cpp
// =============================================================================
#include "app/AppState.h"
#include "util/DebugSwitch.h"
#include "engine/diagnostic/DiagnosticEngine.h"
#include "util/DebugSwitch.h"
#include "util/Logger.h"
#include <cstdio>
#include <chrono>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
#include <QDialog>
#endif
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
#include <QVBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QScrollArea>
#endif

AppState::AppState(QObject* parent) : QObject(parent) {
    // Enable G1-G3 by default; G4/G5 are auto-managed based on target
    for (auto id : allDiagIds()) {
        auto g = diagGroup(id);
        if (g == DiagGroup::G1 || g == DiagGroup::G2 || g == DiagGroup::G3)
            m_enabledDiags.insert(id);
    }

#ifdef PLATFORM_IOS
    // iOS sandbox: these tests can never return useful data
    m_enabledDiags.remove(DiagId::G1DhcpStatus);
    m_enabledDiags.remove(DiagId::G2TcpSettings);
    m_enabledDiags.remove(DiagId::G2DefaultGateway);
    m_enabledDiags.remove(DiagId::G2RoutingTable);
    m_enabledDiags.remove(DiagId::G2ArpTable);
#endif

    m_engine = new DiagnosticEngine(this);

    QObject::connect(m_engine, &DiagnosticEngine::destroyed, this, [this]() { m_engine = nullptr; });
}

AppState::~AppState() {
    if (m_runStatus == RunStatus::Running) {
        m_runStatus = RunStatus::Cancelled;
    }
}

// ── State version — called at end of every mutation method ──────────────
void AppState::bumpVersion() {
    m_stateGeneration.fetch_add(1, std::memory_order_release);
    emit stateVersionChanged();
}

// ── Language switching ──────────────────────────────────────────────────
// 0=EN,1=FR,2=DE,3=RU,4=IT,5=ZH_CN,6=ZH_TW
void AppState::setLanguage(int index) {
    if (index < 0 || index > 6) return;
    m_languageIndex = index;
    emit languageChanged();
    bumpVersion();
    TRACE(" Language set to index %d\n", index);
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
    auto schemeEnd = trimmed.indexOf("://");
    if (schemeEnd < 0) return QString();  // no scheme → not a URL

    QString scheme = trimmed.left(schemeEnd).toLower();
    if (scheme.isEmpty()) return QStringLiteral("Empty URL scheme");
    if (!supportedSchemes().contains(scheme))
        return QStringLiteral("Unsupported protocol: %1:// — supported schemes: %2")
            .arg(scheme, supportedSchemes().join(", "));

    // 2. Parse authority (host[:port])
    QString afterScheme = trimmed.mid(schemeEnd + 3);
    auto pathStart = afterScheme.indexOf('/');
    auto queryStart = afterScheme.indexOf('?');
    auto fragStart = afterScheme.indexOf('#');

    auto authorityEnd = afterScheme.size();
    if (pathStart >= 0) authorityEnd = std::min(authorityEnd, pathStart);
    if (queryStart >= 0) authorityEnd = std::min(authorityEnd, queryStart);
    if (fragStart >= 0) authorityEnd = std::min(authorityEnd, fragStart);

    QString authority = afterScheme.left(authorityEnd);
    if (authority.isEmpty()) return QStringLiteral("URL has no hostname");

    // 3. Separate host and port
    QString host, portStr;
    auto portColon = authority.lastIndexOf(':');
    // Check for IPv6 bracket notation [::1]:port
    if (authority.startsWith('[')) {
        auto closing = authority.indexOf(']');
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
        TRACE(" setTarget('%s')\n", m_target.toUtf8().constData());

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
        TRACE(" setTarget scheme='%s' empty=%d hasScheme=%d isTargetUrl=%d isTargetHttpUrl=%d validateErr='%s'\n",
                scheme.toUtf8().constData(), isTargetEmpty(), trimmed.contains("://"),
                isTargetUrl(), isTargetHttpUrl(), m_targetError.toUtf8().constData());

        // G4: always on when target non-empty (URL or host)
        // G5: only http/https — excluded entirely on iOS (no curl/libcurl)
        setGroupEnabled(3, has);          // G4 on if target non-empty
	#ifndef NO_CURL
        setGroupEnabled(4, has && isHttp); // G5 on only for http/https
	#endif
        TRACE(" setTarget result: has=%d isUrl=%d isHttp=%d G4=%d G5=%d err='%s'\n",
                has, isUrl, isHttp, has, has && isHttp, m_targetError.toUtf8().constData());
        emit targetChanged();
        bumpVersion();
    }
}

// ── Port scan config ───────────────────────────────────────────────────────
void AppState::setPortScanCommon(bool v) { if (m_portScanCommon != v) { m_portScanCommon = v; emit portScanConfigChanged(); bumpVersion(); } }
void AppState::setPortScanFrom(int v) { if (m_portScanFrom != v) { m_portScanFrom = v; emit portScanConfigChanged(); bumpVersion(); } }
void AppState::setPortScanTo(int v) { if (m_portScanTo != v) { m_portScanTo = v; emit portScanConfigChanged(); bumpVersion(); } }

// ── Group labels ───────────────────────────────────────────────────────────
QStringList AppState::groupLabels() const {
    return { QStringLiteral("System & Adapters"),
             QStringLiteral("Connectivity & Security"),
             QStringLiteral("Internet & DNS"),
             QStringLiteral("Remote Host"),
             QStringLiteral("Website / URL") };
}

// ── Test enable/disable ────────────────────────────────────────────────────
static bool isValidDiagId(int id) { return id >= 0 && id < 39; }
static bool isValidGroup(int g) { return g >= 0 && g < 5; }

bool AppState::isDiagEnabled(int diagIdInt) const {
    if (!isValidDiagId(diagIdInt)) return false;
    return m_enabledDiags.contains(static_cast<DiagId>(diagIdInt));
}

void AppState::setDiagEnabled(int diagIdInt, bool enabled) {
    if (!isValidDiagId(diagIdInt)) return;
    auto id = static_cast<DiagId>(diagIdInt);
    if (enabled) m_enabledDiags.insert(id);
    else m_enabledDiags.remove(id);
    bumpVersion();
}

void AppState::setGroupEnabled(int groupInt, bool enabled) {
    if (!isValidGroup(groupInt)) return;
    auto g = static_cast<DiagGroup>(groupInt);
    TRACE(" setGroupEnabled G%d = %d (before: %d tests in set)\n",
            groupInt+1, enabled, (int)m_enabledDiags.size());
    for (auto id : diagIdsForGroup(g)) {
        if (enabled) m_enabledDiags.insert(id);
        else m_enabledDiags.remove(id);
    }
    TRACE(" setGroupEnabled G%d = %d (after: %d tests in set)\n",
            groupInt+1, enabled, (int)m_enabledDiags.size());
    bumpVersion();
}

bool AppState::isGroupAllEnabled(int groupInt) const {
    if (!isValidGroup(groupInt)) return false;
    auto g = static_cast<DiagGroup>(groupInt);
    for (auto id : diagIdsForGroup(g)) {
        if (!m_enabledDiags.contains(id)) return false;
    }
    return true;
}

bool AppState::isGroupAnyEnabled(int groupInt) const {
    auto g = static_cast<DiagGroup>(groupInt);
    for (auto id : diagIdsForGroup(g)) {
        if (m_enabledDiags.contains(id)) return true;
    }
    return false;
}

// ── Run diagnostics ────────────────────────────────────────────────────────
void AppState::runDiagnostics() {
    // Force-reset if stuck from a previous run
    if (m_runStatus == RunStatus::Running) {
        Logger::instance().event("Force-resetting stuck run");
        cancel();
        m_runStatus = RunStatus::Idle;
        m_runGeneration.fetch_add(1, std::memory_order_release);
    }
    TRACE(" runDiagnostics start target='%s'\n", m_target.toUtf8().constData());

    // Reset state before each run (clears previous results, error messages, etc.)
    reset();

    // Flutter behaviour: G1-G3 always run (local-only); G4 requires target; G5 requires URL.
    // The group-level filter below (hasTarget/isUrl) handles G4/G5 exclusion automatically.
    // No blanket error on empty target — only block if NO groups would run at all.

    bool hasTarget = !isTargetEmpty();
    m_runStatus = RunStatus::Running;
    m_runGeneration.fetch_add(1, std::memory_order_release); // invalidate stale callbacks
    TRACE(" status=Running generation=%d, building pending tests\n", (int)m_runGeneration.load());
    m_totalCompleted = 0;
    m_totalDiags = 0;
    m_results.clear();
    m_completedPerGroup.clear();
    m_totalPerGroup.clear();
    m_currentDiagName.clear();
    m_currentGroup.clear();

    // Build groups: group tests by DiagGroup (G1→G5 order)
    m_pendingGroups.clear();
    TRACE(" runDiagnostics: enabledTests=%d hasTarget=%d\n",
            (int)m_enabledDiags.size(), hasTarget);
    // Per-group enabled counts (verify checkbox state)
    for (int g = 0; g < 5; ++g) {
        int enabledInGroup = 0;
        int totalInGroup = 0;
        auto group = static_cast<DiagGroup>(g);
        for (auto id : diagIdsForGroup(group)) {
            totalInGroup++;
            if (m_enabledDiags.contains(id)) enabledInGroup++;
        }
        TRACE("   G%d: %d/%d enabled\n", g+1, enabledInGroup, totalInGroup);
    }
    for (int g = 0; g < 5; ++g) {
        GroupTask gt;
        gt.group = static_cast<DiagGroup>(g);
        for (auto id : diagIdsForGroup(gt.group)) {
            if (!m_enabledDiags.contains(id)) continue;
            if (gt.group == DiagGroup::G4 && !hasTarget) continue;
            if (gt.group == DiagGroup::G5 && !hasTarget) continue;
            gt.diagIds.append(id);
            m_totalPerGroup[gt.group]++;
        }
        if (!gt.diagIds.isEmpty()) {
            // Limit per group via env var
            QByteArray maxEnv = qgetenv("ND_MAX_TESTS");
            if (!maxEnv.isEmpty()) {
                int max = maxEnv.toInt();
                if (max > 0 && gt.diagIds.size() > max) gt.diagIds = gt.diagIds.mid(0, max);
            }
            m_pendingGroups.append(gt);
            m_totalDiags += gt.diagIds.size();
        }
    }
    TRACE(" %d groups, %d total tests\n", (int)m_pendingGroups.size(), m_totalDiags);

    if (m_pendingGroups.isEmpty()) {
        m_errorMessage = hasTarget
            ? QStringLiteral("No diagnostic tests are enabled. Check Config.")
            : QStringLiteral("No target specified and no local tests enabled. Enter a target or enable tests in Config.");
        m_runStatus = RunStatus::Error;
        TRACE(" runDiagnostics blocked: no enabled tests\n");
        emit runStatusChanged();
        return;
    }

    emit runStatusChanged();
    emit progressChanged();
    emit resultsReset();
    bumpVersion();

    Logger::instance().event(QStringLiteral("Starting diagnostic run: %1 tests in %2 groups")
                              .arg(m_totalDiags).arg(m_pendingGroups.size()));

    startNextGroup();
}

void AppState::startNextGroup() {
    if (m_runStatus != RunStatus::Running) return;
    if (m_currentGroupIdx >= m_pendingGroups.size()) {
        TRACE(" All groups complete. Setting runStatus=Completed.\n");
        m_runStatus = RunStatus::Completed;
        m_currentDiagName.clear();
        m_currentGroup.clear();
        emit runStatusChanged();
        emit progressChanged();
        bumpVersion();
        Logger::instance().event(QStringLiteral("Diagnostic run complete"));
        return;
    }

    auto& gt = m_pendingGroups[m_currentGroupIdx];
    m_currentGroup = diagGroupLabel(gt.group);
    m_activeGroupDone.store(0);
    bumpVersion();
    TRACE(" startGroup %s (%d tests)\n", m_currentGroup.toUtf8().constData(), (int)gt.diagIds.size());

    for (int i = 0; i < gt.diagIds.size(); ++i) {
        runDiagInGroup(m_currentGroupIdx, i);
    }
}

void AppState::runDiagInGroup(int groupIdx, int diagIdx) {
    if (m_runStatus != RunStatus::Running) return;
    if (groupIdx >= m_pendingGroups.size()) return;
    auto& gt = m_pendingGroups[groupIdx];
    if (diagIdx >= gt.diagIds.size()) return;

    DiagId id = gt.diagIds[diagIdx];
    m_currentDiagName = staticDiagDisplayName(id);
    emit currentDiagChanged();
    emit groupChanged();
    bumpVersion();

    TRACE(" runDiag id=%d name='%s' group=%d\n", (int)id, m_currentDiagName.toUtf8().constData(), groupIdx);

    // Snapshot shared state before launching worker
    QString target = m_target;
    int psFrom = m_portScanFrom;
    int psTo = m_portScanTo;
    bool psCommon = m_portScanCommon;
    int runGen = m_runGeneration.load(std::memory_order_acquire);

    // Use QThreadPool instead of detached std::thread — threads are reused
    // across test runs, avoiding repeated creation/destruction overhead.
    auto* worker = new QObject; // temporary owner for the QRunnable
    struct Task : public QRunnable {
        QObject* owner;
        AppState* state;
        DiagId id;
        int groupIdx, runGen;
        QString target;
        int psFrom, psTo;
        bool psCommon;
        Task(QObject* o, AppState* s, DiagId i, int gi, int rg, QString t, int pf, int pt, bool pc)
            : owner(o), state(s), id(i), groupIdx(gi), runGen(rg), target(t), psFrom(pf), psTo(pt), psCommon(pc)
        { setAutoDelete(false); }
        void run() override {
            try {
                auto start = std::chrono::steady_clock::now();
                DiagnosticEngine localEngine(nullptr);
                DiagnosticResult result = localEngine.runDiagSync(id, target, psFrom, psTo, psCommon);
                auto end = std::chrono::steady_clock::now();
                result.durationMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                QTimer::singleShot(0, state, [this, result]() {
                    if (state->m_runGeneration.load(std::memory_order_acquire) != runGen) { delete owner; delete this; return; }
                    state->onDiagFinished(id, result);
                    int done = state->m_activeGroupDone.fetch_add(1) + 1;
                    auto& gt = state->m_pendingGroups[groupIdx];
                    if (done >= gt.diagIds.size()) {
                        state->m_currentGroupIdx++;
                        QTimer::singleShot(0, state, &AppState::startNextGroup);
                    }
                    delete owner;
                    delete this;
                });
            } catch (...) {
                QTimer::singleShot(0, state, [this]() {
                    if (state->m_runGeneration.load(std::memory_order_acquire) != runGen) { delete owner; delete this; return; }
                    state->onDiagFinished(id, DiagnosticResult::error(id, QStringLiteral("Internal error")));
                    int done = state->m_activeGroupDone.fetch_add(1) + 1;
                    auto& gt = state->m_pendingGroups[groupIdx];
                    if (done >= gt.diagIds.size()) {
                        state->m_currentGroupIdx++;
                        QTimer::singleShot(0, state, &AppState::startNextGroup);
                    }
                    delete owner;
                    delete this;
                });
            }
        }
    };
    QThreadPool::globalInstance()->start(new Task(worker, this, id, groupIdx, runGen, target, psFrom, psTo, psCommon));
}

void AppState::onDiagFinished(DiagId id, DiagnosticResult result) {
    TRACE(" onDiagFinished id=%d status=%d\n", (int)id, (int)result.status);
    // Suppress stale results after cancel/reset
    if (m_runStatus != RunStatus::Running) return;
    DiagGroup g = diagGroup(id);
    m_results[id] = result;
    m_totalCompleted++;
    m_completedPerGroup[g]++;
    m_resultsVersion++;

    emit progressChanged();
    emit diagCompleted(static_cast<int>(id));
    bumpVersion();
}

void AppState::cancel() {
    if (m_runStatus != RunStatus::Running) return;
    m_runStatus = RunStatus::Cancelled;
    m_currentDiagName.clear();
    emit runStatusChanged();
    emit progressChanged();
    bumpVersion();
    Logger::instance().event("Diagnostic run cancelled");
}

void AppState::reset() {
    cancel();
    m_runStatus = RunStatus::Idle;
    m_totalCompleted = 0; m_totalDiags = 0;
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
    bumpVersion();
}

// ── Results for QML ────────────────────────────────────────────────────────
QVariantList AppState::resultsForGroup(int groupInt) const {
    QVariantList list;
    auto g = static_cast<DiagGroup>(groupInt);
    for (auto id : diagIdsForGroup(g)) {
        if (m_results.contains(id)) {
            const auto& r = m_results[id];
            QVariantMap m;
            m["id"] = static_cast<int>(r.id);
            m["diagId"] = static_cast<int>(r.id);
            m["displayName"] = r.displayName.isEmpty() ? staticDiagDisplayName(r.id) : r.displayName;
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

QVariantList AppState::allDiagIdsForGroup(int groupInt) const {
    QVariantList list;
    if (!isValidGroup(groupInt)) return list;
    auto g = static_cast<DiagGroup>(groupInt);
    for (auto id : diagIdsForGroup(g)) {
        list.append(static_cast<int>(id));
    }
    return list;
}

QVariantList AppState::visibleGroups() const {
    QVariantList list;
    for (int i = 0; i < 5; ++i) {
        QVariantMap s = groupStats(i);
        if (s["enabled"].toInt() > 0 || s["total"].toInt() > 0)
            list.append(i);
    }
    return list;
}

QVariantList AppState::allDiagsForGroup(int groupInt) const {
    QVariantList list;
    if (!isValidGroup(groupInt)) return list;
    auto g = static_cast<DiagGroup>(groupInt);
    
    for (auto id : diagIdsForGroup(g)) {
        if (!m_enabledDiags.contains(id)) continue;
        
        if (m_results.contains(id)) {
            // Completed test
            const auto& r = m_results[id];
            QVariantMap m;
            m["id"] = static_cast<int>(r.id);
            m["diagId"] = static_cast<int>(r.id);  // alias for QML access
            m["displayName"] = r.displayName.isEmpty() ? staticDiagDisplayName(r.id) : r.displayName;
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
            // Pending test — mark as running if its group is currently executing
            bool isRunning = (m_runStatus == RunStatus::Running)
                          && (m_currentGroupIdx < m_pendingGroups.size())
                          && (m_pendingGroups[m_currentGroupIdx].group == g);
            QVariantMap m;
            m["id"] = static_cast<int>(id);
            m["diagId"] = static_cast<int>(id);
            m["displayName"] = staticDiagDisplayName(id);
            m["status"] = -1;
            m["statusIcon"] = QStringLiteral("badge-skip");
            m["summary"] = QString(isRunning ? "Running..." : "");
            m["details"] = QString();
            m["durationMs"] = 0;
            m["isDone"] = false;
            m["isPending"] = true;
            m["isRunning"] = isRunning;
            list.append(m);
        }
    }
    return list;
}

QVariantMap AppState::groupStats(int groupInt) const {
    QVariantMap stats;
    auto g = static_cast<DiagGroup>(groupInt);
    // total = tests actually scheduled for this group (m_totalPerGroup).
    // Before any run this is 0 — all counts show 0.
    int total = m_totalPerGroup.value(g, 0);
    int pass = 0, warn = 0, fail = 0, skip = 0, info = 0, completed = 0;
    for (auto id : diagIdsForGroup(g)) {
        if (!m_results.contains(id)) continue;
        completed++;
        switch (m_results[id].status) {
            case DiagStatus::Pass:    pass++; break;
            case DiagStatus::Warning: warn++; break;
            case DiagStatus::Fail:    fail++; break;
            case DiagStatus::Skipped: skip++; break;
            case DiagStatus::Info:    info++; break;
            default: break;
        }
    }
    stats["pass"] = pass; stats["warn"] = warn;
    stats["fail"] = fail; stats["skip"] = skip; stats["info"] = info;
    stats["completed"] = completed; stats["total"] = total;
    stats["enabled"] = total;
    return stats;
}

QString AppState::currentDiagLabel() const {
    if (m_runStatus == RunStatus::Running)
        return QStringLiteral("%1: %2").arg(m_currentGroup, m_currentDiagName);
    return {};
}

QString AppState::diagDisplayName(int diagIdInt) const {
    return staticDiagDisplayName(static_cast<DiagId>(diagIdInt));
}

QString AppState::staticDiagDisplayName(DiagId id) {
    switch (id) {
        case DiagId::G1NetworkAdapters: return QStringLiteral("Network Adapters");
        case DiagId::G1NicAdvanced: return QStringLiteral("NIC Advanced");
        case DiagId::G1WifiDiagnostics: return QStringLiteral("WiFi Information");
        case DiagId::G1WiredDiagnostics: return QStringLiteral("Wired Information");
        case DiagId::G1DhcpStatus: return QStringLiteral("DHCP Status");
        case DiagId::G1IpConfiguration: return QStringLiteral("IP Configuration");
        case DiagId::G1ActiveConnections: return QStringLiteral("Active Connections");
        case DiagId::G1CellularInfo: return QStringLiteral("Cellular Information");
        case DiagId::G2NetworkProfile: return QStringLiteral("Network Profile");
        case DiagId::G2TcpSettings: return QStringLiteral("TCP Settings");
        case DiagId::G2DefaultGateway: return QStringLiteral("Default Gateway");
        case DiagId::G2RoutingTable: return QStringLiteral("Routing Table");
        case DiagId::G2ArpTable: return QStringLiteral("ARP Table");
        case DiagId::G2ProxySettings: return QStringLiteral("Proxy Settings");
        case DiagId::G3NetskopeStatus: return QStringLiteral("Netskope Status");
        case DiagId::G3DnsServers: return QStringLiteral("DNS Servers");
        case DiagId::G3DnsCache: return QStringLiteral("DNS Cache");
        case DiagId::G3DnsPollution: return QStringLiteral("DNS Pollution");
        case DiagId::G3InternetSpeedTest: return QStringLiteral("Internet Connectivity");
        case DiagId::G4DnsResolution: return QStringLiteral("DNS Resolution");
        case DiagId::G4Ping: return QStringLiteral("Ping");
        case DiagId::G4Traceroute: return QStringLiteral("Traceroute");
        case DiagId::G4PathPing: return QStringLiteral("PathPing");
        case DiagId::G4MtuDiscovery: return QStringLiteral("MTU Discovery");
        case DiagId::G4PortScan: return QStringLiteral("Port Scan");
        case DiagId::G5UrlParsing: return QStringLiteral("URL Parsing");
        case DiagId::G5TcpConnect: return QStringLiteral("TCP Connect");
        case DiagId::G5ServiceBanner: return QStringLiteral("Service Banner");
        case DiagId::G5CurlVerbose: return QStringLiteral("HTTP Request");
        case DiagId::G5HttpHeaders: return QStringLiteral("HTTP Headers");
        case DiagId::G5SecurityHeaders: return QStringLiteral("Security Headers");
        case DiagId::G5SslCertificate: return QStringLiteral("SSL Certificate");
        case DiagId::G5HttpRedirect: return QStringLiteral("HTTP Redirect");
        case DiagId::G5HttpCompression: return QStringLiteral("HTTP Compression");
        case DiagId::G5HttpTiming: return QStringLiteral("HTTP Timing");
        case DiagId::G5FtpDiagnostics: return QStringLiteral("FTP");
        case DiagId::G5SshDiagnostics: return QStringLiteral("SSH");
        case DiagId::G5EmailDiagnostics: return QStringLiteral("Email");
    }
    return QStringLiteral("Unknown");
}

QVariantList AppState::allGroupStats() const {
    if (m_cachedStatsVersion == m_resultsVersion && !m_cachedGroupStats.isEmpty())
        return m_cachedGroupStats;
    m_cachedStatsVersion = m_resultsVersion;
    m_cachedGroupStats.clear();
    for (int g = 0; g < 5; ++g)
        m_cachedGroupStats.append(groupStats(g));
    return m_cachedGroupStats;
}

void AppState::showDetailDialog(int diagIdInt) {
    if (!isValidDiagId(diagIdInt)) return;
    auto id = static_cast<DiagId>(diagIdInt);
    if (!m_results.contains(id)) return;
    
    const auto& r = m_results[id];
    
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
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
    statusLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #A0A0B8; font-family: 'JetBrains Mono';");
    layout->addWidget(statusLabel);
    
    // Summary
    if (!r.summary.isEmpty()) {
        auto* sumLabel = new QLabel(QStringLiteral("Summary: %1").arg(r.summary));
        sumLabel->setStyleSheet("font-size: 12px; color: #E0E0E0; font-family: 'JetBrains Mono';");
        sumLabel->setWordWrap(true);
        layout->addWidget(sumLabel);
    }
    
    // Properties
    if (!r.properties.isEmpty()) {
        auto* propHeader = new QLabel("Properties:");
        propHeader->setStyleSheet("font-size: 11px; font-weight: bold; color: #A0A0B8; margin-top: 8px; font-family: 'JetBrains Mono';");
        layout->addWidget(propHeader);
        
        for (const auto& p : r.properties) {
            auto* propLabel = new QLabel(QStringLiteral("  %1: %2").arg(p.label, p.value));
            propLabel->setStyleSheet("font-size: 11px; color: #E0E0E0; font-family: 'JetBrains Mono';");
            propLabel->setWordWrap(true);
            layout->addWidget(propLabel);
        }
    }
    
    // Raw output (scrollable)
    if (!r.details.isEmpty()) {
        auto* outHeader = new QLabel("Output:");
        outHeader->setStyleSheet("font-size: 11px; font-weight: bold; color: #A0A0B8; margin-top: 8px; font-family: 'JetBrains Mono';");
        layout->addWidget(outHeader);
        
        auto* scrollArea = new QScrollArea();
        scrollArea->setStyleSheet("QScrollArea { background-color: #252538; border: none; border-radius: 4px; }");
        scrollArea->setMaximumHeight(200);
        
        auto* detailText = new QLabel(r.details);
        detailText->setStyleSheet("font-size: 10px; color: #A0A0B8; padding: 8px; font-family: 'JetBrains Mono';");
        detailText->setWordWrap(true);
        scrollArea->setWidget(detailText);
        layout->addWidget(scrollArea);
    }
    
    // Close button
    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Close);
    QObject::connect(btnBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    layout->addWidget(btnBox);
    
    dlg->show();
#endif // !PLATFORM_IOS && !PLATFORM_ANDROID
}

QVariantMap AppState::getDetailResult(int diagIdInt) const {
    QVariantMap m;
    if (!isValidDiagId(diagIdInt)) return m;
    auto id = static_cast<DiagId>(diagIdInt);
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