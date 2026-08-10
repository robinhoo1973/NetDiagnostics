// =============================================================================
// AppState.cpp
// =============================================================================
#include "app/AppState.h"
#if defined(PLATFORM_IOS)
#include <ifaddrs.h>
#else
#if defined(PLATFORM_ANDROID)
#include <QJniObject>
#endif
#endif
#include "Common/Model/DiagNames.h"
#include "Common/Platform/DeviceCapability.h"
#include "Common/Services/DnsResolver.h"
#include "Diagnostics/Model/G5/G5WebsiteUrl.h"
#include "Diagnostics/Controller/TaskFactory.h"
#include "Common/Utils/DebugSwitch.h"
#include "Common/Utils/Logger.h"
#include "Common/Utils/TargetRedaction.h"
#include <QtConcurrent/QtConcurrent>
#include <QThreadPool>
#include "Diagnostics/Model/GeoProbe.h"
#include "Diagnostics/Model/G3/G3Diagnostics.h"
#include "Configuration/Controller/ConfigurationController.h"
#include "Report/Controller/ReportController.h"
#include "Settings/Controller/SettingsController.h"
#include "Common/Utils/Translator.h"
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
#if defined(PLATFORM_ANDROID)
#include "Common/Platform/Android/PlatformAndroidJni.h"
#endif
#include "Common/Utils/StartupLog.h"
#if defined(PLATFORM_IOS)
#include "Common/Platform/PlatformStore.h"
#endif
#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

