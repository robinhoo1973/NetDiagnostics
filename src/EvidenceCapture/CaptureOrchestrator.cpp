// =============================================================================
// CaptureOrchestrator.cpp — Master orchestrator implementation
// =============================================================================

// 5WHY: This file is only compiled on iOS/Android (CMakeLists.txt gates
// capture sources behind IOS OR ANDROID).  Enforce the invariant here so
// a future desktop build attempt fails with a clear message instead of
// silent runtime misbehavior (e.g., skipping the ffmpeg check).
#if !defined(PLATFORM_MOBILE)
#error CaptureOrchestrator is currently mobile-only (see CMakeLists.txt ios/android guard)
#endif

#include "EvidenceCapture/CaptureOrchestrator.h"
#include "EvidenceCapture/CaptureScenario.h"
#include "EvidenceCapture/NavigationAdapter.h"
#include "EvidenceCapture/ScrollController.h"
#include "Common/Model/CaptureFeatureGate.h"
#include "Common/Platform/PlatformCapture.h"
#include "Common/Platform/PlatformRecording.h"
#include "Common/Platform/PlatformKeepAwake.h"
#include "Common/Platform/PlatformFocus.h"
#include "app/AppState.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QDateTime>
#include <QCoreApplication>
#include <QDebug>
#include <QUrl>
#include <QStorageInfo>
#include <QSysInfo>

void CaptureOrchestrator::restoreSystemState() {
    // 5WHY: platformSetKeepAwake(false) was still called separately at every
    // terminal-state handler (5 sites) even after the other three restore calls
    // were centralized.  Adding it here guarantees no future terminal state
    // forgets to release the wakelock — battery-drain regression prevention.
    platformSetKeepAwake(false);
    if (!platformRestoreBrightness()) {
        qWarning() << "CaptureOrchestrator: brightness restore failed or was never set";
    }
    // 5WHY: Restore orientation BEFORE disabling focus mode.  If DND is
    // disabled first, queued notifications flood in while the screen is
    // still locked to capture orientation — rendering at the wrong
    // orientation and causing visual glitches when orientation is then
    // unlocked.  Orient → DND = notifications arrive at user's orientation.
    // 5WHY: platformUnlockOrientation is idempotent on Android
    // (setRequestedOrientation(USER) restores the default).  iOS now
    // returns true when unlocking succeeded, false when orientation was
    // never locked (sentinel guard).  Check return for diagnostics.
    if (!platformUnlockOrientation()) {
        qWarning() << "CaptureOrchestrator: orientation unlock failed or was never locked";
    }
    if (!platformDisableFocusMode()) {
        qWarning() << "CaptureOrchestrator: focus-mode restore failed or was never enabled";
    }
    // 5WHY: needsFocusModeSetupChanged was emitted unconditionally in
    // restoreSystemState(), which fires from terminal states (Completed,
    // Failed, Cancelled).  At that point the preflight overlay (the only
    // consumer of needsFocusModeSetup) is gone — the signal was a no-op.
    // Only emit if the FSM is still running (overlays could be visible).
    if (m_stateMachine->isRunning()) {
        emit needsFocusModeSetupChanged();
    }

    // 5WHY: Always force-stop the platform recording when m_recording is
    // true, even if platformIsRecording() returns false.  A cancel during
    // StartingRecording (before the first video frame arrives) leaves the
    // ReplayKit/MediaProjection capture handler active without a
    // corresponding stop — s_recording is false, so platformIsRecording()
    // returns false, and the guard would skip the stop call.  The frame
    // handler then fires on the next video frame, sets s_recording=true,
    // and the recording runs indefinitely with no C++ tracking.
    //
    // platformStopRecording(nullptr) handles the !s_recording case correctly:
    // it sets s_stopping=true and clears s_startCb (see PlatformRecording.mm
    // lines ~233, ~266-272).  This causes the frame handler to discard
    // frames (if (s_stopping) return at line 187) and prevents the recording
    // from ever starting — exactly what we want.
    if (m_recording) {
        platformStopRecording(nullptr);
    }
    m_recording = false;
}

// 5WHY: Manifest and execution log wrote raw m_diagUrl without sanitization,
// risking credential leakage (user:pass@host) and token exposure (query params).
// Centralize the sanitization here so all data-exfiltration paths go through
// one well-audited function.
static QString sanitizeUrl(const QString& raw) {
    QUrl u(raw);
    // 5WHY: RemoveUserInfo flag on toString() already strips both user
    // and password — setPassword() was dead code. Only query needs manual
    // stripping since no QUrl::RemoveQuery flag exists.
    u.setQuery(QString());
    // Keep scheme + host + port + path — enough for audit but not for replay.
    return u.toString(QUrl::RemoveUserInfo | QUrl::PrettyDecoded);
}

// 5WHY: Colon-to-dash sanitization for Windows-safe filenames was copy-pasted
// in two branches of executeStep (captureBefore + Capture).  Extract once.
// Broadened to cover all characters illegal in Windows filenames so a future
// step description containing <, >, ", /, \, |, ?, or * doesn't cause a
// silent file-creation failure when output is copied to a Windows share.
static QString sanitizeFilename(const QString& raw) {
    QString s = raw;
    // 5WHY: Single-pass character switch — O(n) instead of O(9n) from 9
    // sequential s.replace() calls, each doing a full string scan even when
    // the illegal character is absent (the common case for 8 of 9 chars).
    for (int i = 0; i < s.size(); ++i) {
        switch (s[i].unicode()) {
        case ':': case '/': case '\\': case '<': case '>':
        case '"': case '|': case '?': case '*':
            s[i] = QLatin1Char('-');
            break;
        }
    }
    // Trim trailing dots and spaces (Windows restriction: Explorer strips them,
    // but the filesystem rejects them if they're the only trailing chars).
    // 5WHY: while-loop with endsWith() + chop(1) was O(n^2) — each endsWith()
    // rescanned the string.  Find the trim point in one reverse scan (O(n)).
    int end = s.size() - 1;
    while (end >= 0 && (s[end] == QLatin1Char('.') || s[end] == QLatin1Char(' '))) {
        --end;
    }
    s.truncate(end + 1);
    if (s.isEmpty()) s = QStringLiteral("capture");
    return s;
}

// 5WHY: "/Metadata/manifest.json", "/Logs/execution.log", and subdirectory names
// were bare string literals repeated 3–4 times each.  A rename would require a
// grep-and-replace across every site — easy to miss one.  Centralize once.
static const QString kManifestRelPath = QStringLiteral("/Metadata/manifest.json");
static const QString kExecLogRelPath  = QStringLiteral("/Logs/execution.log");
static const QString kSubdirScreenshots = QStringLiteral("/Screenshots");
static const QString kSubdirVideos      = QStringLiteral("/Videos");
static const QString kSubdirLogs        = QStringLiteral("/Logs");
static const QString kSubdirMetadata    = QStringLiteral("/Metadata");

