// =============================================================================
// CaptureStateMachine.cpp — implementation
// =============================================================================
#include "EvidenceCapture/CaptureStateMachine.h"

QMap<CaptureState, QSet<CaptureState>> CaptureStateMachine::buildTransitionTable() {
    QMap<CaptureState, QSet<CaptureState>> t;

    // Idle → Preflight (start capture flow)
    t[CaptureState::Idle]    = {CaptureState::Preflight};

    // Preflight → CountdownToStart (checks passed), Cancelled, Failed
    t[CaptureState::Preflight] = {CaptureState::CountdownToStart,
                                   CaptureState::Cancelled,
                                   CaptureState::Failed};

    // CountdownToStart → CreatingSession, Cancelled, Failed
    t[CaptureState::CountdownToStart] = {CaptureState::CreatingSession,
                                          CaptureState::Cancelled,
                                          CaptureState::Failed};

    // CreatingSession → StartingRecording (recording modes) or ExecutingSteps (screenshot-only), Failed
    t[CaptureState::CreatingSession] = {CaptureState::StartingRecording,
                                         CaptureState::ExecutingSteps,
                                         CaptureState::Failed};

    // StartingRecording → ExecutingSteps, Failed, Cancelled
    t[CaptureState::StartingRecording] = {CaptureState::ExecutingSteps,
                                           CaptureState::Cancelled,
                                           CaptureState::Failed};

    // ExecutingSteps → StoppingRecording (recording modes) or Finalizing (screenshot-only), Failed, Cancelled
    t[CaptureState::ExecutingSteps] = {CaptureState::StoppingRecording,
                                        CaptureState::Finalizing,
                                        CaptureState::Cancelled,
                                        CaptureState::Failed};

    // StoppingRecording → Finalizing, Failed
    t[CaptureState::StoppingRecording] = {CaptureState::Finalizing,
                                           CaptureState::Failed};

    // Finalizing → Completed, Failed
    t[CaptureState::Finalizing] = {CaptureState::Completed,
                                    CaptureState::Failed};

    // Terminal states → reset() returns to Idle
    t[CaptureState::Completed]  = {};
    t[CaptureState::Cancelled]  = {};
    t[CaptureState::Failed]     = {};

    return t;
}

const QMap<CaptureState, QSet<CaptureState>> CaptureStateMachine::kTransitions =
    CaptureStateMachine::buildTransitionTable();

CaptureStateMachine::CaptureStateMachine(QObject* parent)
    : QObject(parent) {}

bool CaptureStateMachine::canTransitionTo(CaptureState target) const {
    auto it = kTransitions.constFind(m_state);
    if (it == kTransitions.constEnd()) return false;
    return it->contains(target);
}

bool CaptureStateMachine::transitionTo(CaptureState target) {
    if (!canTransitionTo(target)) {
        return false;
    }
    CaptureState old = m_state;
    m_state = target;
    emit stateChanged(static_cast<int>(old), static_cast<int>(target));
    return true;
}

bool CaptureStateMachine::isTerminal() const {
    return m_state == CaptureState::Completed
        || m_state == CaptureState::Cancelled
        || m_state == CaptureState::Failed;
}

bool CaptureStateMachine::isRunning() const {
    return m_state != CaptureState::Idle && !isTerminal();
}

void CaptureStateMachine::reset() {
    if (!isTerminal() && m_state != CaptureState::Idle) return; // safety: only reset from terminal
    m_state = CaptureState::Idle;
    emit stateChanged(-1, static_cast<int>(CaptureState::Idle));
}
