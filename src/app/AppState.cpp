// =============================================================================
// AppState.cpp
// =============================================================================
#include "app/AppState.h"
#if defined(PLATFORM_IOS)
#include <ifaddrs.h>
#elif defined(PLATFORM_ANDROID)
#include <QJniObject>
#endif
#include "Common/Model/DiagNames.h"
#include "Common/Services/DnsResolver.h"
#include "Diagnostics/Model/G5/G5WebsiteUrl.h"
#include "Diagnostics/Controller/TaskFactory.h"
#include "Common/Utils/DebugSwitch.h"
#include "Common/Utils/Logger.h"
#include <QtConcurrent/QtConcurrent>
#include "Diagnostics/Model/GeoProbe.h"
#include "Diagnostics/Model/G3/G3Diagnostics.h"
#include "Dashboard/Controller/DashboardController.h"
#include "Diagnostics/Controller/DiagnosticsController.h"
#include "Configuration/Controller/ConfigurationController.h"
#include "Report/Controller/ReportController.h"
#include "Settings/Controller/SettingsController.h"
#include <QTimer>
#include <QUrl>
#include <QCoreApplication>
#include <QPdfWriter>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QBuffer>
#include <QImage>
#include <QDesktopServices>
#include <QSettings>
#include "Common/Platform/PlatformShare.h"
#if defined(PLATFORM_IOS)
#include "Common/Platform/PlatformStore.h"
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
    // G1-G3 active by default (G4/G5 auto-managed via setTarget)
    m_activeGroups = {0, 1, 2};

    // ── Create MVC Controllers & Models ──────────────────────────────────
    m_targetModel  = new TargetModel(this);
    m_resultsModel = new ResultsModel(this);
    m_dashCtrl   = new DashboardController(this, this);
    m_diagCtrl   = new DiagnosticsController(this, this);
    m_configCtrl = new ConfigurationController(this, this);
    m_reportCtrl = new ReportController(this, this);
    m_settingsCtrl = new SettingsController(this, this);

    // 5WHY: G4/G5 auto-management was inline in setTarget() — now reacts
    // to TargetModel::targetChanged signal, separating concerns.
    // 5WHY (2nd): the lambda handled G4/G5 auto-management but never
    // emitted AppState::targetChanged().  QML bindings on target,
    // defaultPortForScheme, and sub-fields were therefore stale after
    // individual field edits (e.g. user typing in host TextField).
    connect(m_targetModel, &TargetModel::targetChanged, this, [this]() {
        bool has = !m_targetModel->isEmpty();
        bool isUrl = m_targetModel->isUrl();
        setGroupEnabled(3, has);
        setGroupEnabled(4, has && isUrl);
        bool had3 = m_activeGroups.contains(3);
        bool had4 = m_activeGroups.contains(4);
        if (has) { m_activeGroups.insert(3); if (isUrl) m_activeGroups.insert(4); }
        else { m_activeGroups.remove(3); m_activeGroups.remove(4); }
        if (had3 != m_activeGroups.contains(3) || had4 != m_activeGroups.contains(4))
            emit groupActiveChanged();
        // Keep ResultsModel's G5 scheme filter in sync with the target
        m_resultsModel->setSchemeFilter(m_targetModel->scheme(), isUrl);
        // Forward to QML so bindings on target/targetScheme/etc. re-evaluate
        emit targetChanged();
    });

    // Forward ReportController signals
    connect(m_reportCtrl, &ReportController::savePathPicked,
            this, &AppState::savePathPicked);

    // Forward SettingsController signals
    connect(m_settingsCtrl, &SettingsController::premiumChanged,
            this, &AppState::premiumChanged);
    connect(m_settingsCtrl, &SettingsController::purchaseInProgressChanged,
            this, &AppState::purchaseInProgressChanged);
    connect(m_settingsCtrl, &SettingsController::premiumRequired,
            this, &AppState::premiumRequired);
    connect(m_settingsCtrl, &SettingsController::restoreCompleted,
            this, &AppState::restoreCompleted);
    connect(m_settingsCtrl, &SettingsController::languageChanged,
            this, &AppState::languageChanged);
    connect(m_settingsCtrl, &SettingsController::themeChanged,
            this, &AppState::themeChanged);

    // Enable G1-G3 by default; G4/G5 are auto-managed based on target
    m_configCtrl->config().enableDefaultGroups();

    // Restore persisted settings (language/theme/diags handled by Controllers)
    // 5WHY: null check was dead code — m_settingsCtrl initialized in ctor, never cleared.
    m_settingsCtrl->loadSettings();
    m_configCtrl->loadSettings();
    loadSettings();
}