// 5WHY: Magic numbers for timing delays were bare integer literals scattered
// across 5+ call sites.  Named constants let you tune capture pacing globally.
static const int kStepDeferMs            = 100;   // async deferral to avoid infinite recursion
static const int kReportPreviewTimeoutMs = 5000;  // openReportPreview safety fallback
static const int kPreviewRenderSettleMs  = 1000;  // QML overlay render settle before screenshot
static const int kCountdownSafetyMs      = 10000; // preflight-countdown safety-net timeout
static const int kCountdownFallbackMs    = 120000; // fallback if QML never confirms (iOS DND setup abandon)
static const int kNavigateSettleMs       = 500;   // page settle after navigate (screenshot mode)
static const int kNavigateSettleRecordMs = 4000;  // page settle after navigate (recording mode)
static const int kDiagCancelTimeoutMs    = 500;   // settle after diagnostic cancel on timeout
static const int kOpenDetailSettleMs     = 1500;  // StackView settle after tab switch for OpenDetail
static const int kRecordingStartTimeoutMs = 30000; // platform recording start safety timeout

// ═════════════════════════════════════════════════════════════════════════════
// Constructor
// ═════════════════════════════════════════════════════════════════════════════

CaptureOrchestrator::CaptureOrchestrator(AppState* appState, QObject* parent)
    : QObject(parent)
    , m_appState(appState)
    , m_stateMachine(new CaptureStateMachine(this))
    , m_navAdapter(new NavigationAdapter(nullptr, m_appState, this))
    , m_scrollCtrl(new ScrollController(this))
    , m_delayTimer(new QTimer(this))
{
    m_delayTimer->setSingleShot(true);
    // Single permanent connection — eliminate disconnect+reconnect churn
    // on every Navigate/WaitPageReady step (scheduleStepAfter now just starts).
    connect(m_delayTimer, &QTimer::timeout, this, &CaptureOrchestrator::executeNextStep);

    connect(m_stateMachine, &CaptureStateMachine::stateChanged,
            this, &CaptureOrchestrator::onStateChanged);
    connect(m_scrollCtrl, &ScrollController::scrollFinished,
            this, &CaptureOrchestrator::onStepScrollFinished);
    connect(m_navAdapter, &NavigationAdapter::reportPreviewReady,
            this, &CaptureOrchestrator::onReportPreviewReady);
}