AppState::AppState(QObject* parent) : QObject(parent) {
    // G1-G3 active by default (G4/G5 auto-managed via setTarget)
    m_activeGroups = {0, 1, 2};

    // ── Create MVC Controllers & Models ──────────────────────────────────
    // 5WHY (Android launch crash): AppState ctor creates 7 controllers + 2 models
    // sequentially.  If ANY of these constructors crashes (e.g. missing native
    // library, JNI init failure, QSettings corruption), the app flash-quits
    // with zero diagnostic trail.  Log each step via STARTUP_LOG (NOT a
    // logcat-only macro) so the LAST line visible before the crash pinpoints
    // the failing constructor in the app-scoped file AND the public Download
    // mirror — readable by a non-technical user without adb.
    STARTUP_LOG("AppState ctor: creating TargetModel...");
    m_targetModel  = new TargetModel(this);
    STARTUP_LOG("TargetModel OK — creating ResultsModel...");
    m_resultsModel = new ResultsModel(this);
    STARTUP_LOG("ResultsModel OK — creating ConfigurationController...");
    m_configCtrl = new ConfigurationController(this, this);
    STARTUP_LOG("ConfigurationController OK — creating ReportController...");
    m_reportCtrl = new ReportController(this, this);
    STARTUP_LOG("ReportController OK — creating SettingsController...");
    m_settingsCtrl = new SettingsController(this, this);
    STARTUP_LOG("SettingsController OK — creating Translator...");
    m_translator = new Translator(this);
    m_translator->initialize(this);
    STARTUP_LOG("Translator OK — AppState ctor complete");

    // 5WHY: G4/G5 auto-management was inline in setTarget() — now reacts
    // to TargetModel::targetChanged signal, separating concerns.
    // 5WHY (2nd): the lambda handled G4/G5 auto-management but never
    // emitted AppState::targetChanged().  QML bindings on target,
    // defaultPortForScheme, and sub-fields were therefore stale after
    // individual field edits (e.g. user typing in host TextField).
    connect(m_targetModel, &TargetModel::targetChanged, this, [this]() {
        bool has = !m_targetModel->isEmpty();
        bool isUrl = m_targetModel->isUrl();
        // 5WHY (regression 2026-08-09): P0-1 removed the runtime auto-enable
        // of G4/G5 from this lambda to stop QSettings pollution, assuming the
        // new enableDefaultGroups() (all groups on) covered everyone. But
        // upgraded installs whose PERSISTED enabledDiags only contains G1-G3
        // (saved by old builds) are overwritten by loadSettings() and never
        // regain G4/G5 — "type a target → G4/G5 don't run".  Fix: restore a
        // MEMORY-ONLY auto-enable via ensureGroupTestsAvailable() — it runs
        // when a group has ZERO enabled tests (stale/upgraded config), never
        // persists, and leaves partially-configured groups (user intent)
        // untouched. Active-group gating stays on m_activeGroups.
        if (has) {
            ensureGroupTestsAvailable(DiagGroup::G4);
            if (isUrl) ensureGroupTestsAvailable(DiagGroup::G5);
        }
        bool had3 = m_activeGroups.contains(3);
        bool had4 = m_activeGroups.contains(4);
        // 5WHY: a Config green-dot deactivation is a USER preference — it must
        // survive target edits. Without this check the lambda re-inserted a
        // group the user explicitly deactivated, and the next setGroupActive
        // persisted the reversal (m_userDeactivatedGroups is written in
        // setGroupActive but was never READ here — review F-1).
        if (has) {
            if (!m_userDeactivatedGroups.contains(3)) m_activeGroups.insert(3);
            if (isUrl && !m_userDeactivatedGroups.contains(4)) m_activeGroups.insert(4);
        }
        else { m_activeGroups.remove(3); m_activeGroups.remove(4); }
        if (had3 != m_activeGroups.contains(3) || had4 != m_activeGroups.contains(4))
            emit groupActiveChanged();
        // Keep ResultsModel's G5 scheme filter in sync with the target
        m_resultsModel->setSchemeFilter(m_targetModel->scheme(), isUrl);
        // Forward to QML so bindings on target/targetScheme/etc. re-evaluate
        emit targetChanged();
        // One target mutation now produces one global refresh after all
        // derived state has settled, including structured-field setters.
        bumpVersion();
    });

    // Forward ReportController signals
    connect(m_reportCtrl, &ReportController::savePathPicked,
            this, &AppState::savePathPicked);

    // Forward the executing-group signal so QML can bind appState.currentRunningGroup
    // (QML binds to appState, not the resultsModel context property).
    connect(m_resultsModel, &ResultsModel::currentRunningGroupChanged,
            this, &AppState::currentRunningGroupChanged);

    // Forward SettingsController signals
    connect(m_settingsCtrl, &SettingsController::premiumChanged,
            this, &AppState::premiumChanged);
    connect(m_settingsCtrl, &SettingsController::purchaseInProgressChanged,
            this, &AppState::purchaseInProgressChanged);
    connect(m_settingsCtrl, &SettingsController::premiumRequired,
            this, &AppState::premiumRequired);
    connect(m_settingsCtrl, &SettingsController::purchaseDeferred,
            this, &AppState::purchaseDeferred);
    connect(m_settingsCtrl, &SettingsController::purchaseFailed,
            this, &AppState::purchaseFailed);
    connect(m_settingsCtrl, &SettingsController::restoreCompleted,
            this, &AppState::restoreCompleted);
    connect(m_settingsCtrl, &SettingsController::languageChanged,
            this, &AppState::languageChanged);
    connect(m_settingsCtrl, &SettingsController::themeChanged,
            this, &AppState::themeChanged);

    // Restore persisted settings (language/theme/diags handled by Controllers)
    // 5WHY: null check was dead code — m_settingsCtrl initialized in ctor, never cleared.
    m_settingsCtrl->loadSettings();

    // Detect or override the mobile-layout flag.
    // ND_MOBILE=1 forces the mobile layout on desktop so the screenshot CI
    // can capture authentic mobile screenshots without a device or simulator.
    if (qEnvironmentVariableIntValue("ND_MOBILE") == 1)
        m_isMobile = true;
    else
#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID)
        m_isMobile = true;
#else
        m_isMobile = false;
#endif

    // ND_CAPTURE_TARGET pre-fills the target URL for screenshot automation.
    // This bypasses the QML TextField binding-loop issue that prevents
    // xdotool/xvkbd from injecting keystrokes into the host field.
    m_configCtrl->loadSettings();
    loadSettings();

    // 5WHY: ND_CAPTURE_TARGET must be applied AFTER loadSettings() — a target
    // set first would make ensureGroupTestsAvailable() see the default
    // all-enabled config (no top-up needed), then loadSettings() would
    // overwrite it with the persisted G1-G3-only list, leaving G4/G5 disabled
    // for the whole session. Setting the target here runs the targetChanged
    // handler once, against the FINAL persisted state.
    const QString captureTarget = qEnvironmentVariable("ND_CAPTURE_TARGET");
    if (!captureTarget.isEmpty()) {
        m_targetModel->setTarget(captureTarget);
        TRACE("ND_CAPTURE_TARGET set: %s", qPrintable(captureTarget));
    }

    // ND_AUTORUN triggers a diagnostic run after startup — used by the
    // screenshot CI to enter the running/complete states without relying
    // on GUI automation to click buttons and type URLs.
    // ND_AUTORUN_DELAY_MS sets the delay (default 4000ms).
    if (qEnvironmentVariableIntValue("ND_AUTORUN") == 1) {
        int delayMs = qEnvironmentVariableIntValue("ND_AUTORUN_DELAY_MS");
        if (delayMs <= 0) delayMs = 4000;
        QTimer::singleShot(delayMs, this, [this]() {
            if (m_runStatus == RunStatus::Idle && !m_targetModel->isEmpty()) {
                TRACE("ND_AUTORUN firing (delay=%dms)", delayMs);
                runDiagnostics();
            }
        });
    }
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

QString AppState::gitHash() const {
#if defined(ND_GIT_HASH)
    // 5WHY: Full 40-char SHA1 overflows the version label on narrow screens.
    // Truncate to 7 chars — standard Git abbreviated hash, unique within repo.
    return QStringLiteral(ND_GIT_HASH).left(7);
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
int AppState::languageIndex() const { return m_settingsCtrl->languageIndex(); }
int AppState::themeMode() const { return m_settingsCtrl->themeMode(); }
bool AppState::isPremium() const { return m_settingsCtrl->isPremium(); }
bool AppState::isMobile() const { return m_isMobile; }
bool AppState::isPremiumPlatform() const { return m_settingsCtrl->isPremiumPlatform(); }
bool AppState::platformSupportsIap() const { return m_settingsCtrl->supportsIap(); }
bool AppState::purchaseInProgress() const { return m_settingsCtrl->purchaseInProgress(); }

// 0=EN,1=FR,2=DE,3=RU,4=IT,5=ZH_CN,6=ZH_TW,7=ES,8=PT
void AppState::setLanguage(int index) {
    if (m_settingsCtrl->languageIndex() == index) return;
    m_settingsCtrl->setLanguageIndex(index);
    bumpVersion();
}

// ── Theme mode persistence ─────────────────────────────────────────────
void AppState::setThemeMode(int mode) {
    if (m_settingsCtrl->themeMode() == mode) return;
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
    // TargetModel emits targetChanged → AppState lambda handles G4/G5 and
    // performs the single post-mutation stateVersion bump.
}

// ── Group labels ───────────────────────────────────────────────────────────
QStringList AppState::groupLabels() const { return DiagnosticConfig::groupLabels(); }

// ── Test enable/disable — delegated to DiagnosticConfig ──────────────
bool AppState::isDiagEnabled(int diagIdInt) const { return m_configCtrl->config().isDiagEnabled(diagIdInt); }
// 5WHY: was calling m_configCtrl->config().setDiagEnabled() directly,
// which updates in-memory state but bypasses ConfigurationController's
// saveSettings() — individual test enable/disable toggles were LOST
// on app restart.  Now routes through the controller, which persists
// enabledDiags to QSettings on every change.  AppState::saveSettings()
// only stores activeGroups and is not needed here.
void AppState::setDiagEnabled(int diagIdInt, bool enabled) {
    if (m_configCtrl->setDiagEnabled(diagIdInt, enabled)) {
        // 5WHY: record that the user explicitly configured this test's group
        // so ensureGroupTestsAvailable() never silently re-enables a group
        // the user chose to change (see ensureGroupTestsAvailable).
        m_userConfiguredGroups.insert(
            static_cast<int>(DiagnosticConfig::diagGroup(static_cast<DiagId>(diagIdInt))));
        bumpVersion();
    }
}
void AppState::setGroupEnabled(int groupInt, bool enabled) {
    if (m_configCtrl->setGroupEnabled(groupInt, enabled)) {
        m_userConfiguredGroups.insert(groupInt);
        bumpVersion();
    }
}
bool AppState::isGroupAllEnabled(int groupInt) const { return m_configCtrl->config().isGroupAllEnabled(groupInt); }
bool AppState::isGroupAnyEnabled(int groupInt) const { return m_configCtrl->config().isGroupAnyEnabled(groupInt); }

// ── Group activation (separate from enable/disable) ─────────────────
void AppState::setGroupActive(int groupInt, bool active) {
    if (groupInt < 0 || groupInt >= 5) return;
    // 5WHY: The old code computed `changed` via a ternary with negated
    // contains() and only acted if the membership changed.  Early-return
    // on no-change is simpler — it eliminates the temporary bool, two
    // negated expressions, and the nested if.
    if (m_activeGroups.contains(groupInt) == active) return;
    // 5WHY: record BOTH the user's intent. m_userDeactivatedGroups stops the
    // targetChanged lambda from re-inserting a group the user deactivated via
    // the Config green dot; m_userConfiguredGroups stops
    // ensureGroupTestsAvailable() from topping up a group the user touched.
    m_userConfiguredGroups.insert(groupInt);
    if (active) {
        m_activeGroups.insert(groupInt);
        m_userDeactivatedGroups.remove(groupInt);
    } else {
        m_activeGroups.remove(groupInt);
        m_userDeactivatedGroups.insert(groupInt);
    }
    saveSettings();
    emit groupActiveChanged();
    bumpVersion();
}

bool AppState::isGroupActive(int groupInt) const {
    return m_activeGroups.contains(groupInt);
}

// ── G4/G5 runtime availability (memory-only) ─────────────────────────────
void AppState::ensureGroupTestsAvailable(DiagGroup g) {
    const auto& ids = DiagnosticConfig::diagIdsForGroup(g);
    int enabled = 0;
    for (auto id : ids) {
        if (m_configCtrl->config().isDiagEnabled(static_cast<int>(id))) ++enabled;
    }
    // 5WHY: the per-call "enabled=X/Y" line must NOT go to Logger::event —
    // this runs on every targetChanged keystroke, so it would flood debug.log
    // (2 lines per character typed). TRACE is compiled out in release; the
    // persistent EVENT line is written ONLY when the state actually changes
    // (setAutomaticGroupEnabled returns true exactly when the group flips
    // from disabled to enabled).
    TRACE("ensureGroupTestsAvailable G%d: enabled=%d/%d", (int)g + 1, enabled, ids.size());
    // Only top-up when the WHOLE group is disabled AND the user has not
    // explicitly configured this group in this session (setDiagEnabled /
    // setGroupEnabled / setGroupActive all record into m_userConfiguredGroups).
    // A group with any enabled test means partial user intent — leave it
    // alone. setAutomaticGroupEnabled is memory-only (no saveSettings), so
    // this never writes the transient target state into QSettings.
    if (enabled == 0 && !ids.isEmpty()
        && !m_userConfiguredGroups.contains(static_cast<int>(g))) {
        if (m_configCtrl->setAutomaticGroupEnabled(static_cast<int>(g), true)) {
            Logger::instance().event(QStringLiteral("ensureGroupTestsAvailable: topped up G%1 (memory only)")
                .arg(static_cast<int>(g) + 1));
        }
    }
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
#else
#if defined(PLATFORM_ANDROID)
    // Android: use ConnectivityManager via JNI to check active transports.
    // Returns true only when TRANSPORT_CELLULAR is active AND TRANSPORT_WIFI
    // is NOT active — same logic as iOS (warn only on cellular-only).
    // 5WHY (Android launch crash): use getQtActivity() (version-independent
    // QNativeInterface accessor) instead of hardcoding the Qt5-era Java class
    // "org/qtproject/qt/android/QtNative", which is not stable in Qt 6.
    QJniObject ctx = getQtActivity();
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
#endif
}

// ── Continue after cellular warning ─────────────────────────────────────────
void AppState::continueAfterCellularWarn() {
    // 5WHY: No idempotency guard — if called twice (e.g. signal queue
    // delay + rapid double-tap), the second call would reset
    // m_activeGroupDone.store(0) and create duplicate G3 tasks.
    if (!_cellularWarnVisible) return;
    _cellularWarnVisible = false;
    emit cellularWarnVisibleChanged();
    // 5WHY: _bypassCellularCheck is a one-shot flag that suppresses the
    // cellular-data check for exactly one entry into startNextGroup().
    // It is set immediately before the call and cleared immediately after,
    // so it cannot leak into subsequent group transitions (e.g. G4/G5).
    // The flag is NOT a user-approval token — it is purely a control-flow
    // bypass for the G2→G3 boundary after the user clicked Continue.
    _bypassCellularCheck = true;
    startNextGroup();
    _bypassCellularCheck = false;
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

    TRACE(" runDiagnostics start target='%s'\n", TargetRedaction::forDisplay(m_targetModel->target()).toUtf8().constData());

    // 5WHY: Both QML run buttons block invalid targets, but automated capture,
    // tests, and future native entry points call this method directly. Without
    // the same guard here, malformed targets could still schedule G4/G5 work
    // and produce misleading network errors. AppState is the execution
    // boundary, so it must enforce the invariant independently of the UI.
    const QString validationError = m_targetModel->validationError();
    if (!m_targetModel->isEmpty() && !validationError.isEmpty()) {
        m_errorMessage = validationError;
        m_runStatus = RunStatus::Error;
        Logger::instance().event(QStringLiteral("Diagnostic run blocked by invalid target"));
        emit runStatusChanged();
        bumpVersion();
        return;
    }

    // Clear probe cache before each diagnostic run
    GeoProbe::instance().clear();

    // 5WHY: detectCountry() blocked the main thread for up to 5 seconds,
    // freezing the UI after Run was clicked. The original concern (QNAM
    // needing an event dispatcher) is moot — httpsGet() creates its own
    // local QEventLoop, which works in any thread.  Preload the GeoIP
    // cache in a background thread so subsequent calls from diagnostics
    // return instantly from the static cache without blocking the UI.
    // QThreadPool::start is the fire-and-forget primitive (QtConcurrent::run
    // returns a QFuture we never use and trips -Wunused-result).
    QThreadPool::globalInstance()->start([]() {
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
    m_totalPerGroup.clear();
    m_currentDiagName.clear();
    m_currentGroup.clear();

    // Build groups: group tests by DiagGroup (G1→G5 order)
    m_pendingGroups.clear();
    TRACE(" runDiagnostics: enabledTests=%d hasTarget=%d\n",
            (int)m_configCtrl->config().enabledDiags().size(), hasTarget);
    // 5WHY: hardware can change between runs (USB Ethernet, WiFi toggled).
    // Re-probe on every run so the schedule matches the current device and
    // the trace below reflects the same probe results.
    DeviceCapability::invalidateCache();
    // Per-group enabled counts (verify checkbox state)
    for (int g = 0; g < 5; ++g) {
        int enabledInGroup = 0;
        int totalInGroup = 0;
        auto group = static_cast<DiagGroup>(g);
        for (auto id : DiagnosticConfig::diagIdsForGroup(group)) {
            // 5WHY: count only tests that can actually run on this OS/device
            // so the trace reflects what will be scheduled, not the full 45.
            if (!DeviceCapability::diagRunnable(id)) continue;
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
            // 5WHY: platform/device-impossible tests are IGNORED entirely —
            // not scheduled, not counted, no Skipped result.  They are also
            // hidden from the Config page (ResultsModel::allDiagIdsForGroup).
            if (!DeviceCapability::diagRunnable(id)) continue;
            if (gt.group == DiagGroup::G4 && !hasTarget) continue;
            if (gt.group == DiagGroup::G5 && !hasTarget) continue;
            // G5: filter by scheme — only schedule tests matching the target's
            // protocol.  Tests that don't match are IGNORED entirely (hidden in
            // Config, never scheduled, never counted) instead of being recorded
            // as Skipped.
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

    // 5WHY: the schedule summary below (groups + per-group counts) is a
    // high-value triage log — it shows immediately whether G4/G5 were
    // scheduled for a target, versus being excluded by activation/scheme/
    // enabled-state gating. TRACE is compiled out, so use the persistent
    // Logger::event() stream.
    Logger::instance().event(QStringLiteral("Starting diagnostic run: %1 tests in %2 groups")
                              .arg(m_totalDiags).arg(m_pendingGroups.size()));
    for (int gi = 0; gi < m_pendingGroups.size(); ++gi)
        Logger::instance().event(QStringLiteral("  scheduled G%1: %2 tests")
            .arg(static_cast<int>(m_pendingGroups[gi].group) + 1)
            .arg(m_pendingGroups[gi].diagIds.size()));

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

    // 5WHY: gt was a reference into m_pendingGroups while the calls below
    // (setCurrentGroup, bumpVersion) emit signals that QML bindings can
    // re-enter AppState through (cancel/reset/runDiagnostics), which clears
    // m_pendingGroups → dangling-reference UB. Copy the lightweight
    // GroupTask (DiagGroup + QList<DiagId>) up front and never hold a
    // reference across signal emission.
    const GroupTask gt = m_pendingGroups[m_currentGroupIdx];
    m_currentGroup = diagGroupLabel(gt.group);

    // 5WHY: G3 Internet tests make real network requests (DNS, HTTP, DoH).
    // Check at the G2→G3 boundary (not at runDiagnostics start) so G1/G2
    // complete first, and only warn when cellular is the sole connection.
    if (!_bypassCellularCheck && gt.group == DiagGroup::G3 && isCellularData()
        && m_configCtrl && m_configCtrl->isGroupAnyEnabled(static_cast<int>(DiagGroup::G3))) {
        _cellularWarnVisible = true;
        emit cellularWarnVisibleChanged();
        return;  // pause — continueAfterCellularWarn() will resume
    }

    m_activeGroupDone.store(0);
    // 5WHY: setCurrentGroup() was passed the pending-group index while
    // ResultsModel::isRunning compares against the DiagGroup enum value
    // (static_cast<int>(g) == m_currentRunningGroup). When G1 was disabled,
    // pending[0] = G2 (enum 1) but setCurrentGroup(0) made G1's spinner
    // spin while G2's stayed still. Pass the enum value so the comparison
    // matches.
    m_resultsModel->setCurrentGroup(static_cast<int>(gt.group));  // spinner for this group
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

    // Create task via factory — each task handles its own timeout internally
    int runGen = m_runGeneration.load(std::memory_order_acquire);
    auto task = TaskFactory::createTask(id, m_targetModel->target());
    if (!task) {
        // 5WHY: Generic "Unknown DiagId" gave no clue which test or how to fix.
        // Now includes the DiagId value and target to help diagnose Config/enum mismatches.
        onDiagFinished(id, DiagnosticResult::error(id,
            QStringLiteral("Unknown DiagId %1 (target: %2) — check Config/diagIdsForGroup for unregistered tests")
            .arg(static_cast<int>(id)).arg(TargetRedaction::forDisplay(m_targetModel->target()))));
        // 5WHY: The null-task path bypasses the connect() lambda's
        // m_activeGroupDone counter. Increment it here to prevent group deadlock.
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
            if (m_resultsModel->hasResult(id)) return;
            onDiagFinished(id, result);
            int done = m_activeGroupDone.fetch_add(1) + 1;
            // 5WHY: m_pendingGroups[groupIdx] used QList::operator[] — an
            // out-of-range index inserts an EMPTY element (silent UB). The
            // generation check above already rejects stale runs, but a
            // bounds check makes the group-advance decision total even for
            // an unexpected callback order.
            if (groupIdx < m_pendingGroups.size()
                && done >= m_pendingGroups[groupIdx].diagIds.size()) {
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

void AppState::onDiagFinished(DiagId id, const DiagnosticResult& result) {
    TRACE(" onDiagFinished id=%d status=%d\n", (int)id, (int)result.status);
    if (m_runStatus != RunStatus::Running) return;
    if (m_resultsModel->hasResult(id)) return;
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

    // 5WHY: _cellularWarnVisible was cleared by QML callers (Cancel button,
    // backdrop tap, nav dismiss) before calling cancel(), but cancel() itself
    // never reset it.  If any future C++ code path calls cancel() without
    // first clearing the flag, the cellular warning dialog stays visible.
    // Tie the cellular warning lifecycle to the run lifecycle to prevent
    // a zombie dialog after the run is gone.
    if (_cellularWarnVisible) {
        _cellularWarnVisible = false;
        emit cellularWarnVisibleChanged();
    }

    // DiagnosticTask::cancel() stops the per-task watchdog and suppresses
    // the finished() signal via the atomic cancelled flag. Tasks in-flight
    // will complete silently in the background. No explicit result marking
    // is needed — the UI shows pending tests as cancelled via runStatus.
    // 5WHY: cancel() previously did NOT bump m_runGeneration — only reset()
    // did. A cancelled run's in-flight watchdogs (up to 120s later) still
    // fired finished() with the OLD generation, so their callbacks called
    // onDiagFinished()/m_pendingGroups[..] and clobbered UI state after the
    // run was already cancelled. Bump the generation here so every in-flight
    // lambda sees a stale generation and returns immediately. (reset() also
    // bumps again — harmless, just invalidates twice.)
    m_runGeneration.fetch_add(1, std::memory_order_release);
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

QString AppState::diagIconName(int diagIdInt) const {
    return ::diagIconName(static_cast<DiagId>(diagIdInt));
}

QString AppState::groupIconName(int groupInt) const {
    return ::groupIconName(static_cast<DiagGroup>(groupInt));
}

QString AppState::diagAnimationUrl(int diagIdInt) const {
    // Map DiagAnimType → QRC URL.  Single source — no QML-side switch needed.
    if (!DiagnosticConfig::isValidDiagId(diagIdInt))
        return QStringLiteral("qrc:/qml/widgets/animations/JiggleAnimation.qml");
    switch (::diagAnimationType(static_cast<DiagId>(diagIdInt))) {
        case DiagAnimType::Bounce: return QStringLiteral("qrc:/qml/widgets/animations/BounceAnimation.qml");
        case DiagAnimType::Path:   return QStringLiteral("qrc:/qml/widgets/animations/PathAnimation.qml");
        case DiagAnimType::Pulse:  return QStringLiteral("qrc:/qml/widgets/animations/PulseAnimation.qml");
        case DiagAnimType::Type:   return QStringLiteral("qrc:/qml/widgets/animations/TypeAnimation.qml");
        case DiagAnimType::Lock:   return QStringLiteral("qrc:/qml/widgets/animations/LockAnimation.qml");
        default:                   return QStringLiteral("qrc:/qml/widgets/animations/JiggleAnimation.qml");
    }
}

ReportData AppState::buildReportData() const {
    ReportData d;
    d.target = TargetRedaction::forDisplay(m_targetModel->target()).toHtmlEscaped();
    d.timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    d.appVersion = appVersion();
    d.buildNumber = buildNumber();
    d.gitHash = gitHash();
    d.groupLabels = groupLabels();
    d.results = m_resultsModel->allResults();
    // Pre-translate all diagnostic display names at snapshot time so
    // ReportEngine never depends on the active locale or QML context.
    for (auto id : ::allDiagIds()) {
        d.displayNames[id] = ::diagDisplayName(id);
    }
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

void AppState::probeRestore() {
    m_settingsCtrl->probeRestore();
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
#if defined(PLATFORM_MOBILE)
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

// =============================================================================
// QSettings persistence — survives app restarts
// =============================================================================

static constexpr const char* kSettingsGroup = "AppSettings";

void AppState::loadSettings() {
    QSettings s;
    // 5WHY: AppState persisted under group "settings" while the Controllers
    // use "AppSettings" — two writers to the same QSettings file used
    // different groups. Migrate any legacy "settings/activeGroups" value
    // into the canonical group once, then read from it.
    s.beginGroup(QStringLiteral("settings"));
    QVariantList legacyAg = s.value("activeGroups").toList();
    s.endGroup();

    s.beginGroup(QString::fromLatin1(kSettingsGroup));
    QVariantList ag = s.value("activeGroups").toList();
    if (ag.isEmpty() && !legacyAg.isEmpty()) {
        ag = legacyAg;
        s.setValue("activeGroups", ag);
    }

    // Active groups (G1-G5 shown/hidden)
    if (!ag.isEmpty()) {
        m_activeGroups.clear();
        for (const auto& v : ag)
            m_activeGroups.insert(v.toInt());
    }

    s.endGroup();
}

void AppState::saveSettings() {
    QSettings s;
    s.beginGroup(QString::fromLatin1(kSettingsGroup));

    QVariantList ag;
    for (int g : m_activeGroups) ag.append(g);
    s.setValue("activeGroups", ag);

    s.endGroup();
}

