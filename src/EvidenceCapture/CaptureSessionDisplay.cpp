// =============================================================================
// CaptureSessionDisplay.cpp — Display Model implementation
// =============================================================================
#include "EvidenceCapture/CaptureSessionDisplay.h"
#include "EvidenceCapture/CaptureOrchestrator.h"

CaptureSessionDisplay::CaptureSessionDisplay(CaptureOrchestrator* orchestrator,
                                             QObject* parent)
    : QObject(parent)
    , m_orchestrator(orchestrator)
    , m_elapsedTimer(new QTimer(this))
{
    // ── Wire orchestrator signals ────────────────────────────────────
    connect(m_orchestrator, &CaptureOrchestrator::stepChanged,
            this, &CaptureSessionDisplay::onStepChanged);
    connect(m_orchestrator, &CaptureOrchestrator::captureCountChanged,
            this, &CaptureSessionDisplay::onCaptureCountChanged);
    // captureModeChanged fires once in startCapture() — initializes
    // visibility flags; also covers the case where the display is
    // created mid-session (re-reads current mode values).
    connect(m_orchestrator, &CaptureOrchestrator::captureModeChanged,
            this, &CaptureSessionDisplay::onCaptureModeChanged);

    // ── 1-second elapsed timer (replaces QML Timer polling) ──────────
    m_elapsedTimer->setInterval(1000);
    connect(m_elapsedTimer, &QTimer::timeout,
            this, &CaptureSessionDisplay::onElapsedTick);
    // 5WHY: Don't start the timer in the constructor — it runs on
    // captureModeChanged() (first startCapture call) and auto-stops
    // via onElapsedTick() when the FSM reaches a terminal state.
    // Avoids 1s CPU wake-ups for the entire app lifetime.

    // ── Seed with current orchestrator state ─────────────────────────
    // 5WHY: Only seed step display if a session exists (totalSteps > 0).
    // Seeding onStepChanged(0,0) would produce "step 1 of 0" when no
    // capture has been started yet — a misleading initial state.
    if (m_orchestrator->totalSteps() > 0) {
        onStepChanged(m_orchestrator->currentStep(), m_orchestrator->totalSteps());
    }
    onCaptureCountChanged(m_orchestrator->captureCount());
    updateVisibility();
    onElapsedTick();
}

// ═════════════════════════════════════════════════════════════════════════
// Signal handlers
// ═════════════════════════════════════════════════════════════════════════

void CaptureSessionDisplay::onStepChanged(int current, int total) {
    // 5WHY: C++ emits 0-indexed current, N totalSteps.  Display as
    // 1-indexed for the user (step 1 of N).  Both numbers are
    // space-padded for monospace right-alignment in QML.
    QString step = pad2(current + 1);
    QString tot  = pad2(total);
    if (step != m_stepDisplay) {
        m_stepDisplay = step;
        emit stepDisplayChanged();
    }
    if (tot != m_totalDisplay) {
        m_totalDisplay = tot;
        emit totalDisplayChanged();
    }
}

void CaptureSessionDisplay::onCaptureCountChanged(int count) {
    QString disp = pad2(count);
    if (disp != m_countDisplay) {
        m_countDisplay = disp;
        emit countDisplayChanged();
    }
}

void CaptureSessionDisplay::onCaptureModeChanged() {
    updateVisibility();
    // 5WHY: Start the elapsed timer when CaptureOrchestrator begins
    // a new session.  The timer auto-stops in onElapsedTick() when
    // the FSM reaches Idle/Completed/Cancelled/Failed.
    if (!m_elapsedTimer->isActive()) {
        m_elapsedTimer->start();
    }
}

void CaptureSessionDisplay::onElapsedTick() {
    if (!m_orchestrator) return;
    // 5WHY: Auto-stop the timer when the capture FSM is Idle or
    // terminal (Completed/Cancelled/Failed).  Restart happens via
    // onCaptureModeChanged() on the next session.
    // Use isRunning() instead of hardcoded stateInt() magic numbers
    // so the check survives FSM enum renumbering.
    if (!m_orchestrator->isRunning()) {
        m_elapsedTimer->stop();
        return;
    }
    int secs = m_orchestrator->elapsedSeconds();
    int h = secs / 3600;
    int m = (secs % 3600) / 60;
    int s = secs % 60;
    QString disp = pad0(h) + QLatin1String(":") +
                   pad0(m) + QLatin1String(":") +
                   pad0(s);
    if (disp != m_elapsedDisplay) {
        m_elapsedDisplay = disp;
        emit elapsedDisplayChanged();
    }
}

// ═════════════════════════════════════════════════════════════════════════
// Helpers
// ═════════════════════════════════════════════════════════════════════════

void CaptureSessionDisplay::updateVisibility() {
    if (!m_orchestrator) return;
    bool dot = m_orchestrator->isRecordingCapture();
    bool ss  = m_orchestrator->wantsScreenshot();
    if (dot != m_showRecordingDot || ss != m_showScreenshotGroup) {
        m_showRecordingDot    = dot;
        m_showScreenshotGroup = ss;
        emit visibilityFlagsChanged();
    }
}