CaptureOrchestrator::~CaptureOrchestrator() {
    // 5WHY: cancel() already stops recording (fire-and-forget async stop).
    // m_recording is cleared in restoreSystemState(), not in cancel(),
    // to prevent the starting-recording race (see cancel() comment).
    if (m_stateMachine->isRunning()) {
        cancel();  // triggers restoreSystemState() via onStateChanged(Cancelled)
    } else {
        // 5WHY: Safety net — if cancel() wasn't called (FSM already terminal
        // or never started), restore system state here so the device is not
        // left in capture mode after object teardown.
        restoreSystemState();
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// QML API
// ═════════════════════════════════════════════════════════════════════════════

int CaptureOrchestrator::stateInt() const {
    return m_stateMachine->stateInt();
}

void CaptureOrchestrator::setAppContent(QObject* appContent) {
    m_navAdapter->setAppContent(appContent);
}

void CaptureOrchestrator::setScrollFlickable(QObject* flickable) {
    m_scrollCtrl->setFlickable(flickable);
}

void CaptureOrchestrator::openFocusSettings() {
    platformOpenFocusSettings();
}

bool CaptureOrchestrator::needsFocusModeSetup() const {
    // 5WHY: The preflight overlay always shows the DND hint, but on Android
    // where DND can be enabled programmatically (platformEnableFocusMode()),
    // showing the hint when DND is already active is misleading.  On iOS,
    // programmatic Focus mode is unavailable — platformEnableFocusMode() only
    // mutes audio — so the hint must always appear.
#if defined(PLATFORM_IOS)
    // iOS: no programmatic Focus mode API — user must always be reminded.
    return true;
#elif defined(PLATFORM_ANDROID)
    // Android: platformEnableFocusMode() may succeed if the user granted
    // ACCESS_NOTIFICATION_POLICY.  Show the hint only when DND is NOT active.
    return !platformIsFocusModeEnabled();
#else
    // Desktop: no DND support — hint is irrelevant.
    return false;
#endif
}

bool CaptureOrchestrator::supportsBothModes() const {
    // 5WHY: Delegate to the platform layer which owns the capability
    // knowledge.  Each platform's PlatformRecording.cpp returns the
    // ground truth for whether simultaneous recording+screenshot is
    // possible.  Avoids #ifdef fragility and keeps the orchestrator
    // platform-agnostic.
    return platformSupportsScreenshotWhileRecording();
}

QString CaptureOrchestrator::captureBasePath() {
    // 5WHY: iOS log files (debug.log, crash.log) live under
    // Documents/NetDiagnostics/ — co-locating Capture output there
    // makes all diagnostic evidence discoverable in one place via Files.app.
    // Desktop: AppDataLocation; iOS: DocumentsLocation (matched to Logger.cpp).
    //
    // 5WHY: PLATFORM_IOS (not PLATFORM_MOBILE) is used here because iOS and
    // Android genuinely differ on this path — iOS must use DocumentsLocation
    // for Files.app discoverability, while Android uses AppDataLocation.
    // The file's #error guard ensures the #else branch only hits Android.
    //
    // 5WHY: Called twice during session startup.  Cache in a static local —
    // the writable location never changes during the app's lifetime.
    static QString path;
    if (path.isEmpty()) {
#if defined(PLATFORM_IOS)
        path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#else
        path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#endif
    }
    return path;
}

void CaptureOrchestrator::failCapture(const QString& errorCode, const QString& message) {
    // 5WHY: Transition FSM FIRST so onStateChanged(Failed) runs
    // restoreSystemState() BEFORE the captureFailed signal fires.
    // A QML handler reading captureOrchestrator.state synchronously
    // from captureFailed would otherwise see the pre-Failed state.
    m_stateMachine->transitionTo(CaptureState::Failed);
    emit captureFailed(errorCode, message);
}

void CaptureOrchestrator::requestModeSelection() {
    // 5WHY: was calling CaptureFeatureGate::setFeatureEnabled(true) directly,
    // bypassing AppState::enableCaptureFeature().  The captureFeatureChanged
    // signal was never emitted, so the QML binding appState.captureFeatureEnabled
    // stayed stale — the Settings icon glow/dot indicator didn't update after
    // the first double-click.  Route through AppState so the NOTIFY signal fires
    // and the QML bindings re-evaluate.
    if (!CaptureFeatureGate::isFeatureEnabled()) {
        if (m_appState) {
            m_appState->enableCaptureFeature();
        } else {
            CaptureFeatureGate::setFeatureEnabled(true);
        }
    }
    emit modeSelectionRequested();
}

void CaptureOrchestrator::notifyCountdownStarted() {
    // 5WHY: The C++ safety timer was previously registered in
    // onStateChanged(CountdownToStart), which fires BEFORE the QML
    // countdown actually begins.  On iOS, the user must manually enable
    // Focus mode first — opening Settings, enabling DND, returning —
    // which can take 30-60 seconds.  The old 10s safety timer would fire
    // while the user was still in Settings, prematurely advancing the FSM
    // to CreatingSession → StartingRecording (triggering the ReplayKit
    // permission dialog).  Instead, start the safety timer only when the
    // QML countdown timer actually begins (after Focus confirmation).
    // On Android, QML calls this immediately when the preflight loads.
    int gen = ++m_countdownGen;
    QTimer::singleShot(kCountdownSafetyMs, this, [this, gen]() {
        if (m_countdownGen == gen
            && m_stateMachine->state() == CaptureState::CountdownToStart) {
            qWarning() << "CaptureOrchestrator: countdown safety timer fired — "
                           "QML overlay may have failed to load";
            m_stateMachine->transitionTo(CaptureState::CreatingSession);
        }
    });
}

void CaptureOrchestrator::onCountdownFinished() {
    // 5WHY: Called by QML when the visual 3-2-1 countdown reaches 0.
    // Only valid from CountdownToStart state.
    if (m_stateMachine->state() == CaptureState::CountdownToStart) {
        m_stateMachine->transitionTo(CaptureState::CreatingSession);
    }
}

void CaptureOrchestrator::startCapture(int captureMode, const QString& diagUrl) {
    if (!CaptureFeatureGate::isFeatureEnabled()) {
        emit captureFailed(QStringLiteral("FEATURE_DISABLED"),
                           QStringLiteral("Capture feature is disabled. Double-click "
                                          "the app icon in Settings > About to enable."));
        return;
    }
    if (m_stateMachine->isRunning()) return;
    if (diagUrl.trimmed().isEmpty()) {
        emit captureFailed(QStringLiteral("NO_URL"),
                           QStringLiteral("Please enter a diagnostic URL."));
        return;
    }

    // 5WHY: After a prior capture reaches Completed/Cancelled/Failed (terminal),
    // the transition table has no path back to Preflight. reset() returns the
    // machine to Idle, from which Preflight IS a valid transition.
    if (m_stateMachine->isTerminal()) {
        m_stateMachine->reset();
    }

    m_captureMode = captureMode;
    m_diagUrl = diagUrl.trimmed();
    m_recording = (captureMode == RecordingOnly || captureMode == Both);
    m_currentStep = 0;
    m_captureCount = 0;
    m_sessionDir.clear();
    m_currentAction.clear();  // 5WHY: stale action from a previous capture (e.g.
    // "Recording saved: ...") would be briefly visible in the QML overlay until
    // the first onStateChanged → CountdownToStart sets a new action.  Clear it
    // at the start so the UI shows an empty state until the new action is set.
    // 5WHY: If the previous session was cancelled during OpenReport before the
    // safety timeout fired, m_waitingForReportPreview was left true — a new
    // session reaching OpenReport would save a second safety timeout while the
    // stale one was still pending, allowing the stale timer to prematurely
    // advance the new session.  Reset it so each session starts with a clean
    // report-preview wait state.
    m_waitingForReportPreview = false;
    m_elapsed.start();

    // Build and cache the scenario once, filtered by capture mode
    CaptureScenario scenario = buildDefaultScenario(m_diagUrl);
    m_filteredScenario.clear();
    for (const auto& step : scenario.steps()) {
        if (step.recordingOnly && !m_recording) continue;
        m_filteredScenario.addStep(step);
    }
    m_totalSteps = m_filteredScenario.stepCount();

    m_stateMachine->transitionTo(CaptureState::Preflight);
}

void CaptureOrchestrator::cancel() {
    if (!m_stateMachine->isRunning()) return;

    // 5WHY: Stop all active timers to prevent stale callbacks after cancel.
    // m_delayTimer (inter-step settle delay) and m_pollTimer (WaitDiagComplete
    // polling) must be stopped so they don't fire after the state machine has
    // left ExecutingSteps.  While executeNextStep() guards against wrong state,
    // stopping timers here avoids wasted work and prevents potential re-entrancy
    // from timer callbacks interleaving with cleanup.
    m_delayTimer->stop();
    if (m_pollTimer) {
        m_pollTimer->stop();
        m_pollTimer->deleteLater();
        m_pollTimer = nullptr;
    }
    // 5WHY: If cancel is called during OpenReport, m_waitingForReportPreview
    // was left true.  The NavigationAdapter's 500ms deferred timer would still
    // fire reportPreviewReady → onReportPreviewReady → schedule a 1s-delayed
    // executeNextStep with a stale FSM state.  Reset it so the stale callback
    // returns immediately (its guard checks this flag first).
    m_waitingForReportPreview = false;

    // 5WHY: cancel() stopped m_delayTimer and m_pollTimer, but did NOT
    // increment m_countdownGen.  Stale QTimer::singleShot callbacks from
    // OpenDetail (1500ms, executeStep line ~910) and OpenReport (5000ms,
    // executeStep line ~935) use m_countdownGen as a stale-session guard.
    // Since cancel() left m_countdownGen unchanged, those callbacks would
    // fire and call openDiagnosticDetail() + restart m_delayTimer on a
    // cancelled session — a UI glitch where detail overlays appear briefly
    // after the user cancelled.  Incrementing the generation counter here
    // invalidates all outstanding callbacks in one atomic operation.
    ++m_countdownGen;

    // 5WHY: platformStopRecording(nullptr) initiates an async stop.
    // Do NOT clear m_recording here — if the recording hasn't started yet
    // (e.g. cancel during StartingRecording, before the platform callback
    // fires), the recording may start after this guard and would never be
    // stopped.  restoreSystemState() is the single point that clears
    // m_recording AFTER its own safety-net force-stop check.
    if (m_recording && platformIsRecording()) {
        platformStopRecording(nullptr);
    }

    // Stop any in-progress scroll
    m_scrollCtrl->cancel();

    // 5WHY: transitionTo(Cancelled) is not valid from every state
    // (Idle and terminal states reject it — see FSM table).
    // StoppingRecording and Finalizing both accept Cancelled per the
    // FSM table (verified in CaptureStateMachine.cpp:46-48).
    // Only emit captureCancelled and disable keep-awake if the FSM
    // accepted the transition.  If rejected, the FSM continues and its
    // terminal handlers will disable keep-awake.
    bool didCancel = m_stateMachine->transitionTo(CaptureState::Cancelled);
    if (didCancel) {
        // 5WHY: restoreSystemState() is called in onStateChanged(Cancelled)
        // (line ~410) — the transitionTo above triggers it synchronously.
        // Calling it here too would invoke platformUnlockOrientation /
        // platformDisableFocusMode twice (JNI round-trips and warnings).
        emit captureCancelled();
    }
    // If transition was rejected, the FSM stays in its current state and
    // will complete normally (or fail with a timeout).
}

// ═════════════════════════════════════════════════════════════════════════════
// State machine handler — drives the entire workflow
// ═════════════════════════════════════════════════════════════════════════════

void CaptureOrchestrator::onStateChanged(int from, int to) {
    Q_UNUSED(from);

    CaptureState s = static_cast<CaptureState>(to);

    switch (s) {
    case CaptureState::Preflight:
        runPreflight();
        break;

    case CaptureState::CountdownToStart:
        qInfo() << "CaptureOrchestrator: entering CountdownToStart — "
                   "setting up keepAwake, focus, brightness, orientation";
        // 5WHY: was using QTimer::singleShot(3000) independent of the QML
        // countdown — two timers racing. Now the QML countdown is the sole
        // authority: CapturePreflightOverlay.countdownFinished() calls
        // onCountdownFinished() below, which transitions to CreatingSession.
        m_currentAction = QStringLiteral("Preparing to capture...");
        emit actionChanged(m_currentAction);
        platformSetKeepAwake(true);
        // 5WHY: DND is best-effort — on Android it requires the user to grant
        // ACCESS_NOTIFICATION_POLICY permission in Settings.  If the permission
        // wasn't granted, warn so the user isn't surprised when notifications
        // interrupt the capture.  Don't fail the capture — DND is a nice-to-have.
        if (!platformEnableFocusMode()) {
            qWarning() << "CaptureOrchestrator: Focus/DND mode unavailable — "
                           "notifications may interrupt the capture.  Grant "
                           "notification policy access in system Settings.";
        }
        // 5WHY: needsFocusModeSetup was declared CONSTANT but its value changes
        // on Android after platformEnableFocusMode() toggles s_focusEnabled.
        // Emit the NOTIFY signal so the QML visible binding on the DND hint
        // label re-evaluates and hides the hint once DND is active.
        emit needsFocusModeSetupChanged();
        // 5WHY: All three platform functions now return bool — warn if any
        // preflight setup fails so the user/developer knows why the recording
        // is dim, the orientation shifts, or notifications interrupt capture.
        if (!platformSetMaxBrightness()) {
            qWarning() << "CaptureOrchestrator: cannot set max screen brightness";
        }
        if (!platformLockOrientation()) {
            qWarning() << "CaptureOrchestrator: orientation lock unavailable "
                           "— device rotation may disrupt the recording";
        }
        // 5WHY: Safety timer is now registered by notifyCountdownStarted(),
        // called from QML when the actual countdown begins.  On iOS this is
        // after the user confirms Focus mode; on Android it fires immediately
        // when the preflight overlay loads.  In both cases, the timer is
        // a generation-guarded fallback for QML overlay load failure.
        //
        // 5WHY (round-31): If the QML preflight overlay fails to load, or
        // the iOS user walks away without confirming Focus mode,
        // notifyCountdownStarted() is never called — no safety timer runs.
        // The FSM would hang in CountdownToStart forever.  Add a fallback
        // timer (120s) here that fires only if notifyCountdownStarted() was
        // never invoked (gen counters won't have diverged).  When QML calls
        // notifyCountdownStarted(), it increments m_countdownGen, which
        // invalidates this fallback timer — so it never races with the
        // shorter 10s timer in notifyCountdownStarted().
        {
            int fallbackGen = m_countdownGen;
            QTimer::singleShot(kCountdownFallbackMs, this, [this, fallbackGen]() {
                if (m_countdownGen == fallbackGen
                    && m_stateMachine->state() == CaptureState::CountdownToStart) {
                    qWarning() << "CaptureOrchestrator: countdown fallback timer fired — "
                                  "QML preflight may have failed to load or user abandoned Focus setup";
                    m_stateMachine->transitionTo(CaptureState::CreatingSession);
                }
            });
        }
        break;

    case CaptureState::CreatingSession:
        createSession();
        // 5WHY: createSession() can transition to Failed (STORAGE_ERROR).
        // If it did, we must NOT override that with a spurious transition
        // to StartingRecording/ExecutingSteps.  All terminal states reject
        // incoming transitions, but the code path is semantically wrong.
        if (m_stateMachine->state() != CaptureState::CreatingSession) return;
        if (m_recording) {
            m_stateMachine->transitionTo(CaptureState::StartingRecording);
        } else {
            m_stateMachine->transitionTo(CaptureState::ExecutingSteps);
        }
        break;

    case CaptureState::StartingRecording:
        qInfo() << "CaptureOrchestrator: entering StartingRecording";
        startPlatformRecording();
        // 5WHY: StartingRecording is an async state — the FSM only advances
        // when the platform recording callback fires.  If the callback is
        // never invoked (AVAssetWriter enters unexpected status, JNI
        // exception drops callback, ReplayKit permission dialog cancelled),
        // the FSM would hang forever.  Add a 30s safety timeout consistent
        // with the other async-state timeouts in this file (CountdownToStart,
        // OpenReport, Navigate, WaitDiagComplete all have timeouts).
        {
            int gen = m_countdownGen;
            QTimer::singleShot(kRecordingStartTimeoutMs, this, [this, gen]() {
                if (m_countdownGen == gen
                    && m_stateMachine->state() == CaptureState::StartingRecording) {
                    qWarning() << "CaptureOrchestrator: recording start timeout — "
                                  "platform callback never fired, aborting";
                    // Force-stop the platform recording in case it partially started
                    if (m_recording) platformStopRecording(nullptr);
                    failCapture(QStringLiteral("RECORDING_START_TIMEOUT"),
                                QStringLiteral("Screen recording failed to start within 30 seconds"));
                }
            });
        }
        break;

    case CaptureState::ExecutingSteps:
        qInfo() << "CaptureOrchestrator: entering ExecutingSteps — "
                   << m_totalSteps << " steps to execute";
        executeNextStep();
        break;

    case CaptureState::StoppingRecording:
        stopPlatformRecording();
        break;

    case CaptureState::Finalizing:
        finalizeSession();
        break;

    case CaptureState::Completed:
        restoreSystemState();
        // Loader — onStateChanged sets source="CaptureResultSummary.qml"
        // which loads asynchronously, but captureCompleted fires in the
        // same event-loop iteration before the Loader finishes.  Defer the
        // signal so the Loader has time to begin incubation and the
        // onCaptureCompleted handler finds the ResultSummary item.
        QTimer::singleShot(0, this, [this]() {
            emit captureCompleted(m_sessionDir);
        });
        break;

    case CaptureState::Failed:
        restoreSystemState();
        // Specific error code the phase handler already emitted (e.g.
        // "NO_FFMPEG", "RECORDING_FAILED"). The Failed handler only
        // restores system state; the specific error was already emitted.
        break;

    case CaptureState::Cancelled:
        // restoreSystemState() includes keepAwake(false) — no need for a
        // separate call here even when the Cancelled transition was triggered
        // from a non-cancel() path (e.g. FSM auto-transition on error).
        restoreSystemState();
        break;

    default:
        break;
    }

    // 5WHY: Emit stateChanged AFTER the C++ case body so QML handlers see
    // the fully-configured state.  Previously emitted before the switch,
    // which meant onStateChanged(CountdownToStart) fired QML's preflight
    // overlay load BEFORE platformSetKeepAwake/SetMaxBrightness/LockOrientation
    // and the safety timer were set up — a 3s countdown could finish before
    // the 10s safety net was registered.
    emit stateChanged();
}

// ═════════════════════════════════════════════════════════════════════════════
// Phase implementations
// ═════════════════════════════════════════════════════════════════════════════

void CaptureOrchestrator::runPreflight() {
    // 5WHY: Synchronous QProcess::waitForFinished(3000) blocked the main
    // (UI) thread for up to 3 seconds.  Use Qt's findExecutable() which
    // searches PATH without spawning a process — faster and portable
    // (doesn't rely on the external `which` command).
    //
    // 5WHY: The ffmpeg check was platform-agnostic but ffmpeg is only
    // needed on desktop platforms (Linux/macOS/Windows).  iOS uses
    // ReplayKit and Android uses MediaProjection — neither needs ffmpeg.
    // Checking for ffmpeg on mobile platforms is a false negative that
    // blocks recording mode entirely.
    // 5WHY: This file is only compiled on iOS/Android (CMakeLists.txt gates
    // capture sources behind IOS OR ANDROID).  Mobile platforms use native
    // recording APIs (ReplayKit / MediaProjection) — no ffmpeg check needed.
    // platformStartRecording() reports any errors via its callback.

    finishPreflight();
}

void CaptureOrchestrator::finishPreflight() {
    // 5WHY: If the FSM left Preflight (cancelled, failed) while the async
    // ffmpeg check ran, bail out — don't continue the preflight from a
    // stale state.
    if (m_stateMachine->state() != CaptureState::Preflight) return;

    // Check disk space (>100MB) — use same base path as createSession()
    QStorageInfo storage(captureBasePath());
    if (storage.isValid() && storage.bytesAvailable() < 100 * 1024 * 1024) {
        failCapture(QStringLiteral("LOW_DISK"),
                    QStringLiteral("Less than 100MB disk space available."));
        return;
    }

    m_stateMachine->transitionTo(CaptureState::CountdownToStart);
}

void CaptureOrchestrator::createSession() {
    QString modeStr;
    switch (m_captureMode) {
    case ScreenshotOnly: modeStr = QStringLiteral("Screenshot"); break;
    case RecordingOnly:  modeStr = QStringLiteral("Video");      break;
    case Both:           modeStr = QStringLiteral("Video+Screenshot"); break;
    default:             modeStr = QStringLiteral("Unknown");    break;
    }

    const QDateTime now = QDateTime::currentDateTime();
    QString ts = now.toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    QString base = captureBasePath();
    QString root = base + QStringLiteral("/NetDiagnostics/Capture");
    m_sessionDir = root + QStringLiteral("/") + ts;

    // 5WHY: Four sequential mkpath() calls each re-verify the full parent
    // chain (m_sessionDir with ~4 path components), causing ~12-16 redundant
    // stat() syscalls on the capture startup hot path.  Create m_sessionDir
    // once via mkpath, then create each subdirectory via mkdir (single stat).
    QDir dir;
    if (!dir.mkpath(m_sessionDir)
        || !dir.mkdir(m_sessionDir + kSubdirScreenshots)
        || !dir.mkdir(m_sessionDir + kSubdirVideos)
        || !dir.mkdir(m_sessionDir + kSubdirLogs)
        || !dir.mkdir(m_sessionDir + kSubdirMetadata)) {
        failCapture(QStringLiteral("STORAGE_ERROR"), QStringLiteral("Cannot create session directory"));
        return;
    }

    // Write initial manifest.json (in Metadata/ per design §10)
    // 5WHY: QSysInfo::machineHostName() calls gethostname() (syscall),
    // productType/Version may read /etc/os-release.  These are immutable
    // for the process lifetime — cache in static locals.
    static const QString s_hostName = QSysInfo::machineHostName();
    static const QString s_osInfo   = QSysInfo::productType() + QStringLiteral(" ") + QSysInfo::productVersion();

    QJsonObject manifest;
    manifest["session_id"] = ts;
    manifest["capture_mode"] = modeStr;
    QString safeUrl = sanitizeUrl(m_diagUrl);
    manifest["diag_url"] = safeUrl;
    manifest["started_at"] = now.toString(Qt::ISODate);
    manifest["status"] = "running";
    manifest["device"] = s_hostName;
    manifest["os"] = s_osInfo;
    manifest["captures"] = QJsonArray();

    QFile mf(m_sessionDir + kManifestRelPath);
    if (!mf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        failCapture(QStringLiteral("STORAGE_ERROR"),
                    QStringLiteral("Cannot write session manifest"));
        return;
    }
    if (mf.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented)) == -1) {
        failCapture(QStringLiteral("STORAGE_ERROR"),
                    QStringLiteral("Failed to write session manifest"));
        return;
    }

    // Write execution log header
    // 5WHY: open() failure was silently ignored — if the log file cannot be
    // created (disk full, permissions, path too long), the capture proceeded
    // with no execution log and no warning.  Log a warning so the developer
    // can diagnose why the execution log is missing from the session directory.
    QFile logFile(m_sessionDir + kExecLogRelPath);
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream logStream(&logFile);
        logStream << "=== Automated Capture Session ===\n"
                  << "Session:  " << ts << "\n"
                  << "Mode:     " << modeStr << "\n"
                  << "URL:      " << safeUrl << "\n"
                  << "Started:  " << now.toString(Qt::ISODate) << "\n\n";
        // 5WHY: QTextStream buffers writes — operator<< failures are only
        // detected by checking status() after the writes.  Without this check,
        // a disk-full condition during header writing is silently swallowed
        // (consistent with finalizeSession() which also checks stream status).
        if (logStream.status() != QTextStream::Ok) {
            qWarning() << "CaptureOrchestrator: failed to write execution log header";
        }
    } else {
        qWarning() << "CaptureOrchestrator: cannot open execution log for writing:"
                   << (m_sessionDir + kExecLogRelPath);
    }

    m_currentAction = QStringLiteral("Session created: ") + m_sessionDir;
    emit actionChanged(m_currentAction);
}

