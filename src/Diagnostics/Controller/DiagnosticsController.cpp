// =============================================================================
// DiagnosticsController.cpp — Diagnostics page controller
// =============================================================================
#include "Diagnostics/Controller/DiagnosticsController.h"
#include "app/AppState.h"

DiagnosticsController::DiagnosticsController(AppState* appState, QObject* parent)
    : QObject(parent), m_appState(appState)
{
    // Forward AppState signals to DiagnosticsController signals so QML can bind
    // to this controller instead of reaching into AppState directly.
    connect(m_appState, &AppState::runStatusChanged,
            this, &DiagnosticsController::runStatusChanged);
    connect(m_appState, &AppState::progressChanged,
            this, &DiagnosticsController::progressChanged);
    connect(m_appState, &AppState::currentDiagChanged,
            this, &DiagnosticsController::currentDiagChanged);
    connect(m_appState, &AppState::groupChanged,
            this, &DiagnosticsController::groupChanged);
    connect(m_appState, &AppState::cellularWarnVisibleChanged,
            this, &DiagnosticsController::cellularWarnChanged);
    // AppState uses runStatusChanged as the errorMessage NOTIFY; forward it to
    // our dedicated errorChanged signal so QML bindings re-evaluate errorMessage.
    connect(m_appState, &AppState::runStatusChanged,
            this, &DiagnosticsController::errorChanged);
}

void DiagnosticsController::runDiagnostics() { m_appState->runDiagnostics(); }
void DiagnosticsController::cancel() { m_appState->cancel(); }

int DiagnosticsController::runStatus() const
{
    return static_cast<int>(m_appState->runStatus());
}

int DiagnosticsController::totalCompleted() const
{
    return m_appState->totalCompleted();
}

QString DiagnosticsController::currentDiagLabel() const
{
    return m_appState->currentDiagLabel();
}

QString DiagnosticsController::currentGroup() const
{
    return m_appState->currentGroup();
}

QString DiagnosticsController::errorMessage() const
{
    return m_appState->errorMessage();
}

bool DiagnosticsController::cellularWarnVisible() const
{
    return m_appState->cellularWarnVisible();
}
