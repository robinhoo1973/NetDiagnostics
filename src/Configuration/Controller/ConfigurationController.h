// =============================================================================
// ConfigurationController.h — Configuration page controller
//
// Owns: DiagnosticConfig (test enable/disable, port-scan settings)
// Accesses: AppState for persistence (saveSettings)
// =============================================================================
#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QSet>
#include "Configuration/Model/DiagnosticConfig.h"

class AppState;

class ConfigurationController : public QObject {
    Q_OBJECT

public:
    explicit ConfigurationController(AppState* appState, QObject* parent = nullptr);

    // Test enable/disable
    Q_INVOKABLE bool isDiagEnabled(int diagIdInt) const;
    Q_INVOKABLE bool setDiagEnabled(int diagIdInt, bool enabled);
    Q_INVOKABLE bool setGroupEnabled(int groupInt, bool enabled);
    // Memory-only group enable — used for G4/G5 runtime availability when a
    // target is present. NEVER persists (unlike setGroupEnabled), so a
    // transient target state can't pollute QSettings.
    bool setAutomaticGroupEnabled(int groupInt, bool enabled);
    Q_INVOKABLE bool isGroupAllEnabled(int groupInt) const;
    Q_INVOKABLE bool isGroupAnyEnabled(int groupInt) const;

    // Group active management
    Q_INVOKABLE void setGroupActive(int groupInt, bool active);
    Q_INVOKABLE bool isGroupActive(int groupInt) const;

    // Access to underlying config
    DiagnosticConfig& config() { return m_config; }
    const DiagnosticConfig& config() const { return m_config; }

    // Persistence helpers
    void loadSettings();
    void saveSettings();

signals:
    void groupActiveChanged();

private:
    AppState* m_appState;
    DiagnosticConfig m_config;
};