void CaptureOrchestrator::startPlatformRecording() {
    QString recPath = m_sessionDir + kSubdirVideos + QStringLiteral("/Capture_AutoDemo");

    m_currentAction = QStringLiteral("Starting screen recording...");
    emit actionChanged(m_currentAction);

    platformStartRecording(recPath, [this](bool ok, const QString& pathOrError) {
        if (ok) {
            qInfo() << "CaptureOrchestrator: recording started — path:" << pathOrError;
            m_currentAction = QStringLiteral("Recording started");
            emit actionChanged(m_currentAction);
            m_stateMachine->transitionTo(CaptureState::ExecutingSteps);
        } else {
            qWarning() << "CaptureOrchestrator: recording start FAILED:" << pathOrError;
            failCapture(QStringLiteral("RECORDING_FAILED"), pathOrError);
        }
    });
}

void CaptureOrchestrator::stopPlatformRecording() {
    m_currentAction = QStringLiteral("Stopping recording...");
    emit actionChanged(m_currentAction);

    platformStopRecording([this](bool ok, const QString& pathOrError) {
        if (ok) {
            m_currentAction = QStringLiteral("Recording saved: ") + pathOrError;
            emit actionChanged(m_currentAction);
            m_stateMachine->transitionTo(CaptureState::Finalizing);
        } else {
            // 5WHY: Previously always transitioned to Finalizing even on
            // recording stop failure, causing finalizeSession() to mark
            // the session as "completed" with a missing/corrupt recording.
            // Now transitions to Failed so the manifest reflects reality.
            m_currentAction = QStringLiteral("Recording stop failed: ") + pathOrError;
            emit actionChanged(m_currentAction);
            failCapture(QStringLiteral("RECORDING_STOP_FAILED"), pathOrError);
        }
    });
}

