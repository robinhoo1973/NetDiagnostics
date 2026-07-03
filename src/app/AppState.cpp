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
#include <QFont>
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
#include <QSettings>
#include "util/PlatformShare.h"
#if defined(PLATFORM_IOS)
#include "util/PlatformStore.h"
#endif
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

    // Restore the Premium entitlement (non-consumable IAP com.netdiagnostic.app.premium).
    // Persisted locally after a verified purchase / restore so the unlock survives
    // app restarts without forcing the user to Restore every launch.
    {
        QSettings s;
        m_isPremium = s.value(QStringLiteral("premium/unlocked"), false).toBool();
    }
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
// 0=EN,1=FR,2=DE,3=RU,4=IT,5=ZH_CN,6=ZH_TW,7=ES,8=PT
void AppState::setLanguage(int index) {
    if (index < 0 || index > 8) return;
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
// Short CSS class token for the rich HTML badges (pass/warn/fail/skip/info).
QString reportStatusClass(DiagStatus s) {
    switch (s) {
        case DiagStatus::Pass:    return QStringLiteral("pass");
        case DiagStatus::Warning: return QStringLiteral("warn");
        case DiagStatus::Fail:    return QStringLiteral("fail");
        case DiagStatus::Error:   return QStringLiteral("fail");
        case DiagStatus::Info:    return QStringLiteral("info");
        case DiagStatus::Skipped: return QStringLiteral("skip");
    }
    return QStringLiteral("skip");
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
    // Qt rich text (QTextDocument / QML Text.RichText) supports only a limited
    // HTML4/CSS2 subset — no gradients / flex / border-radius. We use tables +
    // bgcolor + <font>, wrapped in a sans-serif <div> (Qt defaults to a dated
    // serif), so it renders cleanly in both the QML preview and the PDF.
    h += QStringLiteral("<div style=\"font-family:'Helvetica Neue',Arial,'PingFang SC','Microsoft YaHei',sans-serif;color:#0F172A\">");

    // ── Header band ──
    h += QStringLiteral(
        "<table width=\"100%\" cellpadding=\"16\" cellspacing=\"0\"><tr>"
        "<td bgcolor=\"#0F172A\">"
        "<font color=\"#FFFFFF\" size=\"6\"><b>Network Diagnostic Report</b></font><br/>"
        "<font color=\"#E2E8F0\" size=\"3\">%1</font><br/>"
        "<font color=\"#94A3B8\" size=\"2\">%2 &nbsp;&middot;&nbsp; v%3 (build %4)</font>"
        "</td></tr></table><br/>")
        .arg(target, ts, appVersion(), buildNumber());

    // ── Summary cards (5) ──
    auto card = [](const QString& bg, const QString& fg, int val, const QString& lbl) {
        return QStringLiteral(
            "<td width=\"20%\" align=\"center\" bgcolor=\"%1\">"
            "<font color=\"%2\" size=\"6\"><b>%3</b></font><br/>"
            "<font color=\"%2\" size=\"2\">%4</font></td>")
            .arg(bg, fg).arg(val).arg(lbl);
    };
    h += QStringLiteral("<table width=\"100%\" cellpadding=\"12\" cellspacing=\"6\"><tr>");
    h += card(QStringLiteral("#ECFDF5"), colorPass, tPass, QStringLiteral("Pass"));
    h += card(QStringLiteral("#FFFBEB"), colorWarn, tWarn, QStringLiteral("Warning"));
    h += card(QStringLiteral("#FEF2F2"), colorFail, tFail, QStringLiteral("Fail"));
    h += card(QStringLiteral("#F1F5F9"), colorSkip, tSkip, QStringLiteral("Skipped"));
    h += card(QStringLiteral("#EFF6FF"), colorInfo, tInfo, QStringLiteral("Info"));
    h += QStringLiteral("</tr></table>");
    h += QStringLiteral("<p align=\"center\"><font color=\"#64748B\" size=\"2\">%1 tests total</font></p><br/>")
        .arg(tTotal);

    // ── Per-group results — one table per group with a header band ──
    for (int g = 0; g < 5; ++g) {
        QVariantMap s = groupStats(g);
        if (s.value(QStringLiteral("total")).toInt() == 0) continue;
        const QString glabel = g < labels.size() ? labels[g].toHtmlEscaped() : QString();
        h += QStringLiteral(
            "<table width=\"100%\" cellpadding=\"9\" cellspacing=\"0\"><tr>"
            "<td bgcolor=\"#1E293B\">"
            "<font color=\"#FFFFFF\" size=\"3\"><b>G%1 &middot; %2</b></font>"
            "&nbsp;&nbsp;<font color=\"%3\" size=\"2\">%4 Pass</font>"
            "<font color=\"#64748B\" size=\"2\"> &middot; </font><font color=\"%5\" size=\"2\">%6 Warn</font>"
            "<font color=\"#64748B\" size=\"2\"> &middot; </font><font color=\"%7\" size=\"2\">%8 Fail</font>"
            "<font color=\"#64748B\" size=\"2\"> &middot; </font><font color=\"%9\" size=\"2\">%10 Skip</font>"
            "</td></tr></table>")
            .arg(g+1).arg(glabel)
            .arg(colorPass).arg(s.value(QStringLiteral("pass")).toInt())
            .arg(colorWarn).arg(s.value(QStringLiteral("warn")).toInt())
            .arg(colorFail).arg(s.value(QStringLiteral("fail")).toInt())
            .arg(colorSkip).arg(s.value(QStringLiteral("skip")).toInt());
        h += QStringLiteral(
            "<table width=\"100%\" border=\"1\" cellpadding=\"9\" cellspacing=\"0\" bordercolor=\"#E2E8F0\">"
            "<tr bgcolor=\"#F8FAFC\">"
            "<th align=\"left\" width=\"40%\"><font color=\"#64748B\" size=\"2\">TEST</font></th>"
            "<th align=\"left\" width=\"16%\"><font color=\"#64748B\" size=\"2\">STATUS</font></th>"
            "<th align=\"left\"><font color=\"#64748B\" size=\"2\">SUMMARY</font></th></tr>");
        bool alt = false;
        for (auto id : diagIdsForGroup(static_cast<DiagGroup>(g))) {
            if (!m_results.contains(id)) continue;
            const auto& r = m_results[id];
            const QString name = (r.displayName.isEmpty() ? staticDiagDisplayName(id)
                                                          : r.displayName).toHtmlEscaped();
            h += QStringLiteral(
                "<tr bgcolor=\"%1\">"
                "<td><font color=\"#0F172A\"><b>%2</b></font></td>"
                "<td><font color=\"%3\"><b>&#9679;&nbsp;%4</b></font></td>"
                "<td><font color=\"#475569\">%5</font></td></tr>")
                .arg(alt ? QStringLiteral("#F8FAFC") : QStringLiteral("#FFFFFF"))
                .arg(name)
                .arg(reportStatusColor(r.status), reportStatusText(r.status))
                .arg(r.summary.isEmpty() ? QStringLiteral("&mdash;") : r.summary.toHtmlEscaped());
            alt = !alt;
        }
        h += QStringLiteral("</table><br/>");
    }

    if (fullDetail) {
        h += QStringLiteral("<table width=\"100%\" cellpadding=\"10\" cellspacing=\"0\"><tr>"
            "<td bgcolor=\"#0F172A\"><font color=\"#FFFFFF\" size=\"4\"><b>Detailed Output</b></font></td>"
            "</tr></table><br/>");
        for (int g = 0; g < 5; ++g) {
            if (groupStats(g).value(QStringLiteral("total")).toInt() == 0) continue;
            h += QStringLiteral("<p><font color=\"#1E293B\" size=\"3\"><b>G%1 &middot; %2</b></font></p>")
                .arg(g+1).arg(g < labels.size() ? labels[g].toHtmlEscaped() : QString());
            for (auto id : diagIdsForGroup(static_cast<DiagGroup>(g))) {
                if (!m_results.contains(id)) continue;
                const auto& r = m_results[id];
                const QString name = (r.displayName.isEmpty() ? staticDiagDisplayName(id)
                                                              : r.displayName).toHtmlEscaped();
                const QString statusColor = reportStatusColor(r.status);
                h += QStringLiteral(
                    "<table width=\"100%\" cellpadding=\"9\" cellspacing=\"0\"><tr>"
                    "<td bgcolor=\"#F1F5F9\">"
                    "<font color=\"%1\"><b>&#9679;</b></font> <font color=\"#0F172A\"><b>%2</b></font> "
                    "<font color=\"%1\" size=\"2\"><b>%3</b></font> "
                    "<font color=\"#94A3B8\" size=\"2\">%4 ms</font>")
                    .arg(statusColor, name, reportStatusText(r.status))
                    .arg(r.durationMs);
                if (!r.summary.isEmpty())
                    h += QStringLiteral("<br/><font color=\"#475569\" size=\"2\">%1</font>")
                        .arg(r.summary.toHtmlEscaped());
                h += QStringLiteral("</td></tr></table>");
                const QString body = r.details.isEmpty() ? r.rawOutput : r.details;
                if (!body.trimmed().isEmpty())
                    h += QStringLiteral(
                        "<table width=\"100%\" cellpadding=\"10\" cellspacing=\"0\" border=\"1\" bordercolor=\"#334155\">"
                        "<tr><td bgcolor=\"#0F172A\"><pre style=\"font-family:'SF Mono','Consolas','Courier New',monospace;"
                        "font-size:11px;color:#E2E8F0\">%1</pre></td></tr></table><br/>")
                        .arg(body.toHtmlEscaped());
            }
        }
    }
    h += QStringLiteral("<p align=\"center\"><font color=\"#94A3B8\" size=\"2\">"
        "Generated by NetDiagnostic &middot; All times in milliseconds</font></p>");
    h += QStringLiteral("</div>");
    return h;
}

// Full standalone HTML document with a modern dark theme (styled after the
// PowerShell NetDiagnostic report). Rendered by a real browser / mail client —
// NOT by the in-app QML preview, which uses the Qt-subset buildReportHtml().
QString AppState::buildRichHtmlDocument() const {
    const QString target = m_target.isEmpty() ? QStringLiteral("(none)") : m_target.toHtmlEscaped();
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    const QStringList labels = groupLabels();

    int tPass=0,tWarn=0,tFail=0,tSkip=0,tInfo=0,tTotal=0;
    for (int g = 0; g < 5; ++g) {
        QVariantMap s = groupStats(g);
        tPass += s.value(QStringLiteral("pass")).toInt(); tWarn += s.value(QStringLiteral("warn")).toInt();
        tFail += s.value(QStringLiteral("fail")).toInt(); tSkip += s.value(QStringLiteral("skip")).toInt();
        tInfo += s.value(QStringLiteral("info")).toInt(); tTotal += s.value(QStringLiteral("total")).toInt();
    }

    static const char* kCss =
        "*{margin:0;padding:0;box-sizing:border-box}"
        "body{font-family:'Segoe UI',Roboto,Arial,sans-serif;background:#1a1a2e;color:#e0e0e0;padding:24px}"
        ".wrap{max-width:960px;margin:0 auto}"
        ".header{text-align:center;padding:34px 24px;background:linear-gradient(135deg,#16213e,#0f3460);border-radius:14px;margin-bottom:26px}"
        ".header h1{font-size:26px;color:#00bcd4;margin-bottom:10px;letter-spacing:.5px}"
        ".header p{font-size:13px;color:#a0a0b8;margin:3px 0}"
        "h2{font-size:18px;color:#00bcd4;margin:26px 0 14px}"
        "h3{font-size:15px;color:#7fb2e6;margin:20px 0 10px}"
        ".cards{display:flex;gap:14px;margin-bottom:22px;flex-wrap:wrap}"
        ".card{flex:1;min-width:110px;text-align:center;padding:18px 10px;border-radius:12px}"
        ".card .count{display:block;font-size:30px;font-weight:700}"
        ".card .label{font-size:11px;color:#a0a0b8;margin-top:6px;letter-spacing:1px;text-transform:uppercase}"
        ".card.pass{background:#16281b;border:1px solid #2d5a2d}.card.pass .count{color:#4ade80}"
        ".card.warn{background:#2b2810;border:1px solid #5a5020}.card.warn .count{color:#facc15}"
        ".card.fail{background:#2b1616;border:1px solid #5a2d2d}.card.fail .count{color:#ef4444}"
        ".card.skip{background:#1e1e2e;border:1px solid #333}.card.skip .count{color:#9aa0b5}"
        ".card.info{background:#141f33;border:1px solid #24406a}.card.info .count{color:#3b82f6}"
        "table.grid{width:100%;border-collapse:collapse;font-size:13px;border-radius:10px;overflow:hidden}"
        "table.grid th{text-align:left;padding:11px 12px;background:#16213e;color:#a0a0b8;font-weight:600}"
        "table.grid td{padding:9px 12px;border-bottom:1px solid #2a2a4a;vertical-align:top}"
        "tr.sec td{background:#1a2840;color:#7fb2e6;font-weight:700}"
        ".badge{display:inline-block;padding:2px 11px;border-radius:12px;font-size:11px;font-weight:700}"
        ".badge.pass{background:#16281b;color:#4ade80}.badge.warn{background:#2b2810;color:#facc15}"
        ".badge.fail{background:#2b1616;color:#ef4444}.badge.skip{background:#26262e;color:#9aa0b5}"
        ".badge.info{background:#141f33;color:#3b82f6}"
        "details.test{background:#16213e;border-radius:10px;margin-bottom:12px;overflow:hidden}"
        "details.test>summary{padding:13px 16px;cursor:pointer;font-weight:600;font-size:14px}"
        "details.test.pass>summary{border-left:4px solid #4ade80}details.test.warn>summary{border-left:4px solid #facc15}"
        "details.test.fail>summary{border-left:4px solid #ef4444}details.test.skip>summary{border-left:4px solid #666}"
        "details.test.info>summary{border-left:4px solid #3b82f6}"
        ".body{padding:14px 16px 18px;border-top:1px solid #2a2a4a}"
        ".analysis{background:#0f1629;border-left:3px solid #00bcd4;padding:11px 13px;border-radius:6px;margin-bottom:12px;font-size:13px;line-height:1.6}"
        ".raw{background:#0a0a14;padding:13px;border-radius:6px;font-family:'Consolas','Courier New',monospace;font-size:12px;white-space:pre-wrap;line-height:1.5;color:#c0c0d0;max-height:420px;overflow:auto}"
        ".meta{color:#8890a6;font-size:11px;font-weight:400}"
        ".footer{text-align:center;padding:20px;color:#5a5a72;font-size:11px;margin-top:28px;border-top:1px solid #23233a}";

    auto card = [](const QString& cls, int n, const QString& lbl) {
        return QStringLiteral("<div class=\"card %1\"><span class=\"count\">%2</span>"
            "<span class=\"label\">%3</span></div>").arg(cls).arg(n).arg(lbl);
    };

    QString h;
    h += QStringLiteral("<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"UTF-8\">\n"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        "<title>Network Diagnostic Report &mdash; %1</title>\n<style>").arg(ts);
    h += QString::fromLatin1(kCss);
    h += QStringLiteral("</style>\n</head>\n<body>\n<div class=\"wrap\">\n");
    h += QStringLiteral(
        "<div class=\"header\"><h1>Network Diagnostic Report</h1>"
        "<p>Generated: %1</p>"
        "<p>Target: <b style=\"color:#e0e0e0\">%2</b></p>"
        "<p>NetDiagnostic v%3 (build %4)</p></div>\n")
        .arg(ts, target, appVersion(), buildNumber());

    h += QStringLiteral("<div class=\"cards\">");
    h += card(QStringLiteral("pass"), tPass, QStringLiteral("Pass"));
    h += card(QStringLiteral("warn"), tWarn, QStringLiteral("Warning"));
    h += card(QStringLiteral("fail"), tFail, QStringLiteral("Fail"));
    h += card(QStringLiteral("skip"), tSkip, QStringLiteral("Skipped"));
    h += card(QStringLiteral("info"), tInfo, QStringLiteral("Info"));
    h += QStringLiteral("</div>\n");

    // Summary table — per group, each test's basic result
    h += QStringLiteral("<h2>Summary &middot; %1 tests</h2>\n").arg(tTotal);
    h += QStringLiteral("<table class=\"grid\"><thead><tr><th style=\"width:44px\">#</th>"
        "<th>Test</th><th style=\"width:96px\">Status</th><th>Summary</th></tr></thead><tbody>\n");
    int idx = 0;
    for (int g = 0; g < 5; ++g) {
        if (groupStats(g).value(QStringLiteral("total")).toInt() == 0) continue;
        const QString glabel = g < labels.size() ? labels[g].toHtmlEscaped() : QString();
        h += QStringLiteral("<tr class=\"sec\"><td colspan=\"4\">G%1 &middot; %2</td></tr>\n").arg(g+1).arg(glabel);
        for (auto id : diagIdsForGroup(static_cast<DiagGroup>(g))) {
            if (!m_results.contains(id)) continue;
            const auto& r = m_results[id];
            const QString name = (r.displayName.isEmpty() ? staticDiagDisplayName(id)
                                                          : r.displayName).toHtmlEscaped();
            ++idx;
            h += QStringLiteral("<tr><td>%1</td><td>%2</td>"
                "<td><span class=\"badge %3\">%4</span></td><td>%5</td></tr>\n")
                .arg(idx).arg(name)
                .arg(reportStatusClass(r.status), reportStatusText(r.status))
                .arg(r.summary.isEmpty() ? QStringLiteral("&mdash;") : r.summary.toHtmlEscaped());
        }
    }
    h += QStringLiteral("</tbody></table>\n");

    // Details — collapsible per test with summary + raw output
    h += QStringLiteral("<h2>Test Details</h2>\n");
    for (int g = 0; g < 5; ++g) {
        if (groupStats(g).value(QStringLiteral("total")).toInt() == 0) continue;
        const QString glabel = g < labels.size() ? labels[g].toHtmlEscaped() : QString();
        h += QStringLiteral("<h3>G%1 &middot; %2</h3>\n").arg(g+1).arg(glabel);
        for (auto id : diagIdsForGroup(static_cast<DiagGroup>(g))) {
            if (!m_results.contains(id)) continue;
            const auto& r = m_results[id];
            const QString name = (r.displayName.isEmpty() ? staticDiagDisplayName(id)
                                                          : r.displayName).toHtmlEscaped();
            const QString cls = reportStatusClass(r.status);
            h += QStringLiteral("<details class=\"test %1\"><summary>"
                "<span class=\"badge %1\">%2</span> &nbsp;%3 "
                "<span class=\"meta\">&middot; %4 ms</span></summary><div class=\"body\">")
                .arg(cls, reportStatusText(r.status), name).arg(r.durationMs);
            if (!r.summary.isEmpty())
                h += QStringLiteral("<div class=\"analysis\">%1</div>").arg(r.summary.toHtmlEscaped());
            const QString body = r.details.isEmpty() ? r.rawOutput : r.details;
            if (!body.trimmed().isEmpty())
                h += QStringLiteral("<div class=\"raw\">%1</div>").arg(body.toHtmlEscaped());
            h += QStringLiteral("</div></details>\n");
        }
    }

    h += QStringLiteral("<div class=\"footer\">Generated by NetDiagnostic &middot; "
        "All times in milliseconds</div>\n</div>\n</body>\n</html>\n");
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
    ts << buildRichHtmlDocument();
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
    doc.setDefaultFont(QFont(QStringLiteral("Helvetica"), 10));
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
    // Persist the non-consumable unlock so it survives restarts.
    QSettings().setValue(QStringLiteral("premium/unlocked"), v);
    emit premiumChanged();
}

void AppState::requestSubscription() {
    // Cross-platform entry point invoked from the QML "Subscribe" button.
    if (m_isPremium) return;       // already subscribed — shouldn't happen (UI guards)
    if (m_purchaseInProgress) return;

#if defined(PLATFORM_IOS)
    // ── iOS: real StoreKit purchase flow ──────────────────────────────
    m_purchaseInProgress = true;
    emit purchaseInProgressChanged();

    platformStartPurchase([this](bool success) {
        m_purchaseInProgress = false;
        emit purchaseInProgressChanged();

        if (success) {
            setPremium(true);           // emits premiumChanged → QML continues to share step
        }
        // On failure (including user-cancelled) we stay non-premium.
    });
#else
    // Android / Desktop: store SDK not wired yet → grant Premium directly
    // so the share flow remains usable. Replace with platformStartPurchase
    // once Google Play Billing is integrated.
    setPremium(true);
#endif
}

void AppState::restorePurchases() {
    if (m_isPremium) return;       // already unlocked
    if (m_purchaseInProgress) return;

#if defined(PLATFORM_IOS)
    // ── iOS: real StoreKit restore flow ───────────────────────────────
    m_purchaseInProgress = true;
    emit purchaseInProgressChanged();

    platformRestorePurchases([this](bool restoredAny) {
        m_purchaseInProgress = false;
        emit purchaseInProgressChanged();

        if (restoredAny) {
            setPremium(true);
        }
        emit restoreCompleted(restoredAny);
    });
#else
    // Android / Desktop: no store to restore from.
    emit restoreCompleted(false);
#endif
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