// =============================================================================
// CaptureStateMachine.h — 11-state FSM for automated evidence capture
// =============================================================================
// Design ref: docs/AutomatedEvidenceCapture_Design.md §4.2
//
// State diagram:
//   Idle → Preflight → CountdownToStart → CreatingSession → StartingRecording*
//   → ExecutingSteps → StoppingRecording* → Finalizing → Completed
//   (* = only in recording / both modes)
//   Any active state → Cancelled (except Idle/Completed/Failed/Cancelled)
//   Any active state → Failed (on error)
// =============================================================================
#pragma once

#include <QObject>
#include <QMap>
#include <QSet>

enum class CaptureState {
    Idle = 0,
    Preflight,
    CountdownToStart,
    CreatingSession,
    StartingRecording,
    ExecutingSteps,
    StoppingRecording,
    Finalizing,
    Completed,
    Cancelled,
    Failed
};

class CaptureStateMachine : public QObject {
    Q_OBJECT
    Q_PROPERTY(int state READ stateInt NOTIFY stateChanged)

public:
    explicit CaptureStateMachine(QObject* parent = nullptr);

    CaptureState state() const { return m_state; }
    int stateInt() const { return static_cast<int>(m_state); }

    // Returns false if transition is illegal. Emits stateChanged on success.
    bool transitionTo(CaptureState target);

    // Check without performing the transition.
    bool canTransitionTo(CaptureState target) const;

    // Is the machine in a terminal state?
    bool isTerminal() const;

    // Is the machine currently running (i.e., capture in progress)?
    bool isRunning() const;

    // Reset back to Idle from any terminal state.
    void reset();

signals:
    void stateChanged(int from, int to);

private:
    CaptureState m_state = CaptureState::Idle;

    static QMap<CaptureState, QSet<CaptureState>> buildTransitionTable();
    static const QMap<CaptureState, QSet<CaptureState>> kTransitions;
};
