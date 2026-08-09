// =============================================================================
// ConfigurationController.cpp — Configuration page controller
// =============================================================================
#include "Configuration/Controller/ConfigurationController.h"
#include "app/AppState.h"
#include <QSettings>

static constexpr const char* kSettingsGroup = "AppSettings";

ConfigurationController::ConfigurationController(AppState* appState, QObject* parent)
    : QObject(parent), m_appState(appState)
{
    // 5WHY: enableDefaultGroups() is already invoked by DiagnosticConfig's
    // constructor — calling it again here was a redundant double-init.
}

bool ConfigurationController::isDiagEnabled(int diagIdInt) const { return m_config.isDiagEnabled(diagIdInt); }

bool ConfigurationController::setDiagEnabled(int diagIdInt, bool enabled) {
    if (!m_config.setDiagEnabled(diagIdInt, enabled)) return false;
    saveSettings();
    return true;
}

bool ConfigurationController::setGroupEnabled(int groupInt, bool enabled) {
    if (!m_config.setGroupEnabled(groupInt, enabled)) return false;
    saveSettings();
    return true;
}

bool ConfigurationController::isGroupAllEnabled(int groupInt) const { return m_config.isGroupAllEnabled(groupInt); }
bool ConfigurationController::isGroupAnyEnabled(int groupInt) const { return m_config.isGroupAnyEnabled(groupInt); }

void ConfigurationController::setGroupActive(int groupInt, bool active) {
    m_appState->setGroupActive(groupInt, active);
}

bool ConfigurationController::isGroupActive(int groupInt) const {
    return m_appState->isGroupActive(groupInt);
}

void ConfigurationController::loadSettings() {
    QSettings s;
    s.beginGroup(QString::fromLatin1(kSettingsGroup));
    QStringList enabledStrs = s.value("enabledDiags").toStringList();
    if (!enabledStrs.isEmpty()) {
        int diagCount = DiagnosticConfig::allDiagIds().size();
        for (int i = 0; i < diagCount; ++i) m_config.setDiagEnabled(i, false);
        for (const auto& str : enabledStrs) {
            bool ok = false; int id = str.toInt(&ok);
            if (ok) m_config.setDiagEnabled(id, true);
        }
    }
    s.endGroup();
}

void ConfigurationController::saveSettings() {
    QSettings s;
    s.beginGroup(QString::fromLatin1(kSettingsGroup));
    QStringList enabledStrs;
    for (auto id : m_config.enabledDiags())
        enabledStrs.append(QString::number(static_cast<int>(id)));
    s.setValue("enabledDiags", enabledStrs);
    s.endGroup();
}