// Forward declaration: defined after appendToManifest (file-scope static).
static bool atomicWriteManifest(const QString& manifestPath, const QJsonObject& obj);

void CaptureOrchestrator::finalizeSession() {
    // 5WHY: the success parameter was dead code after stopPlatformRecording
    // refactored to transition directly to Failed on error. Recording stop
    // failures and all other errors now transition to Failed state directly,
    // so finalizeSession only handles the success path.  The corrupt-manifest
    // fallback below is the only remaining failure path through this function.
    // Update manifest
    // 5WHY: Use the same atomic temp-file-then-rename pattern as
    // appendToManifest().  A crash between resize(0) and write() in the
    // old in-place approach left a zero-byte manifest — losing all
    // journaled capture entries even though the PNG files were intact.
    const QString manifestPath = m_sessionDir + kManifestRelPath;
    QFile mf(manifestPath);
    if (!mf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        failCapture(QStringLiteral("MANIFEST_ERROR"),
                    QStringLiteral("Cannot open session manifest for finalization"));
        return;
    }
    QByteArray data = mf.readAll();
    // 5WHY: readAll() returns whatever was readable — on a failing
    // storage medium the returned data may be truncated without
    // QFile signalling an error synchronously.  Check the file's
    // error state so we don't silently finalize with partial data
    // that happens to parse as valid-but-incomplete JSON.
    if (mf.error() != QFile::NoError) {
        failCapture(QStringLiteral("MANIFEST_ERROR"),
                    QStringLiteral("Cannot read session manifest for finalization"));
        return;
    }
    mf.close();  // release the file handle before rename

    QJsonDocument doc = QJsonDocument::fromJson(data);
    // 5WHY: just returning leaves FSM stuck in Finalizing forever.
    // Corrupt manifest means the session directory is compromised —
    // transition to Failed so the user can retry.
    if (doc.isNull()) {
        // 5WHY: QFile destructor closes via RAII — explicit close() is dead code.
        failCapture(QStringLiteral("MANIFEST_CORRUPT"), QStringLiteral("Session manifest is corrupt, cannot finalize."));
        return;
    }
    QJsonObject obj = doc.object();
    obj["completed_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    obj["status"] = QStringLiteral("completed");
    obj["total_captures"] = m_captureCount;
    obj["duration_s"] = elapsedSeconds();

    if (!atomicWriteManifest(manifestPath, obj)) {
        failCapture(QStringLiteral("MANIFEST_ERROR"),
                    QStringLiteral("Cannot write finalized manifest"));
        return;
    }

    // Append to execution log
    QFile logFile(m_sessionDir + kExecLogRelPath);
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream logStream(&logFile);
        logStream << "\n--- Session complete ---\n"
                   << "Total captures: " << m_captureCount << "\n"
                   << "Ended: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
        if (logStream.status() != QTextStream::Ok) {
            qWarning() << "CaptureOrchestrator: failed to append execution log";
        }
    }

    m_stateMachine->transitionTo(CaptureState::Completed);
}

