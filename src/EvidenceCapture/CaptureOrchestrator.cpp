// =============================================================================
// CaptureOrchestrator.cpp — Master orchestrator implementation
// =============================================================================
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
    platformRestoreBrightness();
    platformDisableFocusMode();
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

    connect(m_stateMachine, &CaptureStateMachine::stateChanged,
            this, &CaptureOrchestrator::onStateChanged);
    connect(m_scrollCtrl, &ScrollController::scrollFinished,
            this, &CaptureOrchestrator::onStepScrollFinished);
}

CaptureOrchestrator::~CaptureOrchestrator() {
    // 5WHY: cancel() already stops recording and disables keepAwake.
    // Calling platformStopRecording(nullptr) again is redundant and
    // risks a race with the cancel() path's stop callbacks.
    if (m_stateMachine->isRunning()) {
        cancel();
    }
    // Safety net: if cancel() failed to stop recording (e.g. state
    // machine rejected the transition), force-stop here.
    if (m_recording && platformIsRecording()) {
        platformStopRecording(nullptr);
    }
    platformSetKeepAwake(false);
    // 5WHY: Safety net — if cancel() or the terminal-state handlers didn't
    // restore system state, disable focus mode here so subsequent app usage
    // isn't affected by the capture's DND/brightness changes.
    restoreSystemState();
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

void CaptureOrchestrator::failCapture(const QString& errorCode, const QString& message) {
    emit captureFailed(errorCode, message);
    m_stateMachine->transitionTo(CaptureState::Failed);
}

void CaptureOrchestrator::requestModeSelection() {
    if (!CaptureFeatureGate::isFeatureEnabled()) {
        CaptureFeatureGate::setFeatureEnabled(true);
    }
    emit modeSelectionRequested();
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
    m_doScreenshot = (captureMode == ScreenshotOnly || captureMode == Both);
    m_currentStep = 0;
    m_captureCount = 0;
    m_sessionDir.clear();
    m_currentAction.clear();  // 5WHY: stale action from a previous capture (e.g.
    // "Recording saved: ...") would be briefly visible in the QML overlay until
    // the first onStateChanged → CountdownToStart sets a new action.  Clear it
    // at the start so the UI shows an empty state until the new action is set.
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

    // 5WHY: platformStopRecording(nullptr) initiates an async stop —
    // after it returns, platformIsRecording() is already false (the
    // atomic is flipped), so the destructor's safety net is moot.
    // Clear m_recording immediately so the flag doesn't leak into
    // the next capture session.
    if (m_recording && platformIsRecording()) {
        platformStopRecording(nullptr);
        m_recording = false;
    } else {
        m_recording = false;
    }

    // Stop any in-progress scroll
    m_scrollCtrl->cancel();

    // 5WHY: transitionTo(Cancelled) is not valid from every state.
    // CreatingSession rejects it (FSM table lacks the transition).
    // StoppingRecording and Finalizing both accept Cancelled per the
    // FSM table (verified in CaptureStateMachine.cpp:46-48).
    // Only emit captureCancelled and disable keep-awake if the FSM
    // accepted the transition.  If rejected, the FSM continues and its
    // terminal handlers will disable keep-awake.
    bool didCancel = m_stateMachine->transitionTo(CaptureState::Cancelled);
    if (didCancel) {
        platformSetKeepAwake(false);
        restoreSystemState();
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
    emit stateChanged();

    CaptureState s = static_cast<CaptureState>(to);

    switch (s) {
    case CaptureState::Preflight:
        runPreflight();
        break;

    case CaptureState::CountdownToStart:
        // 5WHY: was using QTimer::singleShot(3000) independent of the QML
        // countdown — two timers racing. Now the QML countdown is the sole
        // authority: CapturePreflightOverlay.countdownFinished() calls
        // onCountdownFinished() below, which transitions to CreatingSession.
        m_currentAction = QStringLiteral("Preparing to capture...");
        emit actionChanged(m_currentAction);
        platformSetKeepAwake(true);
        platformEnableFocusMode();  // 5WHY: suppress notifications during capture
        platformSetMaxBrightness(); // ensure max screen brightness for clear recordings
        // Safety net: if the QML preflight overlay fails to load or the
        // countdown signal is never emitted, auto-advance after 10s so the
        // capture doesn't hang forever. The state guard ensures this won't
        // race with a successful QML countdown (which fires at ~3s).
        // 5WHY: without a generation counter, a stale timer from a previous
        // cancel+restart cycle can fire during a NEW CountdownToStart and
        // prematurely advance it. The generation counter makes each timer
        // guard against only its own capture attempt.
        {
            int gen = ++m_countdownGen;
            QTimer::singleShot(10000, this, [this, gen]() {
                if (m_countdownGen == gen
                    && m_stateMachine->state() == CaptureState::CountdownToStart) {
                    qWarning() << "CaptureOrchestrator: countdown safety timer fired — QML overlay may have failed to load";
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
        startPlatformRecording();
        break;

    case CaptureState::ExecutingSteps:
        executeNextStep();
        break;

    case CaptureState::StoppingRecording:
        stopPlatformRecording();
        break;

    case CaptureState::Finalizing:
        finalizeSession();
        break;

    case CaptureState::Completed:
        platformSetKeepAwake(false);
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
        platformSetKeepAwake(false);
        restoreSystemState();
        // specific error code the phase handler already emitted (e.g.
        // "NO_FFMPEG", "RECORDING_FAILED"). The Failed handler only
        // restores system state; the specific error was already emitted.
        break;

    case CaptureState::Cancelled:
        // Safety net: if cancel() couldn't disable keep-awake (e.g. the
        // transition was triggered from a path other than cancel()), do it here.
        platformSetKeepAwake(false);
        restoreSystemState();
        break;

    default:
        break;
    }
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
    if (m_recording) {
#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID)
        // Mobile platforms use native recording APIs — no ffmpeg check needed.
        // platformStartRecording() reports any errors via its callback.
#else
        QString ffmpegPath = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
        if (ffmpegPath.isEmpty()) {
            failCapture(QStringLiteral("NO_FFMPEG"),
                        QStringLiteral("ffmpeg is required for screen recording. "
                                       "Install: sudo apt install ffmpeg"));
            return;
        }
        // ffmpeg found — continue with disk space check
#endif
    }

    finishPreflight();
}

void CaptureOrchestrator::finishPreflight() {
    // 5WHY: If the FSM left Preflight (cancelled, failed) while the async
    // ffmpeg check ran, bail out — don't continue the preflight from a
    // stale state.
    if (m_stateMachine->state() != CaptureState::Preflight) return;

    // Check disk space (>100MB)
    QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QStorageInfo storage(root);
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
    }

    // Blueprint §10: yyyyMMdd_HHmmss_zzz under CrashReports/Capture/
    // 5WHY: zzz (milliseconds) prevents directory collisions when two captures
    // are started within the same second (e.g., rapid retry after storage error).
    const QDateTime now = QDateTime::currentDateTime();
    QString ts = now.toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    // Store under CrashReports/Capture/ per design doc §10
    QString root = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + QStringLiteral("/CrashReports/Capture");
    m_sessionDir = root + QStringLiteral("/") + ts;

    // Create subdirectories per design §10 structure
    if (!QDir().mkpath(m_sessionDir + QStringLiteral("/Screenshots"))
        || !QDir().mkpath(m_sessionDir + QStringLiteral("/Videos"))
        || !QDir().mkpath(m_sessionDir + QStringLiteral("/Logs"))
        || !QDir().mkpath(m_sessionDir + QStringLiteral("/Metadata"))) {
        failCapture(QStringLiteral("STORAGE_ERROR"), QStringLiteral("Cannot create session directory"));
        return;
    }

    // Write initial manifest.json (in Metadata/ per design §10)
    QJsonObject manifest;
    manifest["session_id"] = ts;
    manifest["capture_mode"] = modeStr;
    QString safeUrl = sanitizeUrl(m_diagUrl);
    manifest["diag_url"] = safeUrl;
    manifest["started_at"] = now.toString(Qt::ISODate);
    manifest["timestamp"] = now.toString(Qt::ISODate);
    manifest["status"] = "running";
    manifest["device"] = QSysInfo::machineHostName();
    manifest["os"] = QSysInfo::productType() + QStringLiteral(" ") + QSysInfo::productVersion();
    manifest["captures"] = QJsonArray();

    QFile mf(m_sessionDir + QStringLiteral("/Metadata/manifest.json"));
    if (mf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        mf.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
    }

    // Write execution log header
    QFile logFile(m_sessionDir + QStringLiteral("/Logs/execution.log"));
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream logStream(&logFile);
        logStream << "=== Automated Capture Session ===\n"
                  << "Session:  " << ts << "\n"
                  << "Mode:     " << modeStr << "\n"
                  << "URL:      " << safeUrl << "\n"
                  << "Started:  " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";
    }

    m_currentAction = QStringLiteral("Session created: ") + m_sessionDir;
    emit actionChanged(m_currentAction);
}

void CaptureOrchestrator::startPlatformRecording() {
    QString recPath = m_sessionDir + QStringLiteral("/Videos/Capture_AutoDemo");

    m_currentAction = QStringLiteral("Starting screen recording...");
    emit actionChanged(m_currentAction);

    platformStartRecording(recPath, [this](bool ok, const QString& pathOrError) {
        if (ok) {
            m_currentAction = QStringLiteral("Recording started");
            emit actionChanged(m_currentAction);
            m_stateMachine->transitionTo(CaptureState::ExecutingSteps);
        } else {
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

void CaptureOrchestrator::finalizeSession() {
    // 5WHY: the success parameter was dead code after stopPlatformRecording
    // refactored to transition directly to Failed on error. Recording stop
    // failures and all other errors now transition to Failed state directly,
    // so finalizeSession only handles the success path.  The corrupt-manifest
    // fallback below is the only remaining failure path through this function.
    // Update manifest
    QFile mf(m_sessionDir + QStringLiteral("/Metadata/manifest.json"));
    if (mf.open(QIODevice::ReadWrite | QIODevice::Text)) {
        QByteArray data = mf.readAll();
        mf.seek(0);
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
        mf.resize(0);
        mf.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    }

    // Append to execution log
    QFile logFile2(m_sessionDir + QStringLiteral("/Logs/execution.log"));
    if (logFile2.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream logStream2(&logFile2);
        logStream2 << "\n--- Session complete ---\n"
                   << "Total captures: " << m_captureCount << "\n"
                   << "Ended: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
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
    emit stepChanged(stepIndex, m_totalSteps);

    // ── Capture-before: take screenshot before executing the action ──
    if (step->captureBefore && m_doScreenshot) {
        QString safeDesc = step->description;
        safeDesc.replace(QLatin1Char(':'), QLatin1Char('-')); // Windows-safe
        QString filePath = m_sessionDir + QStringLiteral("/Screenshots/")
            + QStringLiteral("%1").arg(m_captureCount, 2, 10, QLatin1Char('0'))
            + QStringLiteral("_") + safeDesc + QStringLiteral(".png");
        if (platformCaptureScreenshot(filePath)) {
            m_captureCount++;
            emit captureCountChanged(m_captureCount);

            // Update manifest
            appendToManifest(step->description, filePath);
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
                m_delayTimer->disconnect();
                connect(m_delayTimer, &QTimer::timeout, this, [this]() {
                    executeNextStep();
                });
                m_delayTimer->start(m_recording ? 4000 : 500);  // longer delay for recording
            });
        break;
    }

    case StepAction::WaitPageReady: {
        int timeout = step->param.isEmpty() ? 3000 : step->param.toInt();
        m_delayTimer->disconnect();
        connect(m_delayTimer, &QTimer::timeout, this, [this]() {
            executeNextStep();
        });
        m_delayTimer->start(timeout);
        break;
    }

    case StepAction::Capture: {
        // 5WHY: RecordingOnly mode (m_doScreenshot=false) should NOT produce
        // PNG files. The captureBefore path correctly guards with m_doScreenshot,
        // but explicit Capture steps had no guard — leaking ~10 screenshots per
        // recording-only session.
        if (!m_doScreenshot) {
            QTimer::singleShot(0, this, [this]() { executeNextStep(); });
            break;
        }
        // 5WHY: step->param holds the filename label per StepAction enum comment
        // ("param = filename label"), but code was reading step->description
        // which is empty for Capture steps, producing filenames like "XX_.png".
        QString label = step->param.isEmpty() ? step->description : step->param;
        // 5WHY: description can contain ':' (e.g. "Diagnostics: run") which is
        // illegal in Windows filenames. Replace with '-' for cross-platform safety.
        label.replace(QLatin1Char(':'), QLatin1Char('-'));
        QString filePath = m_sessionDir + QStringLiteral("/Screenshots/")
            + QStringLiteral("%1").arg(m_captureCount, 2, 10, QLatin1Char('0'))
            + QStringLiteral("_") + label + QStringLiteral(".png");
        if (platformCaptureScreenshot(filePath)) {
            m_captureCount++;
            emit captureCountChanged(m_captureCount);
            appendToManifest(label, filePath);
        }
        // 5WHY: Calling executeNextStep() directly from inside executeStep()
        // before the outer executeNextStep() has incremented m_currentStep
        // causes infinite recursion — the same step is re-executed forever.
        // Schedule the next step asynchronously so m_currentStep++ runs first.
        QTimer::singleShot(100, this, [this]() { executeNextStep(); });
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
            QTimer::singleShot(100, this, [this]() { executeNextStep(); });
        }
        break;
    }

    case StepAction::SetUrl: {
        if (m_appState) {
            m_appState->setTarget(step->param);
        }
        // 5WHY: Same infinite-recursion fix as Capture above.
        QTimer::singleShot(100, this, [this]() { executeNextStep(); });
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
        QTimer::singleShot(100, this, [this]() { executeNextStep(); });
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

        connect(m_pollTimer, &QTimer::timeout, this, [=]() mutable {
            *elapsed += 500;
            int status = m_appState ? m_appState->runStatusInt() : 0;

            if (status != 1) { // not Running → complete/cancelled/error
                m_pollTimer->stop();
                m_pollTimer->deleteLater();
                m_pollTimer = nullptr;
                // Small delay to let results render
                QTimer::singleShot(1000, this, [this]() {
                    executeNextStep();
                });
            } else if (*elapsed >= timeout) {
                m_pollTimer->stop();
                m_pollTimer->deleteLater();
                m_pollTimer = nullptr;
                // Timeout — try to cancel and continue
                if (m_appState) m_appState->cancel();
                QTimer::singleShot(500, this, [this]() {
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
        QTimer::singleShot(1500, this, [this, diagId]() {
            m_navAdapter->openDiagnosticDetail(diagId);
            QTimer::singleShot(1000, this, [this]() {
                executeNextStep();
            });
        });
        break;
    }

    case StepAction::OpenReport: {
        m_navAdapter->openReportPreview();
        // Best-effort — report preview may not be directly accessible
        QTimer::singleShot(2000, this, [this]() {
            executeNextStep();
        });
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

// ═════════════════════════════════════════════════════════════════════════════
// Helpers
// ═════════════════════════════════════════════════════════════════════════════

QString CaptureOrchestrator::sessionDir() const {
    return m_sessionDir;
}

void CaptureOrchestrator::appendToManifest(const QString& description,
                                            const QString& filePath) {
    // Read current manifest, append capture entry, write back
    QFile mf(m_sessionDir + QStringLiteral("/Metadata/manifest.json"));
    if (!mf.open(QIODevice::ReadWrite | QIODevice::Text)) return;

    QByteArray data = mf.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        qWarning() << "CaptureOrchestrator: manifest is corrupt, cannot append capture entry";
        return;
    }
    QJsonObject obj = doc.object();
    QJsonArray captures = obj["captures"].toArray();

    QJsonObject entry;
    entry["seq"] = QStringLiteral("%1").arg(m_captureCount, 2, 10, QLatin1Char('0'));
    entry["description"] = description;
    entry["file"] = QFileInfo(filePath).fileName();
    entry["size"] = QFileInfo(filePath).size();
    entry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    captures.append(entry);

    obj["captures"] = captures;
    mf.resize(0);
    mf.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}
