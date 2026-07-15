// =============================================================================
// DiagnosticsController.cpp — Diagnostics page controller (MVC stub)
// =============================================================================
#include "Diagnostics/Controller/DiagnosticsController.h"
#include "app/AppState.h"

DiagnosticsController::DiagnosticsController(AppState* appState, QObject* parent)
    : QObject(parent), m_appState(appState) {}

void DiagnosticsController::runDiagnostics() { m_appState->runDiagnostics(); }
void DiagnosticsController::cancel() { m_appState->cancel(); }
QString DiagnosticsController::currentDiagLabel() const { return m_appState->currentDiagLabel(); }
QString DiagnosticsController::currentGroup() const { return m_appState->currentGroup(); }
