// =============================================================================
// CaptureOrchestrator.h — Master orchestrator for automated evidence capture
// =============================================================================
// Design ref: docs/AutomatedEvidenceCapture_Design.md §4.1
//
// Owns and coordinates:
//   CaptureStateMachine  — 11-state FSM
//   CaptureScenario      — declarative step definitions
//   NavigationAdapter    — QML AppContent bridge
//   ScrollController     — Flickable smooth scrolling
//
// Uses platform abstractions:
//   PlatformCapture      — screenshot (QScreen::grabWindow)
//   PlatformRecording    — screen recording (ffmpeg / ReplayKit / MediaProjection)
//   PlatformKeepAwake    — prevent screen lock
//   CaptureFeatureGate   — feature toggle
//   CaptureService       — session directory + manifest
// =============================================================================
#pragma once

#include <QObject>
#include <QTimer>
#include <QString>
#include <QElapsedTimer>
#include <functional>
#include "EvidenceCapture/CaptureStateMachine.h"
#include "EvidenceCapture/CaptureScenario.h"

class AppState;
class NavigationAdapter;
class ScrollController;
class CaptureOrchestrator : public QObject {
    Q_OBJECT

    // ── QML-exposed properties ──────────────────────────────────────────
    Q_PROPERTY(int state READ stateInt NOTIFY stateChanged)
    Q_PROPERTY(int currentStep READ currentStep NOTIFY stepChanged)
    Q_PROPERTY(int totalSteps READ totalSteps NOTIFY stepChanged)
    Q_PROPERTY(QString currentAction READ currentAction NOTIFY actionChanged)
    Q_PROPERTY(int captureCount READ captureCount NOTIFY captureCountChanged)
    Q_PROPERTY(int elapsedSeconds READ elapsedSeconds NOTIFY stateChanged)

public:
    enum CaptureMode {
        ScreenshotOnly = 0,
        RecordingOnly  = 1,
        Both           = 2
    };
    Q_ENUM(CaptureMode)

    explicit CaptureOrchestrator(AppState* appState, QObject* parent = nullptr);
    ~CaptureOrchestrator() override;

    // ── Accessors ───────────────────────────────────────────────────────
    int stateInt() const;
    int currentStep() const { return m_currentStep; }
    int totalSteps() const { return m_totalSteps; }
    QString currentAction() const { return m_currentAction; }
    int captureCount() const { return m_captureCount; }
    int elapsedSeconds() const {
        return m_elapsed.isValid() ? static_cast<int>(m_elapsed.elapsed() / 1000) : 0;
    }

    // ── QML-invokable API ───────────────────────────────────────────────
    // Request the mode selection panel. Enables the feature gate and emits
    // modeSelectionRequested() — AppContent's Loader listens and shows CaptureModePanel.
    Q_INVOKABLE void requestModeSelection();
    Q_INVOKABLE void startCapture(int captureMode, const QString& diagUrl);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE bool isRunning() const { return m_stateMachine->isRunning(); }
    Q_INVOKABLE bool isRecordingCapture() const { return m_recording; }
    // Called by QML CapturePreflightOverlay when the visual countdown reaches 0.
    // Replaces the old C++ QTimer::singleShot(3000) — the QML countdown is the
    // single source of truth for timing.
    Q_INVOKABLE void onCountdownFinished();

    // Set the AppContent QML object for navigation. Must be called after
    // QML engine is initialized (in main.cpp after context properties).
    Q_INVOKABLE void setAppContent(QObject* appContent);

    // Wire a QML Flickable to the ScrollController so recording-mode
    // scroll steps actually scroll the page instead of silently no-oping.
    Q_INVOKABLE void setScrollFlickable(QObject* flickable);

    // Open the system Focus / Do-Not-Disturb settings page (iOS/Android).
    // Called from CapturePreflightOverlay when the user taps the DND hint.
    Q_INVOKABLE void openFocusSettings();

signals:
    void stateChanged();
    void stepChanged(int current, int total);
    void actionChanged(const QString& action);
    void captureCountChanged(int count);
    void captureCompleted(const QString& sessionPath);
    void captureCancelled();
    void captureFailed(const QString& errorCode, const QString& userMessage);
    void modeSelectionRequested();  // QML should show CaptureModePanel

private slots:
    void onStateChanged(int from, int to);
    void executeNextStep();
    void onStepScrollFinished();

private:
    // ── Step execution ──────────────────────────────────────────────────
    void executeStep(int stepIndex);
    void runPreflight();
    void finishPreflight();  // second half of preflight after async ffmpeg check
    void createSession();
    void startPlatformRecording();
    void stopPlatformRecording();
    void finalizeSession();

    // ── Helpers ─────────────────────────────────────────────────────────
    QString sessionDir() const;
    // Single gateway for all failure paths — emits captureFailed BEFORE
    // transitioning to Failed.  5WHY: was copy-pasted 6x with ordering
    // comments at each site; extracting here guarantees it once.
    void failCapture(const QString& errorCode, const QString& message);
    void appendToManifest(const QString& description, const QString& filePath);
    // 5WHY: platformRestoreBrightness + platformDisableFocusMode was
    // copy-pasted at 6 call sites (destructor, cancel, Completed, Failed,
    // Cancelled, onStateChanged).  Centralize so new terminal states
    // cannot forget the restore pair.
    void restoreSystemState();
    // 5WHY: iOS log/capture files must live under DocumentsLocation (accessible
    // via Files.app), while desktop uses AppDataLocation.  Centralize the #ifdef
    // so finishPreflight() and createSession() don't duplicate the path logic.
    static QString captureBasePath();

    // Cached scenario — built once in startCapture, filtered by mode.
    // executeNextStep and executeStep both use this filtered copy so
    // step indices never desync when recordingOnly steps are skipped.
    CaptureScenario m_filteredScenario;  // filtered by capture mode

    // ── Owned components ────────────────────────────────────────────────
    AppState*             m_appState = nullptr;
    CaptureStateMachine*  m_stateMachine = nullptr;
    NavigationAdapter*    m_navAdapter = nullptr;
    ScrollController*     m_scrollCtrl = nullptr;
    QTimer*               m_delayTimer = nullptr;  // for inter-step delays

    // ── State ───────────────────────────────────────────────────────────
    int           m_captureMode = 0;
    int           m_currentStep = 0;
    int           m_totalSteps = 0;
    int           m_captureCount = 0;
    QString       m_diagUrl;
    QString       m_currentAction;
    QString       m_sessionDir;
    bool          m_recording = false;  // true if mode is RecordingOnly or Both
    bool          m_doScreenshot = false; // true if mode is ScreenshotOnly or Both
    int           m_countdownGen = 0;     // incremented per CountdownToStart entry; prevents stale safety timers
    QTimer*       m_pollTimer = nullptr;  // non-null during WaitDiagComplete polling; stopped/cleared on cancel
    QElapsedTimer m_elapsed;           // started in startCapture, read by elapsedSeconds()
};
