// =============================================================================
// DashboardController.cpp — Dashboard page controller
// =============================================================================
#include "Dashboard/Controller/DashboardController.h"
#include "app/AppState.h"

DashboardController::DashboardController(AppState* appState, QObject* parent)
    : QObject(parent), m_appState(appState) {}

int DashboardController::totalCompleted() const { return m_appState->totalCompleted(); }
int DashboardController::totalDiags() const { return m_appState->totalDiags(); }
QVariantList DashboardController::allGroupStats() const { return m_appState->allGroupStats(); }
QStringList DashboardController::groupLabels() const { return m_appState->groupLabels(); }
QVariantList DashboardController::resultsForGroup(int g) const { return m_appState->resultsForGroup(g); }
QVariantList DashboardController::allDiagsForGroup(int g) const { return m_appState->allDiagsForGroup(g); }
QVariantList DashboardController::allDiagIdsForGroup(int g) const { return m_appState->allDiagIdsForGroup(g); }
QVariantList DashboardController::visibleGroups() const { return m_appState->visibleGroups(); }
QVariantMap DashboardController::groupStats(int g) const { return m_appState->groupStats(g); }
QVariantMap DashboardController::getDetailResult(int id) const { return m_appState->getDetailResult(id); }
void DashboardController::showDetailDialog(int diagIdInt) { m_appState->showDetailDialog(diagIdInt); }