// ═════════════════════════════════════════════════════════════════════════════
// Step execution engine
// ═════════════════════════════════════════════════════════════════════════════

void CaptureOrchestrator::executeNextStep() {
    if (m_stateMachine->state() != CaptureState::ExecutingSteps) return;

    emit stepChanged(m_currentStep, m_totalSteps);

    if (m_currentStep >= m_filteredScenario.stepCount()) {
        // All steps done
        // 5WHY: Log step completion to the execution log so the log records
        // the full capture journey — not just a header and footer.
        appendExecLog(QStringLiteral("All %1 steps complete").arg(m_totalSteps));
        if (m_recording && platformIsRecording()) {
            m_stateMachine->transitionTo(CaptureState::StoppingRecording);
        } else {
            m_stateMachine->transitionTo(CaptureState::Finalizing);
        }
        return;
    }

    const CaptureStep* step = m_filteredScenario.stepAt(m_currentStep);
    if (!step) {
        // Shouldn't happen, but handle gracefully.
        failCapture(QStringLiteral("STEP_NOT_FOUND"),
                    QStringLiteral("Internal error: step not found at index %1")
                        .arg(m_currentStep));
        return;
    }

    // 5WHY: Execution log previously had only header+footer with no per-step
    // entries.  If the capture crashed mid-session, the log revealed nothing
    // about which step was executing or what actions had been performed.
    // Log every step with its action, description, and timestamp so partial
    // sessions are auditable even when finalizeSession() never runs.
    appendExecLog(QStringLiteral("[Step %1/%2] %3  %4")
        .arg(m_currentStep + 1)
        .arg(m_totalSteps)
        .arg(step->description.isEmpty() ? QStringLiteral("(action %1)").arg(static_cast<int>(step->action)) : step->description)
        .arg(step->captureBefore ? QStringLiteral("(screenshot before)") : QString()));

    executeStep(m_currentStep);

    // Move to next step (steps that need async waiting will pause via state transitions)
    m_currentStep++;
}

