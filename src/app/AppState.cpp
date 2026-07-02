// =============================================================================
// AppState.cpp
// =============================================================================
#include "app/AppState.h"
#include "engine/task/TaskFactory.h"
#include "util/DebugSwitch.h"
#include "util/Logger.h"
#include <cstdio>
#include <chrono>
#include <QTimer>
#include <QCoreApplication>
#include <QTextDocument>
#include <QPdfWriter>
#include <QPageSize>
#include <QPageLayout>
#include <QMarginsF>
#include <QStandardPaths>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QUrl>
#include <QUrlQuery>
#include <QFileInfo>
#include <QProcess>
#include <QDesktopServices>
#include "util/PlatformShare.h"
#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
#include <QDialog>
#endif
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
#include <QVBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QFileDialog>
#endif

AppState::AppState(QObject* parent) : QObject(parent) {
    // Enable G1-G3 by default; G4/G5 are auto-managed based on target
    for (auto id : allDiagIds()) {
        auto g = diagGroup(id);
        if (g == DiagGroup::G1 || g == DiagGroup::G2 || g == DiagGroup::G3)
            m_enabledDiags.insert(id);
    }
    // NOTE: iOS-unavailable tests (TCP settings, ARP table) are NOT hidden here.
    // They stay enabled and report DiagStatus::Skipped so the UI shows a skip
    // icon (like Active Connections). Default gateway / routing table / DHCP now
    // have working iOS implementations and return real data.
}

AppState::~AppState() {
    if (m_runStatus == RunStatus::Running) {
        m_runStatus = RunStatus::Cancelled;
    }
}

// ── App version / build number ──────────────────────────────────────────
QString AppState::appVersion() const {
    const QString v = QCoreApplication::applicationVersion();
    return v.isEmpty() ? QStringLiteral("0.0.1") : v;
}

QString AppState::buildNumber() const {
#if defined(__APPLE__)
    // Read CFBundleVersion from the app bundle Info.plist (CI sets this to the
    // GitHub run number, e.g. "379"). CoreFoundation is a C API — usable here
    // without Objective-C.
    if (CFBundleRef bundle = CFBundleGetMainBundle()) {
        CFTypeRef v = CFBundleGetValueForInfoDictionaryKey(bundle, kCFBundleVersionKey);
        if (v && CFGetTypeID(v) == CFStringGetTypeID()) {
            char buf[64] = {0};
            if (CFStringGetCString((CFStringRef)v, buf, sizeof(buf), kCFStringEncodingUTF8))
                return QString::fromUtf8(buf);
        }
    }
#endif
#ifdef ND_BUILD_NUMBER
    return QStringLiteral(ND_BUILD_NUMBER);
#else
    return QString();
#endif
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
        // G5: http/https only (supported via libcurl on Windows/Linux, NSURLSession on iOS, HttpURLConnection on Android)
        setGroupEnabled(3, has);          // G4 on if target non-empty
#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID) || !defined(NO_CURL)
        setGroupEnabled(4, has && isHttp); // G5 on only for http/https (if platform supports it)
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

    // Create task via factory — each task handles its own timeout internally
    int runGen = m_runGeneration.load(std::memory_order_acquire);
    auto task = TaskFactory::createTask(id, m_target, m_portScanFrom, m_portScanTo, m_portScanCommon);
    if (!task) {
        onDiagFinished(id, DiagnosticResult::error(id, QStringLiteral("Unknown DiagId")));
        return;
    }

    // When task completes (or times out), route to onDiagFinished
    connect(task.get(), &DiagnosticTask::finished, this,
        [this, id, groupIdx, runGen](const DiagnosticResult& result) {
            if (m_runGeneration.load(std::memory_order_acquire) != runGen) return;
            if (m_results.contains(id)) return;
            onDiagFinished(id, result);
            int done = m_activeGroupDone.fetch_add(1) + 1;
            auto& gt = m_pendingGroups[groupIdx];
            if (done >= gt.diagIds.size()) {
                m_currentGroupIdx++;
                QTimer::singleShot(0, this, &AppState::startNextGroup);
            }
        });

    // The task owns its own lifetime: it self-deletes (via deleteLater) only
    // after its worker run() has returned — see DiagnosticTask::onFutureFinished.
    // Deleting it here on the first finished() signal would be unsafe, because a
    // watchdog timeout emits finished() while run() may still be executing.
    task.release()->start(); // transfer ownership to Qt parent/event loop
}