AppState::~AppState() {
    if (m_runStatus == RunStatus::Running) {
        m_runStatus = RunStatus::Cancelled;
    }
}

// ── App version / edition / build number ─────────────────────────────────
QString AppState::appVersion() const {
    const QString v = QCoreApplication::applicationVersion();
    return v.isEmpty() ? QStringLiteral("0.0.1") : v;
}

QString AppState::appEdition() const {
#if defined(APP_EDITION)
    return QStringLiteral(APP_EDITION);
#else
    return QString();
#endif
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
#if defined(ND_BUILD_NUMBER)
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
int AppState::languageIndex() const { return m_settingsCtrl ? m_settingsCtrl->languageIndex() : 0; }
int AppState::themeMode() const { return m_settingsCtrl ? m_settingsCtrl->themeMode() : 2; }
bool AppState::isPremium() const { return m_settingsCtrl ? m_settingsCtrl->isPremium() : false; }
bool AppState::purchaseInProgress() const { return m_settingsCtrl ? m_settingsCtrl->purchaseInProgress() : false; }

// 0=EN,1=FR,2=DE,3=RU,4=IT,5=ZH_CN,6=ZH_TW,7=ES,8=PT
void AppState::setLanguage(int index) {
    m_settingsCtrl->setLanguageIndex(index);
    bumpVersion();
}

// ── Theme mode persistence ─────────────────────────────────────────────
void AppState::setThemeMode(int mode) {
    m_settingsCtrl->setThemeMode(mode);
    bumpVersion();
}

// ── Structured target field setters (delegated to TargetModel) ──────────
void AppState::setTargetScheme(const QString& s) { m_targetModel->setScheme(s); }
void AppState::setTargetHost(const QString& h)     { m_targetModel->setHost(h); }
void AppState::setTargetPort(int p)                { m_targetModel->setPort(p); }
void AppState::setTargetUsername(const QString& u)  { m_targetModel->setUsername(u); }
void AppState::setTargetPassword(const QString& p)  { m_targetModel->setPassword(p); }
void AppState::setTargetPath(const QString& p)      { m_targetModel->setPath(p); }

// ── Target (delegated to TargetModel — G4/G5 managed via signal connection) ─
void AppState::setTarget(const QString& t) {
    m_targetModel->setTarget(t);
    // TargetModel emits targetChanged → AppState lambda handles G4/G5
    emit targetChanged();
    bumpVersion();
}

// ── Group labels ───────────────────────────────────────────────────────────
QStringList AppState::groupLabels() const { return DiagnosticConfig::groupLabels(); }

// ── Test enable/disable — delegated to DiagnosticConfig ──────────────
bool AppState::isDiagEnabled(int diagIdInt) const { return m_configCtrl->config().isDiagEnabled(diagIdInt); }
// 5WHY: was calling m_configCtrl->config().setDiagEnabled() directly,
// which updates in-memory state but bypasses ConfigurationController's
// saveSettings() — individual test enable/disable toggles were LOST
// on app restart.  Now routes through the controller, which persists
// enabledDiags to QSettings on every change.
void AppState::setDiagEnabled(int diagIdInt, bool enabled) { m_configCtrl->setDiagEnabled(diagIdInt, enabled); saveSettings(); bumpVersion(); }
void AppState::setGroupEnabled(int groupInt, bool enabled) { m_configCtrl->setGroupEnabled(groupInt, enabled); saveSettings(); bumpVersion(); }
bool AppState::isGroupAllEnabled(int groupInt) const { return m_configCtrl->config().isGroupAllEnabled(groupInt); }
bool AppState::isGroupAnyEnabled(int groupInt) const { return m_configCtrl->config().isGroupAnyEnabled(groupInt); }

// ── Group activation (separate from enable/disable) ─────────────────
void AppState::setGroupActive(int groupInt, bool active) {
    if (groupInt < 0 || groupInt >= 5) return;
    bool changed = active ? m_activeGroups.contains(groupInt) == false
                          : m_activeGroups.contains(groupInt) == true;
    if (active)
        m_activeGroups.insert(groupInt);
    else
        m_activeGroups.remove(groupInt);
    if (changed) {
        saveSettings();
        emit groupActiveChanged();
        bumpVersion();
    }
}

bool AppState::isGroupActive(int groupInt) const {
    return m_activeGroups.contains(groupInt);
}

// ── Cellular detection ─────────────────────────────────────────────────────
bool AppState::isCellularData() const {
#if defined(PLATFORM_IOS)
    // iOS: check for pdp_ip* interfaces (Packet Data Protocol = cellular).
    // Also check for active WiFi (en* with UP+RUNNING) — if WiFi is active,
    // treat as non-cellular regardless of pdp_ip state (dual-SIM standby).
    struct ifaddrs* ifs = nullptr;
    if (getifaddrs(&ifs) != 0 || !ifs) return false;
    bool hasCellular = false, hasWiFi = false;
    for (auto* p = ifs; p; p = p->ifa_next) {
        if (!p->ifa_name) continue;
        bool up = (p->ifa_flags & IFF_UP) && (p->ifa_flags & IFF_RUNNING);
        if (!up) continue;
        if (strncmp(p->ifa_name, "pdp_ip", 6) == 0) hasCellular = true;
        // 5WHY: Some WiFi interfaces only have IPv6 (AF_INET6) on certain
        // carriers/configs.  Check for any valid address family — the presence
        // of an IP on an en* interface means the interface is provisioned.
        if (strncmp(p->ifa_name, "en", 2) == 0
            && p->ifa_addr && (p->ifa_addr->sa_family == AF_INET
                            || p->ifa_addr->sa_family == AF_INET6)) hasWiFi = true;
    }
    freeifaddrs(ifs);
    return hasCellular && !hasWiFi;  // only warn if cellular is the sole connection
#elif defined(PLATFORM_ANDROID)
    // Android: use ConnectivityManager via JNI to check active transports.
    // Returns true only when TRANSPORT_CELLULAR is active AND TRANSPORT_WIFI
    // is NOT active — same logic as iOS (warn only on cellular-only).
    QJniObject ctx = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity",
        "()Landroid/app/Activity;");
    if (!ctx.isValid()) return false;
    QJniObject svc = ctx.callObjectMethod(
        "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;",
        QJniObject::fromString("connectivity").object<jstring>());
    if (!svc.isValid()) return false;
    QJniObject net = svc.callObjectMethod(
        "getActiveNetwork", "()Landroid/net/Network;");
    if (!net.isValid()) return false;
    QJniObject caps = svc.callObjectMethod(
        "getNetworkCapabilities", "(Landroid/net/Network;)Landroid/net/NetworkCapabilities;",
        net.object());
    if (!caps.isValid()) return false;
    bool hasWiFi = caps.callMethod<jboolean>(
        "hasTransport", "(I)Z", 1);   // TRANSPORT_WIFI = 1
    bool hasCell = caps.callMethod<jboolean>(
        "hasTransport", "(I)Z", 0);   // TRANSPORT_CELLULAR = 0
    return hasCell && !hasWiFi;
#else
    return false;  // desktop — not applicable
#endif
}