void CaptureOrchestrator::executeStep(int stepIndex) {
    // Use the filtered scenario so indices match the actual execution sequence.
    // 5WHY: was using m_scenario (unfiltered) which caused step desync when
    // recordingOnly steps were skipped in screenshot-only mode.
    const CaptureStep* step = m_filteredScenario.stepAt(stepIndex);
    if (!step) return;

    m_currentAction = step->description.isEmpty()
        ? QStringLiteral("Step %1").arg(stepIndex + 1)
        : step->description;
    emit actionChanged(m_currentAction);
    // 5WHY: executeNextStep() already emits stepChanged before calling us —
    // emitting here too causes QML property bindings to re-evaluate twice
    // per step, triggering flicker in any step-progress animation.

    // ── Capture-before: take screenshot before executing the action ──
    if (step->captureBefore && wantsScreenshot()) {
        if (!takeScreenshot(sanitizeFilename(step->description), step->description)) {
            qWarning() << "CaptureOrchestrator: capture-before screenshot failed for step"
                       << (stepIndex + 1) << step->description;
        }
    }

    // ── Execute the action ──
    switch (step->action) {

    case StepAction::Navigate: {
        int tabIdx = step->param.toInt();
        m_navAdapter->switchToTab(tabIdx);

        // Wait for page ready before continuing
        int timeout = 3000;
        m_navAdapter->waitForPageReady(tabIdx, timeout,
            [this](bool ok) {
                if (!ok) {
                    // Page didn't load — continue anyway (best effort)
                }
                // Schedule next step with a small delay for visual settle
                scheduleStepAfter(m_recording ? kNavigateSettleRecordMs : kNavigateSettleMs);
            });
        break;
    }

    case StepAction::WaitPageReady: {
        int timeout = step->param.isEmpty() ? 3000 : step->param.toInt();
        scheduleStepAfter(timeout);
        break;
    }

    case StepAction::Capture: {
        // 5WHY: RecordingOnly mode (wantsScreenshot()=false) should NOT produce
        // PNG files. The captureBefore path correctly guards with wantsScreenshot(),
        // but explicit Capture steps had no guard — leaking ~10 screenshots per
        // recording-only session.
        if (!wantsScreenshot()) {
            // 5WHY: consistent 100ms async deferral with Scroll/SetUrl/RunDiagnostic
            // skip paths — avoids the stale-step infinite-recursion bug.
            deferNextStep();
            break;
        }
        // 5WHY: step->param holds the filename label per StepAction enum comment
        // ("param = filename label"), but code was reading step->description
        // which is empty for Capture steps, producing filenames like "XX_.png".
        QString label = sanitizeFilename(
            step->param.isEmpty() ? step->description : step->param);
        // 5WHY: takeScreenshot() return value was ignored — a failed screenshot
        // (platformCaptureScreenshot returns false) was silently skipped with no
        // warning.  Log the failure so the developer can diagnose missing captures.
        if (!takeScreenshot(label, label)) {
            qWarning() << "CaptureOrchestrator: screenshot failed for step"
                       << (stepIndex + 1) << label;
        }
        // 5WHY: Calling executeNextStep() directly from inside executeStep()
        // before the outer executeNextStep() has incremented m_currentStep
        // causes infinite recursion — the same step is re-executed forever.
        // Schedule the next step asynchronously so m_currentStep++ runs first.
        deferNextStep();
        break;
    }

    case StepAction::Scroll: {
        if (m_recording) {
            int duration = step->param.isEmpty() ? 3000 : step->param.toInt();
            // Need to connect scrollCtrl to the current Flickable
            // This is handled by the QML overlay via setFlickable
            m_scrollCtrl->scrollToBottom(duration);
            // onStepScrollFinished() will call executeNextStep()
        } else {
            // 5WHY: Same infinite-recursion fix as Capture above.
            deferNextStep();
        }
        break;
    }

    case StepAction::SetUrl: {
        if (m_appState) {
            m_appState->setTarget(step->param);
        }
        // 5WHY: Same infinite-recursion fix as Capture above.
        deferNextStep();
        break;
    }

    case StepAction::RunDiagnostic: {
        if (m_appState) {
            // Cancel any previous run first
            if (m_appState->runStatusInt() == 1) {
                m_appState->cancel();
            }
            // 5WHY: processEvents(500) was called between cancel() and
            // runDiagnostics(), which could dispatch pending timers from
            // previous steps and trigger WaitDiagComplete re-entrantly
            // BEFORE runDiagnostics() had executed. cancel() is synchronous;
            // no event-loop pump is needed.
            m_appState->runDiagnostics();
        }
        // WaitDiagComplete follows immediately in the scenario
        deferNextStep();
        break;
    }

    case StepAction::WaitDiagComplete: {
        int timeout = step->param.isEmpty() ? 120000 : step->param.toInt();
        // Clean up any previous poll timer (shouldn't happen, but be safe)
        if (m_pollTimer) {
            m_pollTimer->stop();
            m_pollTimer->deleteLater();
            m_pollTimer = nullptr;
        }
        // 5WHY: was a local QTimer that couldn't be stopped on cancel.
        // Now stored in m_pollTimer so CaptureOrchestrator::cancel() can
        // stop it, preventing wasted ticks and potential re-entrancy.
        m_pollTimer = new QTimer(this);
        auto elapsed = std::make_shared<int>(0);
        m_pollTimer->setInterval(500);

        // 5WHY: Explicit capture list prevents accidental capture of future
        // large locals added above this lambda in executeStep() (~200 lines).
        connect(m_pollTimer, &QTimer::timeout, this,
                [this, elapsed, timeout]() mutable {
            *elapsed += 500;
            int status = m_appState ? m_appState->runStatusInt() : 0;

            if (status != 1) { // not Running → complete/cancelled/error
                m_pollTimer->stop();
                m_pollTimer->deleteLater();
                m_pollTimer = nullptr;
                // Small delay to let results render
                QTimer::singleShot(kPreviewRenderSettleMs, this, [this]() {
                    executeNextStep();
                });
            } else if (*elapsed >= timeout) {
                m_pollTimer->stop();
                m_pollTimer->deleteLater();
                m_pollTimer = nullptr;
                // Timeout — try to cancel and continue
                if (m_appState) m_appState->cancel();
                QTimer::singleShot(kDiagCancelTimeoutMs, this, [this]() {
                    executeNextStep();
                });
            }
        });
        m_pollTimer->start();
        break;
    }

    case StepAction::OpenDetail: {
        int diagId = step->param.toInt();
        // First ensure we're on the diagnostic page
        m_navAdapter->switchToTab(1); // diagnostics tab
        // 5WHY: Generation-counter guard prevents the deferred navigation
        // from firing after cancellation — if the user cancels during the
        // 1500ms settle delay, the stale timer would otherwise open the
        // diagnostic detail view on an aborted capture session.
        int gen = m_countdownGen;
        QTimer::singleShot(kOpenDetailSettleMs, this, [this, diagId, gen]() {
            if (m_countdownGen != gen) return;
            m_navAdapter->openDiagnosticDetail(diagId);
            scheduleStepAfter(kPreviewRenderSettleMs);
        });
        break;
    }

    case StepAction::OpenReport: {
        m_navAdapter->openReportPreview();
        m_waitingForReportPreview = true;
        // 5WHY: openReportPreview() is async — it defers openPreview() by
        // 500ms for the StackView transition to settle.  The old code used a
        // fixed 2000ms timer that advanced regardless of whether the preview
        // actually opened, silently producing wrong screenshots on failure.
        // Now we wait for the reportPreviewReady(bool) signal (connected in
        // the constructor).  If the signal never fires (e.g. QML refactoring
        // removes the signal emission), the 5s safety timeout advances the
        // scenario so the capture doesn't hang forever.
        // 5WHY: capture the current m_countdownGen (incremented once per
        // CountdownToStart) so a stale safety timeout from a cancelled
        // session cannot prematurely advance a new session that reaches
        // OpenReport.
        {
            int gen = m_countdownGen;
            QTimer::singleShot(kReportPreviewTimeoutMs, this, [this, gen]() {
                if (m_waitingForReportPreview && m_countdownGen == gen) {
                    m_waitingForReportPreview = false;
                    qWarning() << "CaptureOrchestrator: report preview timed out — advancing anyway";
                    executeNextStep();
                }
            });
        }
        break;
    }

    } // end switch
}

// ═════════════════════════════════════════════════════════════════════════════
// Step completion callbacks
// ═════════════════════════════════════════════════════════════════════════════

void CaptureOrchestrator::onStepScrollFinished() {
    executeNextStep();
}

void CaptureOrchestrator::onReportPreviewReady(bool ok) {
    if (!m_waitingForReportPreview) return;
    m_waitingForReportPreview = false;

    if (!ok) {
        qWarning() << "CaptureOrchestrator: report preview failed to open — "
                       "screenshot may capture the wrong page";
    }

    // 5WHY: Small settle delay (1s) after the preview opens so the QML
    // overlay has time to render before the next Capture step screenshots
    // it.  The WaitPageReady step that follows OpenReport in the scenario
    // provides additional settle time.
    QTimer::singleShot(kPreviewRenderSettleMs, this, [this]() {
        executeNextStep();
    });
}

// 5WHY: Navigate and WaitPageReady both duplicated the same 3-line ritual:
// targeted-disconnect(m_delayTimer, timeout, this), then connect a lambda
// that calls executeNextStep(), then start(ms).  Any new step needing a
// delayed advance would copy this a third time.  Extract once.
void CaptureOrchestrator::scheduleStepAfter(int ms) {
    // 5WHY: Permanent connection established in constructor (line ~153).
    // No need to disconnect+reconnect on every call — just start the timer.
    m_delayTimer->start(ms);
}