void AppState::onDiagFinished(DiagId id, DiagnosticResult result) {
    TRACE(" onDiagFinished id=%d status=%d\n", (int)id, (int)result.status);
    // Suppress stale results after cancel/reset
    if (m_runStatus != RunStatus::Running) return;
    if (m_results.contains(id)) return; // already handled by timeout/cancel
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

    // DiagnosticTask::cancel() stops the per-task watchdog and suppresses
    // the finished() signal via the atomic cancelled flag. Tasks in-flight
    // will complete silently in the background. No explicit result marking
    // is needed — the UI shows pending tests as cancelled via runStatus.
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
            case DiagStatus::Error:   fail++; break; // timeout counts as failure
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

// ── Report export ─────────────────────────────────────────────
namespace {
QString reportStatusText(DiagStatus s) {
    switch (s) {
        case DiagStatus::Pass:    return QStringLiteral("Pass");
        case DiagStatus::Warning: return QStringLiteral("Warning");
        case DiagStatus::Fail:    return QStringLiteral("Fail");
        case DiagStatus::Skipped: return QStringLiteral("Skipped");
        case DiagStatus::Error:   return QStringLiteral("Error");
        case DiagStatus::Info:    return QStringLiteral("Info");
    }
    return QStringLiteral("-");
}
QString reportStatusColor(DiagStatus s) {
    switch (s) {
        case DiagStatus::Pass:    return QStringLiteral("#16a34a");
        case DiagStatus::Warning: return QStringLiteral("#ca8a04");
        case DiagStatus::Fail:    return QStringLiteral("#dc2626");
        case DiagStatus::Skipped: return QStringLiteral("#6b7280");
        case DiagStatus::Error:   return QStringLiteral("#dc2626");
        case DiagStatus::Info:    return QStringLiteral("#2563eb");
    }
    return QStringLiteral("#111111");
}
// QML FileDialog hands back a file:// URL; convert to a local filesystem path.
QString normalizeReportPath(const QString& p) {
    return p.startsWith(QStringLiteral("file:")) ? QUrl(p).toLocalFile() : p;
}
} // namespace

QString AppState::buildReportHtml(bool fullDetail) const {
    const QString target = m_target.isEmpty() ? QStringLiteral("(none)") : m_target.toHtmlEscaped();
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    const QStringList labels = groupLabels();
    
    // Color palette
    const QString colorPass = QStringLiteral("#10B981");
    const QString colorWarn = QStringLiteral("#F59E0B");
    const QString colorFail = QStringLiteral("#EF4444");
    const QString colorSkip = QStringLiteral("#9CA3AF");
    const QString colorInfo = QStringLiteral("#3B82F6");

    int tPass=0,tWarn=0,tFail=0,tSkip=0,tInfo=0,tTotal=0;
    for (int g = 0; g < 5; ++g) {
        QVariantMap s = groupStats(g);
        tPass += s.value(QStringLiteral("pass")).toInt(); tWarn += s.value(QStringLiteral("warn")).toInt();
        tFail += s.value(QStringLiteral("fail")).toInt(); tSkip += s.value(QStringLiteral("skip")).toInt();
        tInfo += s.value(QStringLiteral("info")).toInt(); tTotal += s.value(QStringLiteral("total")).toInt();
    }

    QString h;
    // Header with gradient effect
    h += QStringLiteral("<div style=\"background:linear-gradient(135deg,#1F2937 0%,#374151 100%);padding:20px;border-radius:8px;margin-bottom:16px;color:white;text-align:center\">");
    h += QStringLiteral("<h1 style=\"margin:0 0 8px 0;font-size:28px;font-weight:bold\">Network Diagnostic Report</h1>");
    h += QStringLiteral("<p style=\"margin:0;font-size:12px;opacity:0.9\">Target: <b>%1</b> · Generated: %2</p>");
    h += QStringLiteral("<p style=\"margin:4px 0 0 0;font-size:11px;opacity:0.8\">NetDiagnostic v%3 (build %4)</p>");
    h += QStringLiteral("</div>")
        .arg(target, ts, appVersion(), buildNumber());

    // Summary stats with color-coded cards
    h += QStringLiteral("<div style=\"display:flex;gap:12px;margin-bottom:16px;flex-wrap:wrap\">");
    h += QStringLiteral("<div style=\"flex:1;min-width:120px;padding:12px;background:%1;opacity:0.1;border-left:4px solid %1;border-radius:4px\">"
        "<div style=\"font-size:14px;font-weight:bold;color:%1\">%2</div>"
        "<div style=\"font-size:11px;color:#666;margin-top:2px\">Passed</div></div>")
        .arg(colorPass, QString::number(tPass));
    h += QStringLiteral("<div style=\"flex:1;min-width:120px;padding:12px;background:%1;opacity:0.1;border-left:4px solid %1;border-radius:4px\">"
        "<div style=\"font-size:14px;font-weight:bold;color:%1\">%2</div>"
        "<div style=\"font-size:11px;color:#666;margin-top:2px\">Warnings</div></div>")
        .arg(colorWarn, QString::number(tWarn));
    h += QStringLiteral("<div style=\"flex:1;min-width:120px;padding:12px;background:%1;opacity:0.1;border-left:4px solid %1;border-radius:4px\">"
        "<div style=\"font-size:14px;font-weight:bold;color:%1\">%2</div>"
        "<div style=\"font-size:11px;color:#666;margin-top:2px\">Failed</div></div>")
        .arg(colorFail, QString::number(tFail));
    h += QStringLiteral("</div>");

    // Stats table with improved styling
    h += QStringLiteral("<h2 style=\"font-size:16px;margin:16px 0 12px 0;padding-bottom:8px;border-bottom:2px solid #E5E7EB\">Group Summary</h2>");
    h += QStringLiteral("<table style=\"width:100%;border-collapse:collapse;font-size:13px\">"
        "<tr style=\"background:#F3F4F6\">"
        "<th style=\"padding:10px;text-align:left;border:1px solid #E5E7EB\">Group</th>"
        "<th style=\"padding:10px;text-align:center;border:1px solid #E5E7EB\">Total</th>"
        "<th style=\"padding:10px;text-align:center;border:1px solid #E5E7EB;color:%1\">✓ Pass</th>"
        "<th style=\"padding:10px;text-align:center;border:1px solid #E5E7EB;color:%2\">⚠ Warn</th>"
        "<th style=\"padding:10px;text-align:center;border:1px solid #E5E7EB;color:%3\">✕ Fail</th>"
        "<th style=\"padding:10px;text-align:center;border:1px solid #E5E7EB;color:%4\">○ Skip</th>"
        "<th style=\"padding:10px;text-align:center;border:1px solid #E5E7EB;color:%5\">ℹ Info</th></tr>")
        .arg(colorPass, colorWarn, colorFail, colorSkip, colorInfo);
    for (int g = 0; g < 5; ++g) {
        QVariantMap s = groupStats(g);
        if (s.value(QStringLiteral("total")).toInt() == 0) continue;
        const int gPass = s.value(QStringLiteral("pass")).toInt();
        const int gWarn = s.value(QStringLiteral("warn")).toInt();
        const int gFail = s.value(QStringLiteral("fail")).toInt();
        const int gSkip = s.value(QStringLiteral("skip")).toInt();
        const int gInfo = s.value(QStringLiteral("info")).toInt();
        h += QStringLiteral("<tr style=\"border-bottom:1px solid #E5E7EB;background:%1\">"
            "<td style=\"padding:10px;border-right:1px solid #E5E7EB\"><b>G%2: %3</b></td>"
            "<td style=\"padding:10px;text-align:center;border-right:1px solid #E5E7EB\">%4</td>"
            "<td style=\"padding:10px;text-align:center;border-right:1px solid #E5E7EB;color:%5\">%6</td>"
            "<td style=\"padding:10px;text-align:center;border-right:1px solid #E5E7EB;color:%7\">%8</td>"
            "<td style=\"padding:10px;text-align:center;border-right:1px solid #E5E7EB;color:%9\">%10</td>"
            "<td style=\"padding:10px;text-align:center;border-right:1px solid #E5E7EB;color:%11\">%12</td>"
            "<td style=\"padding:10px;text-align:center;color:%13\">%14</td></tr>")
            .arg(g % 2 == 0 ? "#FFFFFF" : "#F9FAFB")
            .arg(g+1).arg(g < labels.size() ? labels[g].toHtmlEscaped() : QString())
            .arg(s.value(QStringLiteral("total")).toInt())
            .arg(colorPass).arg(gPass)
            .arg(colorWarn).arg(gWarn)
            .arg(colorFail).arg(gFail)
            .arg(colorSkip).arg(gSkip)
            .arg(colorInfo).arg(gInfo);
    }
    h += QStringLiteral("</table>");

    if (fullDetail) {
        h += QStringLiteral("<h2 style=\"font-size:16px;margin:20px 0 12px 0;padding-bottom:8px;border-bottom:2px solid #E5E7EB\">Detailed Results</h2>");
        for (int g = 0; g < 5; ++g) {
            if (groupStats(g).value(QStringLiteral("total")).toInt() == 0) continue;
            h += QStringLiteral("<h3 style=\"font-size:14px;margin:14px 0 10px 0;padding:8px;background:%1;border-left:3px solid %2;border-radius:3px;color:#FFFFFF\">"
                "G%3: %4</h3>")
                .arg(Qt::rgba(100, 116, 139, 0.3))
                .arg(QStringLiteral("#3B82F6"))
                .arg(g+1).arg(g < labels.size() ? labels[g].toHtmlEscaped() : QString());
            for (auto id : diagIdsForGroup(static_cast<DiagGroup>(g))) {
                if (!m_results.contains(id)) continue;
                const auto& r = m_results[id];
                const QString name = (r.displayName.isEmpty() ? staticDiagDisplayName(id)
                                                              : r.displayName).toHtmlEscaped();
                const QString statusColor = reportStatusColor(r.status);
                h += QStringLiteral("<div style=\"margin:10px 0;padding:10px;border-left:3px solid %1;background:%2;border-radius:4px\">"
                    "<b>%3</b> <span style=\"color:%1;font-weight:bold\">%4</span> "
                    "<span style=\"color:#9CA3AF;font-size:11px\">%5 ms</span>"
                    "</div>")
                    .arg(statusColor, Qt::rgba(31, 41, 55, 0.05))
                    .arg(name, reportStatusText(r.status))
                    .arg(r.durationMs);
                if (!r.summary.isEmpty())
                    h += QStringLiteral("<p style=\"margin:6px 0 0 0;color:#4B5563;font-size:12px\">%1</p>")
                        .arg(r.summary.toHtmlEscaped());
                const QString body = r.details.isEmpty() ? r.rawOutput : r.details;
                if (!body.trimmed().isEmpty())
                    h += QStringLiteral("<pre style=\"background:#f0f1f3;border:1px solid #d1d5db;padding:10px;border-radius:4px;"
                        "font-family:'Courier New',monospace;font-size:11px;color:#1f2937;overflow-x:auto;margin:8px 0\">%1</pre>")
                        .arg(body.toHtmlEscaped());
            }
        }
    }
    h += QStringLiteral("<div style=\"margin-top:20px;padding-top:12px;border-top:1px solid #E5E7EB;font-size:10px;color:#999999;text-align:center\">"
        "Generated by NetDiagnostic • All times in milliseconds</div>");
    return h;
}

QString AppState::defaultReportPath(const QString& ext) const {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (dir.isEmpty()) dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) dir = QDir::tempPath();
    QDir().mkpath(dir);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    return QDir(dir).filePath(QStringLiteral("NetDiagnostic_report_%1.%2").arg(stamp, ext));
}