// ── Continue after cellular warning ─────────────────────────────────────────
void AppState::continueAfterCellularWarn() {
    _cellularWarnVisible = false;
    emit cellularWarnVisibleChanged();
    // Skip cellular check on re-entry — user already approved.
    // Resume from the current group (G3 was paused in startNextGroup).
    _cellularApproved = true;
    startNextGroup();
    _cellularApproved = false;
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

    TRACE(" runDiagnostics start target='%s'\n", m_targetModel->target().toUtf8().constData());

    // Clear probe cache before each diagnostic run
    GeoProbe::instance().clear();

    // 5WHY: detectCountry() blocked the main thread for up to 5 seconds,
    // freezing the UI after Run was clicked. The original concern (QNAM
    // needing an event dispatcher) is moot — httpsGet() creates its own
    // local QEventLoop, which works in any thread.  Preload the GeoIP
    // cache in a background thread so subsequent calls from diagnostics
    // return instantly from the static cache without blocking the UI.
    QtConcurrent::run([]() {
        SystemDiagnostics::detectCountry(5000);
    });

    // Reset state before each run (clears previous results, error messages, etc.)
    reset();

    // Flutter behaviour: G1-G3 always run (local-only); G4 requires target; G5 requires URL.
    // The group-level filter below (hasTarget/isUrl) handles G4/G5 exclusion automatically.
    // No blanket error on empty target — only block if NO groups would run at all.

    bool hasTarget = !isTargetEmpty();
    m_runStatus = RunStatus::Running;
    m_runGeneration.fetch_add(1, std::memory_order_release); // invalidate stale callbacks
    TRACE(" status=Running generation=%d, building pending tests\n", (int)m_runGeneration.load());
    m_totalDiags = 0;
    m_results.clear();
    m_totalPerGroup.clear();
    m_currentDiagName.clear();
    m_currentGroup.clear();

    // Build groups: group tests by DiagGroup (G1→G5 order)
    m_pendingGroups.clear();
    TRACE(" runDiagnostics: enabledTests=%d hasTarget=%d\n",
            (int)m_configCtrl->config().enabledDiags().size(), hasTarget);
    // Per-group enabled counts (verify checkbox state)
    for (int g = 0; g < 5; ++g) {
        int enabledInGroup = 0;
        int totalInGroup = 0;
        auto group = static_cast<DiagGroup>(g);
        for (auto id : DiagnosticConfig::diagIdsForGroup(group)) {
            totalInGroup++;
            if (m_configCtrl->config().enabledDiags().contains(id)) enabledInGroup++;
        }
        TRACE("   G%d: %d/%d enabled\n", g+1, enabledInGroup, totalInGroup);
    }
    for (int g = 0; g < 5; ++g) {
        // Skip inactive groups (user toggled them off via G1-G5 buttons)
        if (!m_activeGroups.contains(g)) continue;
        GroupTask gt;
        gt.group = static_cast<DiagGroup>(g);
        for (auto id : DiagnosticConfig::diagIdsForGroup(gt.group)) {
            if (!m_configCtrl->config().enabledDiags().contains(id)) continue;
            if (gt.group == DiagGroup::G4 && !hasTarget) continue;
            if (gt.group == DiagGroup::G5 && !hasTarget) continue;
            // G5: filter by scheme — only schedule tests matching the target's protocol
            if (gt.group == DiagGroup::G5 && hasTarget) {
                QString scheme = m_targetModel->scheme().isEmpty()
                    ? QStringLiteral("https") : m_targetModel->scheme().toLower();
                if (!g5DiagMatchesScheme(id, scheme)) continue;
            }
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
    m_resultsModel->setTotalPerGroup(m_totalPerGroup);
    m_resultsModel->setTotalDiags(m_totalDiags);
    // Pass enabled diag IDs so ResultsModel can show disabled tests as skipped immediately
    {
        QSet<int> enabledIds;
        const auto& cfgEnabled = m_configCtrl->config().enabledDiags();
        for (auto id : cfgEnabled) enabledIds.insert(static_cast<int>(id));
        m_resultsModel->setEnabledDiags(enabledIds);
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
        m_resultsModel->setCurrentGroup(-1);  // no group running
        emit runStatusChanged();
        emit progressChanged();
        bumpVersion();
        Logger::instance().event(QStringLiteral("Diagnostic run complete"));
        return;
    }

    auto& gt = m_pendingGroups[m_currentGroupIdx];
    m_currentGroup = diagGroupLabel(gt.group);

    // 5WHY: G3 Internet tests make real network requests (DNS, HTTP, DoH).
    // Check at the G2→G3 boundary (not at runDiagnostics start) so G1/G2
    // complete first, and only warn when cellular is the sole connection.
    if (!_cellularApproved && gt.group == DiagGroup::G3 && isCellularData()
        && m_configCtrl && m_configCtrl->isGroupAnyEnabled(static_cast<int>(DiagGroup::G3))) {
        _cellularWarnVisible = true;
        emit cellularWarnVisibleChanged();
        return;  // pause — continueAfterCellularWarn() will resume
    }

    m_activeGroupDone.store(0);
    m_resultsModel->setCurrentGroup(m_currentGroupIdx);  // spinner for this group
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
    m_currentDiagName = ::diagDisplayName(id);
    emit currentDiagChanged();
    emit groupChanged();
    bumpVersion();

    TRACE(" runDiag id=%d name='%s' group=%d\n", (int)id, m_currentDiagName.toUtf8().constData(), groupIdx);

    // ── Phase 2: Simulated platform skip-policy enforcement ───────────
    // 5WHY: Without this check, tests that should be skipped on the simulated
    // platform would still execute on the host, producing false Pass/Fail
    // results that don't reflect the simulated platform's capabilities.
    // Spec: "isSimulatedPlatformShouldSkip > host capability check".
    if (m_skipReasonMap.contains(static_cast<int>(id))) {
        QString reason = m_skipReasonMap[static_cast<int>(id)];
        TRACE("   SKIPPED by policy: %s\n", reason.toUtf8().constData());
        // Use factory to populate details/rawOutput (not just summary)
        DiagnosticResult skipped = DiagnosticResult::skipped(
            id, QStringLiteral("Simulated platform policy: %1").arg(reason));
        onDiagFinished(id, skipped);
        // 5WHY: Skipped tests bypass task creation, so the connect() lambda's
        // m_activeGroupDone counter would never fire. Increment it here so
        // group completion tracking stays correct.
        int done = m_activeGroupDone.fetch_add(1) + 1;
        if (done >= gt.diagIds.size()) {
            m_currentGroupIdx++;
            QTimer::singleShot(0, this, &AppState::startNextGroup);
        }
        return;
    }

    // Create task via factory — each task handles its own timeout internally
    int runGen = m_runGeneration.load(std::memory_order_acquire);
    auto task = TaskFactory::createTask(id, m_targetModel->target());
    if (!task) {
        // 5WHY: Generic "Unknown DiagId" gave no clue which test or how to fix.
        // Now includes the DiagId value and target to help diagnose Config/enum mismatches.
        onDiagFinished(id, DiagnosticResult::error(id,
            QStringLiteral("Unknown DiagId %1 (target: %2) — check Config/diagIdsForGroup for unregistered tests")
            .arg(static_cast<int>(id)).arg(m_targetModel->target())));
        // 5WHY: Like the skip-policy path above, the null-task path also
        // bypasses the connect() lambda's m_activeGroupDone counter. Increment
        // it here to prevent group deadlock.
        int done = m_activeGroupDone.fetch_add(1) + 1;
        if (done >= gt.diagIds.size()) {
            m_currentGroupIdx++;
            QTimer::singleShot(0, this, &AppState::startNextGroup);
        }
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
    // 5WHY: task.release()->start() leaked the raw pointer if start() threw
    // (e.g. QTimer allocation failure).  Now: call start() first with unique_ptr
    // still owning, release only after success.  Verdict: SAFE refactor — same
    // ownership transfer semantics, better exception safety.
    auto* rawTask = task.get();
    rawTask->start();
    task.release(); // only release ownership after start() succeeds
}

// 5WHY: DiagnosticResult parameter was passed by value (copy into parameter +
// copy into map = double copy).  const& avoids the parameter-copy overhead;
// m_results[id]=result still does one copy into the QMap.  DiagnosticResult
// contains QString/QDateTime/QVector — each copy is a heap allocation.
void AppState::onDiagFinished(DiagId id, const DiagnosticResult& result) {
    TRACE(" onDiagFinished id=%d status=%d\n", (int)id, (int)result.status);
    if (m_runStatus != RunStatus::Running) return;
    if (m_results.contains(id)) return;
    m_results[id] = result;
    m_resultsModel->addResult(id, result);

    emit progressChanged();
    emit diagCompleted(static_cast<int>(id));
    if (result.status == DiagStatus::Fail || result.status == DiagStatus::Error)
        emit diagFailed(static_cast<int>(id));
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
    // 5WHY: Clearing m_pendingGroups while in-flight tasks still hold
    // lambdas referencing it by index causes out-of-bounds access. Bumping
    // m_runGeneration invalidates all stale lambdas before they can execute.
    m_runGeneration.fetch_add(1, std::memory_order_release);
    m_runStatus = RunStatus::Idle;
    m_totalDiags = 0;
    m_results.clear();
    m_totalPerGroup.clear();
    m_errorMessage.clear();
    m_pendingGroups.clear();
    m_currentGroupIdx = 0;
    m_activeGroupDone.store(0);
    m_resultsModel->clear();
    m_resultsModel->setCurrentGroup(-1);
    // 5WHY: DNS cache was never invalidated between runs. If the network
    // changes (VPN, Wi-Fi handoff), subsequent runs would return stale
    // cached IPs from the previous network. Clearing ensures fresh resolution.
    DnsResolver::instance().clearCache();
    emit runStatusChanged();
    emit progressChanged();
    emit resultsReset();
    bumpVersion();
}

QString AppState::currentDiagLabel() const {
    if (m_runStatus == RunStatus::Running)
        return QStringLiteral("%1: %2").arg(m_currentGroup, m_currentDiagName);
    return {};
}

QString AppState::diagDisplayName(int diagIdInt) const {
    return ::diagDisplayName(static_cast<DiagId>(diagIdInt));
}

// staticDiagDisplayName — now delegates to shared DiagNames.h
QString AppState::staticDiagDisplayName(DiagId id) {
    return ::diagDisplayName(id);
}

ReportData AppState::buildReportData() const {
    ReportData d;
    d.target = m_targetModel->isEmpty() ? QStringLiteral("(none)") : m_targetModel->target().toHtmlEscaped();
    d.timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    d.appVersion = appVersion();
    d.buildNumber = buildNumber();
    d.groupLabels = groupLabels();
    d.results = m_results;
    d.diagDisplayName = &AppState::staticDiagDisplayName;
    for (int g = 0; g < 5; ++g) {
        d.groupStats[g] = groupStats(g);
        d.diagIdsInGroup[static_cast<DiagGroup>(g)] =
            DiagnosticConfig::diagIdsForGroup(static_cast<DiagGroup>(g));
    }
    return d;
}

QString AppState::buildReportHtml(bool fullDetail, bool darkBackground) const {
    return ReportEngine::buildHtml(buildReportData(), fullDetail, darkBackground);
}

QString AppState::renderPreviewImage(const QString& html, int width) const {
    QImage img = ReportEngine::renderHtmlToImage(html, width);
    if (img.isNull()) return {};
    // 5WHY: Saving to a temp file and passing a file:// URL to QML Image
    // fails on iOS (sandbox blocks bare paths) and requires path-separator
    // normalization on Windows. Base64-encoded data URI is self-contained —
    // no filesystem access, no platform-specific URL construction.
    // QML Image natively supports data: URIs.
    QByteArray pngData;
    QBuffer buf(&pngData);
    buf.open(QIODevice::WriteOnly);
    if (!img.save(&buf, "PNG")) return {};
    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(pngData.toBase64());
}

// Full standalone HTML document with modern dark-theme CSS. Used by:
// - In-app QML preview (via exportHtml() → HtmlPreviewWebView)
// - External browser / mail client (exportHtml → open/shared externally)
// The Qt-subset buildReportHtml() is used only for PDF export + image fallback.
QString AppState::buildRichHtmlDocument(bool darkBackground) const {
    return ReportEngine::buildRichDocument(buildReportData(), darkBackground);
}
QString AppState::defaultReportPath(const QString& ext) const {
    return ReportEngine::defaultReportPath(ext);
}
QString AppState::exportHtml(const QString& filePath, bool darkBackground) const {
    return ReportEngine::exportHtml(filePath, buildRichHtmlDocument(darkBackground));
}

QString AppState::exportPdf(const QString& filePath) const {
    // 5WHY: buildReportHtml(false) defaulted darkBackground=false (light)
    // producing a PDF that looked completely different from the dark-themed
    // in-app preview. Now uses isDarkMode() so shared PDF matches app theme.
    return ReportEngine::exportPdf(filePath, buildReportHtml(true, isDarkMode()));
}

void AppState::openPdfExternally() const {
    // 5WHY: "PDF Preview" showed a QTextDocument→QImage PNG, not a real PDF.
    // Generate an actual PDF and open it in the system's native PDF viewer.
    const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("NetDiagnostics_preview.pdf"));
    const QString saved = exportPdf(path);
    if (!saved.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(saved));
    }
}

void AppState::openHtmlExternally() const {
    const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("NetDiagnostics_preview.html"));
    const QString saved = exportHtml(path, isDarkMode());
    if (!saved.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(saved));
    }
}