void CaptureOrchestrator::deferNextStep() {
    QTimer::singleShot(kStepDeferMs, this, [this]() { executeNextStep(); });
}

void CaptureOrchestrator::appendExecLog(const QString& line) {
    // 5WHY: Per-step entries give the execution log auditability.  Without
    // them, a mid-capture crash leaves only the session header — zero
    // information about which steps completed or what was in progress.
    // Best-effort append: if the log file is unwritable (disk full, permissions
    // changed mid-session), silently skip — don't fail the capture over logging.
    if (m_sessionDir.isEmpty()) return;
    QFile logFile(m_sessionDir + kExecLogRelPath);
    if (!logFile.open(QIODevice::Append | QIODevice::Text)) return;
    QTextStream ts(&logFile);
    ts << QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz "))
       << line << "\n";
    // 5WHY: flush() ensures the entry is on disk before the next potentially-
    // crashing operation.  Without flush, buffered writes are lost on crash.
    ts.flush();
}

// ═════════════════════════════════════════════════════════════════════════════
// Helpers
// ═════════════════════════════════════════════════════════════════════════════

QString CaptureOrchestrator::sessionDir() const {
    return m_sessionDir;
}

// 5WHY: captureBefore and Capture blocks in executeStep() duplicated ~15 lines
// of identical screenshot-path logic.  The m_captureCount+1 off-by-one fix
// had to be applied identically in both places.  Extract once — the caller
// handles label selection and step-advance timing; this handles the common
// mechanics.
bool CaptureOrchestrator::takeScreenshot(const QString& sanitizedLabel,
                                          const QString& manifestDesc) {
    QString filePath = m_sessionDir + kSubdirScreenshots
        + QStringLiteral("/%1").arg(m_captureCount + 1, 3, 10, QLatin1Char('0'))
        + QStringLiteral("_") + sanitizedLabel + QStringLiteral(".png");
    // 5WHY: The capture running overlay (z:2100) is rendered on top of the app
    // and would appear in every screenshot — progress bar, step labels, red dot.
    // Hide it during the screenshot so evidence files show only the diagnostic
    // content.  QML binds opacity to suppressOverlay.
    //
    // 5WHY (round-30): QML property bindings are evaluated lazily — the
    // emits in lines 1135/1138 both fire in the same event-loop iteration
    // and the binding engine sees only the final value (false).  The
    // overlay was NEVER hidden.  Call processEvents() after the first emit
    // to flush pending QML binding updates before the screenshot runs.
    m_suppressOverlay = true;
    emit suppressOverlayChanged();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    // 5WHY: processEvents() can dispatch pending timers (m_delayTimer from
    // a concurrent Navigate settle, QTimer::singleShot callbacks) that may
    // trigger executeNextStep() or cancel() re-entrantly.  If the FSM has
    // left ExecutingSteps, abort — taking a screenshot outside the capture
    // loop would produce a stale/corrupt evidence file.
    if (m_stateMachine->state() != CaptureState::ExecutingSteps) {
        m_suppressOverlay = false;
        emit suppressOverlayChanged();
        return false;
    }
    bool ok = platformCaptureScreenshot(filePath);
    m_suppressOverlay = false;
    emit suppressOverlayChanged();
    if (!ok) {
        qWarning() << "CaptureOrchestrator: screenshot failed for" << sanitizedLabel;
        return false;
    }
    m_captureCount++;
    emit captureCountChanged(m_captureCount);
    appendToManifest(manifestDesc, filePath);
    return true;
}

// 5WHY: finalizeSession() and appendToManifest() each duplicated a ~14-line
// "write JSON to .tmp file, close, then atomic rename" block.  Extract once
// so a fix (e.g. adding fsync before rename for non-journaling filesystems)
// applies uniformly.  Returns true on success; caller handles error reporting.
static bool atomicWriteManifest(const QString& manifestPath, const QJsonObject& obj) {
    const QString tmpPath = manifestPath + QStringLiteral(".tmp");
    QFile tmpFile(tmpPath);
    if (!tmpFile.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    if (tmpFile.write(QJsonDocument(obj).toJson(QJsonDocument::Indented)) == -1) {
        tmpFile.remove();
        return false;
    }
    tmpFile.close();
    // 5WHY: QFile::rename() on POSIX is an atomic rename(2) — the destination
    // is atomically replaced.  If the app crashes before this line, only the
    // .tmp file is lost; the original manifest is untouched.
    if (!tmpFile.rename(manifestPath)) {
        tmpFile.remove();  // clean up orphaned temp file
        return false;
    }
    return true;
}

void CaptureOrchestrator::appendToManifest(const QString& description,
                                            const QString& filePath) {
    // 5WHY: This performs a full read-parse-modify-truncate-write cycle per
    // capture entry — an N+1 I/O pattern.  For a 20-screenshot session this is
    // 20 full manifest rewrites.  Deliberately NOT batched in memory: the
    // manifest acts as a write-ahead journal.  Each entry is persisted
    // immediately so that if the app crashes mid-capture (e.g. the user is
    // capturing evidence of a crash), partial results — all screenshots taken
    // before the crash — are already recorded in the manifest.  Batching would
    // lose all evidence if the crash occurs before finalizeSession().
    //
    // 5WHY: The old code opened the manifest ReadWrite, called resize(0), then
    // write().  A crash between resize(0) and write() left a zero-byte file —
    // losing ALL previously journaled entries even though the PNG files were
    // intact.  Now we write the new content to a temp file and rename it over
    // the original.  On POSIX (Linux/macOS/iOS), rename() is atomic — the
    // destination is either the old or new content, never a partial file.
    // On Windows, QFile::rename() may fall back to copy+delete, but capture
    // happens on mobile/desktop where POSIX atomic rename is available.
    const QString manifestPath = m_sessionDir + kManifestRelPath;
    QFile mf(manifestPath);
    if (!mf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "CaptureOrchestrator: cannot open manifest for append, capture entry lost";
        return;
    }

    QByteArray data = mf.readAll();
    // 5WHY: Check the file error state after readAll — consistent with
    // finalizeSession().  A failing storage medium may return truncated
    // data that happens to parse as valid-but-incomplete JSON.
    if (mf.error() != QFile::NoError) {
        qWarning() << "CaptureOrchestrator: read error on manifest, cannot append capture entry";
        return;
    }
    mf.close();  // release the file handle before rename

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        qWarning() << "CaptureOrchestrator: manifest is corrupt, cannot append capture entry";
        return;
    }
    QJsonObject obj = doc.object();
    QJsonArray captures = obj["captures"].toArray();

    QJsonObject entry;
    entry["seq"] = QStringLiteral("%1").arg(m_captureCount, 3, 10, QLatin1Char('0'));
    entry["description"] = description;
    QFileInfo fi(filePath);
    entry["file"] = fi.fileName();
    entry["size"] = fi.size();
    entry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    captures.append(entry);

    obj["captures"] = captures;

    if (!atomicWriteManifest(manifestPath, obj)) {
        qWarning() << "CaptureOrchestrator: cannot write manifest, capture entry lost";
        return;
    }
}
