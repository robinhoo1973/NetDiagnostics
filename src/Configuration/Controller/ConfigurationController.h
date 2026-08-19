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
    // 5WHY (复核 2026-08-19 死代码清除): setAutomaticGroupEnabled 曾在此声明
    // ——运行时按目标切换 G4/G5 启用的旧机制，后被"默认全启用 + 运行期仅按
    // m_activeGroups 门控"取代（DiagnosticConfig.cpp enableDefaultGroups 注释
    // 记录了该历史）。零调用方 + 绕过 AppState 语义信号封装（旁路隐患），
    // 随本轮清除。
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