QString AppState::generatePreviewPdf() const {
    // 5WHY: Fixed filename meant PdfDocument.source URL never changed
    // across re-generations (e.g. theme toggle). PdfDocument only reloads
    // when the source URL string changes, not the file content.
    // msecsSinceEpoch guarantees unique filename (vs. zzz which may
    // always be "000" on platforms without sub-second clock precision).
    const qint64 ts = QDateTime::currentMSecsSinceEpoch();
    const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("NetDiagnostics_preview_%1.pdf").arg(ts));
    const QString saved = exportPdf(path);
    if (saved.isEmpty()) return {};
    // 5WHY: Timestamped files accumulate in TempLocation on each preview
    // open + theme toggle. Clean up old preview files (keep last 3) to
    // prevent unbounded growth. TempLocation is cleaned by OS eventually,
    // but proactive cleanup is kinder on storage-constrained devices.
    QDir tmpDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    const auto entries = tmpDir.entryInfoList(
        {QStringLiteral("NetDiagnostics_preview_*.pdf")},
        QDir::Files, QDir::Time); // newest first
    for (int i = 3; i < entries.size(); ++i) {
        QFile::remove(entries[i].absoluteFilePath());
    }
    // 5WHY: Returned QUrl::fromLocalFile(saved).toString() which is a file://
    // URL.  ReportScreen.qml's toFileUrl() then prepended ANOTHER file:///,
    // creating an invalid double-prefix URL (file:///file:///...).  On iOS,
    // NativePdfDocument.setSource() checks url.isLocalFile() → false for
    // double-prefix → uses url.toString() which is invalid → PDF load fails.
    // Fix: return native path (same as exportHtml/exportPdf), let toFileUrl()
    // handle the file:/// conversion uniformly.
    return saved;
}