QString AppState::exportHtml(const QString& filePath) const {
    const QString path = normalizeReportPath(filePath);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        Logger::instance().event(QStringLiteral("exportHtml: cannot open %1").arg(path));
        return QString();
    }
    QTextStream ts(&f);
    ts << buildReportHtml(true);
    f.close();
    return path;
}

QString AppState::exportPdf(const QString& filePath) const {
    const QString path = normalizeReportPath(filePath);
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);
    writer.setTitle(QStringLiteral("Network Diagnostic Report"));

    QTextDocument doc;
    doc.setHtml(buildReportHtml(false)); // summary only -> ~1 page
    doc.print(&writer);
    return QFile::exists(path) ? path : QString();
}

void AppState::requestSavePath(const QString& format) {
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
    auto* dlg = new QFileDialog(nullptr, QStringLiteral("Save Report"),
                                defaultReportPath(format));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setAcceptMode(QFileDialog::AcceptSave);
    dlg->setNameFilter(format == QLatin1String("pdf")
        ? QStringLiteral("PDF document (*.pdf)")
        : QStringLiteral("HTML document (*.html)"));
    dlg->setDefaultSuffix(format);
    connect(dlg, &QFileDialog::fileSelected, this, [this, format](const QString& p) {
        if (!p.isEmpty()) emit savePathPicked(format, p);
    });
    dlg->open(); // non-modal (show()-style) — avoids the ARM64 exec() crash
#else
    // Mobile: no native file dialog — save straight to the Documents folder.
    emit savePathPicked(format, defaultReportPath(format));
#endif
}

