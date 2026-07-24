// =============================================================================
// CaptureOrchestrator.cpp — Master orchestrator implementation
// =============================================================================
#include "EvidenceCapture/CaptureOrchestrator.h"
#include "EvidenceCapture/CaptureScenario.h"
#include "EvidenceCapture/NavigationAdapter.h"
#include "EvidenceCapture/ScrollController.h"
#include "EvidenceCapture/CaptureService.h"
#include "Common/Model/CaptureFeatureGate.h"
#include "Common/Platform/PlatformCapture.h"
#include "Common/Platform/PlatformRecording.h"
#include "Common/Platform/PlatformKeepAwake.h"
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
#include <QStorageInfo>
#include <QProcess>

// ═════════════════════════════════════════════════════════════════════════════
// Constructor
// ═════════════════════════════════════════════════════════════════════════════

CaptureOrchestrator::CaptureOrchestrator(AppState* appState, QObject* parent)
    : QObject(parent)
    , m_appState(appState)
    , m_stateMachine(new CaptureStateMachine(this))
    , m_navAdapter(new NavigationAdapter(nullptr, m_appState, this))
    , m_scrollCtrl(new ScrollController(this))
    , m_captureSvc(appState->captureService())
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

void CaptureOrchestrator::requestModeSelection() {
    if (!CaptureFeatureGate::isFeatureEnabled()) {
        CaptureFeatureGate::setFeatureEnabled(true);
    }
    emit modeSelectionRequested();
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

    m_captureMode = captureMode;
    m_diagUrl = diagUrl.trimmed();
    m_recording = (captureMode == RecordingOnly || captureMode == Both);
    m_doScreenshot = (captureMode == ScreenshotOnly || captureMode == Both);
    m_currentStep = 0;
    m_captureCount = 0;
    m_sessionDir.clear();

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

    // Stop recording if active
    if (m_recording && platformIsRecording()) {
        platformStopRecording(nullptr);
    }

    // Restore screen state
    platformSetKeepAwake(false);

    // Clean up session
    if (!m_sessionDir.isEmpty()) {
        QDir dir(m_sessionDir);
        if (dir.exists()) {
            // Keep partial results; don't delete
        }
    }

    m_stateMachine->transitionTo(CaptureState::Cancelled);
    emit captureCancelled();
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
        // QML handles the countdown via state binding; when countdown ends,
        // QML calls transitionTo(CreatingSession) via a signal or direct call.
        // For now, auto-advance after 3s (QML overlay will be added later).
        m_currentAction = QStringLiteral("Preparing to capture...");
        emit actionChanged(m_currentAction);
        platformSetKeepAwake(true);
        QTimer::singleShot(3000, this, [this]() {
            m_stateMachine->transitionTo(CaptureState::CreatingSession);
        });
        break;

    case CaptureState::CreatingSession:
        createSession();
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
        finalizeSession(true);
        break;

    case CaptureState::Completed:
        platformSetKeepAwake(false);
        emit captureCompleted(m_sessionDir);
        break;

    case CaptureState::Failed:
        platformSetKeepAwake(false);
        emit captureFailed(QStringLiteral("CAPTURE_FAILED"),
                           QStringLiteral("Capture failed. See execution.log for details."));
        break;

    case CaptureState::Cancelled:
        // Already handled in cancel()
        break;

    default:
        break;
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Phase implementations
// ═════════════════════════════════════════════════════════════════════════════

void CaptureOrchestrator::runPreflight() {
    // Check ffmpeg availability for recording modes
    if (m_recording) {
        QProcess which;
        which.start(QStringLiteral("which"), {QStringLiteral("ffmpeg")});
        which.waitForFinished(3000);
        if (which.exitCode() != 0) {
            m_stateMachine->transitionTo(CaptureState::Failed);
            emit captureFailed(QStringLiteral("NO_FFMPEG"),
                               QStringLiteral("ffmpeg is required for screen recording. "
                                              "Install: sudo apt install ffmpeg"));
            return;
        }
    }

    // Check disk space (>100MB)
    QString root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QStorageInfo storage(root);
    if (storage.isValid() && storage.bytesAvailable() < 100 * 1024 * 1024) {
        m_stateMachine->transitionTo(CaptureState::Failed);
        emit captureFailed(QStringLiteral("LOW_DISK"),
                           QStringLiteral("Less than 100MB disk space available."));
        return;
    }

    m_stateMachine->transitionTo(CaptureState::CountdownToStart);
}

void CaptureOrchestrator::createSession() {
    QString modeStr;
    switch (m_captureMode) {
    case ScreenshotOnly: modeStr = QStringLiteral("screenshot"); break;
    case RecordingOnly:  modeStr = QStringLiteral("recording");  break;
    case Both:           modeStr = QStringLiteral("both");        break;
    }

    QString ts = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    // Store under Evidence/AutoCapture/ adjacent to crash reports
    // 5WHY: co-located with crash reports so all diagnostic evidence is in one place.
    QString root = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + QStringLiteral("/Evidence/AutoCapture");
    m_sessionDir = root + QStringLiteral("/") + ts + QStringLiteral("_") + modeStr;

    QDir().mkpath(m_sessionDir);

    // Write initial manifest.json
    QJsonObject manifest;
    manifest["session_id"] = ts;
    manifest["capture_mode"] = modeStr;
    manifest["diag_url"] = m_diagUrl;
    manifest["started_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    manifest["status"] = "running";
    manifest["captures"] = QJsonArray();

    QFile mf(m_sessionDir + QStringLiteral("/manifest.json"));
    if (mf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        mf.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
    }

    // Write execution log header
    QFile logFile(m_sessionDir + QStringLiteral("/execution.log"));
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream logStream(&logFile);
        logStream << "=== Automated Capture Session ===\n"
                  << "Session:  " << ts << "\n"
                  << "Mode:     " << modeStr << "\n"
                  << "URL:      " << m_diagUrl << "\n"
                  << "Started:  " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";
    }

    m_currentAction = QStringLiteral("Session created: ") + m_sessionDir;
    emit actionChanged(m_currentAction);
}

void CaptureOrchestrator::startPlatformRecording() {
    QString recPath = m_sessionDir + QStringLiteral("/recording");

    m_currentAction = QStringLiteral("Starting screen recording...");
    emit actionChanged(m_currentAction);

    platformStartRecording(recPath, [this](bool ok, const QString& pathOrError) {
        if (ok) {
            m_currentAction = QStringLiteral("Recording started");
            emit actionChanged(m_currentAction);
            m_stateMachine->transitionTo(CaptureState::ExecutingSteps);
        } else {
            m_stateMachine->transitionTo(CaptureState::Failed);
            emit captureFailed(QStringLiteral("RECORDING_FAILED"), pathOrError);
        }
    });
}

void CaptureOrchestrator::stopPlatformRecording() {
    m_currentAction = QStringLiteral("Stopping recording...");
    emit actionChanged(m_currentAction);

    platformStopRecording([this](bool ok, const QString& pathOrError) {
        if (ok) {
            m_currentAction = QStringLiteral("Recording saved: ") + pathOrError;
        } else {
            m_currentAction = QStringLiteral("Recording stop issue: ") + pathOrError;
        }
        emit actionChanged(m_currentAction);
        m_stateMachine->transitionTo(CaptureState::Finalizing);
    });
}

void CaptureOrchestrator::finalizeSession(bool success) {
    // Update manifest
    QFile mf(m_sessionDir + QStringLiteral("/manifest.json"));
    if (mf.open(QIODevice::ReadWrite | QIODevice::Text)) {
        QByteArray data = mf.readAll();
        mf.seek(0);
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();
        obj["completed_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        obj["status"] = success ? QStringLiteral("completed") : QStringLiteral("failed");
        obj["total_captures"] = m_captureCount;
        mf.resize(0);
        mf.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    }

    // Append to execution log
    QFile logFile2(m_sessionDir + QStringLiteral("/execution.log"));
    if (logFile2.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream logStream2(&logFile2);
        logStream2 << "\n--- Session " << (success ? "complete" : "failed") << " ---\n"
                   << "Total captures: " << m_captureCount << "\n"
                   << "Ended: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    }

    if (success) {
        m_stateMachine->transitionTo(CaptureState::Completed);
    }
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
        // Shouldn't happen, but handle gracefully
        m_stateMachine->transitionTo(CaptureState::Failed);
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
        QString filePath = m_sessionDir + QStringLiteral("/")
            + QStringLiteral("%1").arg(m_captureCount, 2, 10, QLatin1Char('0'))
            + QStringLiteral("_") + step->description + QStringLiteral(".png");
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
                m_delayTimer->start(m_recording ? 2000 : 500);  // longer delay for recording
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
        QString filePath = m_sessionDir + QStringLiteral("/")
            + QStringLiteral("%1").arg(m_captureCount, 2, 10, QLatin1Char('0'))
            + QStringLiteral("_") + step->description + QStringLiteral(".png");
        if (platformCaptureScreenshot(filePath)) {
            m_captureCount++;
            emit captureCountChanged(m_captureCount);
            appendToManifest(step->description, filePath);
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
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 500);
            }
            m_appState->runDiagnostics();
        }
        // WaitDiagComplete follows immediately in the scenario
        // 5WHY: Same infinite-recursion fix as Capture above.
        QTimer::singleShot(100, this, [this]() { executeNextStep(); });
        break;
    }

    case StepAction::WaitDiagComplete: {
        int timeout = step->param.isEmpty() ? 120000 : step->param.toInt();
        // Poll for diagnostic completion
        auto pollTimer = new QTimer(this);
        auto elapsed = std::make_shared<int>(0);
        pollTimer->setInterval(500);

        connect(pollTimer, &QTimer::timeout, this, [=]() mutable {
            *elapsed += 500;
            int status = m_appState ? m_appState->runStatusInt() : 0;

            if (status != 1) { // not Running → complete/cancelled/error
                pollTimer->stop();
                pollTimer->deleteLater();
                // Small delay to let results render
                QTimer::singleShot(1000, this, [this]() {
                    executeNextStep();
                });
            } else if (*elapsed >= timeout) {
                pollTimer->stop();
                pollTimer->deleteLater();
                // Timeout — try to cancel and continue
                if (m_appState) m_appState->cancel();
                QTimer::singleShot(500, this, [this]() {
                    executeNextStep();
                });
            }
        });
        pollTimer->start();
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

void CaptureOrchestrator::onStepPageReady(int tabIndex) {
    Q_UNUSED(tabIndex);
    // Page is ready; next step is triggered by the callback in executeStep
}

void CaptureOrchestrator::onStepScrollFinished() {
    executeNextStep();
}

void CaptureOrchestrator::onStepDiagComplete() {
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
    QFile mf(m_sessionDir + QStringLiteral("/manifest.json"));
    if (!mf.open(QIODevice::ReadWrite | QIODevice::Text)) return;

    QByteArray data = mf.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
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