void AppState::requestSavePath(const QString& format) {
    m_reportCtrl->requestSavePath(format);
}

void AppState::setPremium(bool v) {
    m_settingsCtrl->setPremium(v);
}

void AppState::requestSubscription() {
    m_settingsCtrl->requestSubscription();
}

void AppState::restorePurchases() {
    m_settingsCtrl->restorePurchases();
}

void AppState::shareReport(const QString& format) {
    m_settingsCtrl->shareReport(format);
}

void AppState::shareExistingReport(const QString& filePath, const QString& format) {
    m_settingsCtrl->shareExistingReport(filePath, format);
}

void AppState::deleteFile(const QString& filePath) {
    if (!filePath.isEmpty()) QFile::remove(filePath);
}

void AppState::setCrashReportPath(const QString& path) {
    if (m_crashReportPath == path) return;
    m_crashReportPath = path;
    emit crashReportChanged();
}

void AppState::shareCrashReport() {
    if (m_crashReportPath.isEmpty() || !QFile::exists(m_crashReportPath))
        return;
#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID)
    // Mobile: OS share sheet so the user can email/upload the crash log.
    platformShareFile(m_crashReportPath, QStringLiteral("text/plain"),
                      QStringLiteral("NetDiagnostics Crash Report"));
#else
    // Desktop: reveal the crash log in the system file manager.
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_crashReportPath).absolutePath()));
#endif
}