void AppState::setPremium(bool v) {
    if (m_isPremium == v) return;
    m_isPremium = v;
    emit premiumChanged();
}

void AppState::shareReport(const QString& format) {
    if (!m_isPremium) { emit premiumRequired(); return; }
    const QString ext = (format == QLatin1String("pdf")) ? QStringLiteral("pdf")
                                                         : QStringLiteral("html");
#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID)
    // Generate into a temp file, then present the OS share sheet.
    const QString tmp = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("NetDiagnostic_report.%1").arg(ext));
    const QString saved = (ext == QLatin1String("pdf")) ? exportPdf(tmp) : exportHtml(tmp);
    if (saved.isEmpty()) { emit reportShared(false); return; }
    platformShareFile(saved,
                      ext == QLatin1String("pdf") ? QStringLiteral("application/pdf")
                                                  : QStringLiteral("text/html"),
                      QStringLiteral("Network Diagnostic Report"));
    emit reportShared(true);
#else
    // Desktop: save to Documents and hand off to the default mail client.
    const QString saved = (ext == QLatin1String("pdf")) ? exportPdf(defaultReportPath(ext))
                                                        : exportHtml(defaultReportPath(ext));
    if (saved.isEmpty()) { emit reportShared(false); return; }
    emailReportDesktop(saved);
    emit reportShared(true);
#endif
}

void AppState::emailReportDesktop(const QString& path) {
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
    const QString subject = QStringLiteral("Network Diagnostic Report");
#ifdef Q_OS_LINUX
    // xdg-email can attach the file directly to a new mail.
    if (QProcess::startDetached(QStringLiteral("xdg-email"),
            {QStringLiteral("--subject"), subject, QStringLiteral("--attach"), path}))
        return;
#endif
    // Fallback (Windows/macOS or no xdg-email): mailto cannot attach, so open the
    // default mail client with a note and reveal the saved file for manual attach.
    QUrl mailto;
    mailto.setScheme(QStringLiteral("mailto"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("subject"), subject);
    q.addQueryItem(QStringLiteral("body"),
        QStringLiteral("The Network Diagnostic report is saved at: %1").arg(path));
    mailto.setQuery(q);
    QDesktopServices::openUrl(mailto);
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#else
    Q_UNUSED(path);
#endif
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