void AppState::emailReportDesktop(const QString& path) {
    ReportEngine::emailReportDesktop(path);
}

void AppState::showDetailDialog(int diagIdInt) {
    if (!DiagnosticConfig::isValidDiagId(diagIdInt)) return;
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

// =============================================================================
// QSettings persistence — survives app restarts
// =============================================================================

static const char* kSettingsGroup = "settings";

void AppState::loadSettings() {
    QSettings s;
    s.beginGroup(QString::fromLatin1(kSettingsGroup));

    // Active groups (G1-G5 shown/hidden)
    QVariantList ag = s.value("activeGroups").toList();
    if (!ag.isEmpty()) {
        m_activeGroups.clear();
        for (const auto& v : ag)
            m_activeGroups.insert(v.toInt());
    }

    s.endGroup();
    s.sync();
}

void AppState::saveSettings() {
    QSettings s;
    s.beginGroup(QString::fromLatin1(kSettingsGroup));

    QVariantList ag;
    for (int g : m_activeGroups) ag.append(g);
    s.setValue("activeGroups", ag);

    s.endGroup();
    s.sync();
}

// ── Phase 2: Simulator skip-policy bridge ────────────────────────────────
void AppState::setSkipRules(const QVariantList& rules) {
    m_skipRules = rules;
    m_skipReasonMap.clear();
    for (const auto& v : rules) {
        QVariantMap m = v.toMap();
        int diagId = m.value("diagId", -1).toInt();
        QString reason = m.value("reason").toString();
        if (diagId >= 0 && !reason.isEmpty())
            m_skipReasonMap[diagId] = reason;
    }
    emit skipRulesChanged();
    TRACE(" Skip rules updated: %d rules active\n", m_skipReasonMap.size());
}