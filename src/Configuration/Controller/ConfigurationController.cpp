// =============================================================================
// ConfigurationController.cpp — Configuration page controller
// =============================================================================
#include "Configuration/Controller/ConfigurationController.h"
#include "app/AppState.h"
#include "Common/Services/DiagnosticBase.h"   // 5WHY (2026-09-05): 可运行性单一入口
#include "Common/Utils/SettingsKeys.h"   // simplify: 组名单一来源（与 AppState 共用）
#include <QSettings>

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
    // 5WHY (2026-09-05 全禁后重启复活): 曾以"空列表"当"无存档"哨兵——空
    // 列表也是"用户禁用了全部检测"的合法序列化形态（INI 后端把空
    // QStringList 存为 @Invalid()，读回仍是空列表，与缺键不可区分）→
    // 全禁后重启被 enableDefaultGroups 静默复活。键存在性才是唯一可靠
    // 哨兵：exists 检查区分"没存过"与"存了空集"。
    if (s.contains(QStringLiteral("enabledDiags"))) {
        QStringList enabledStrs = s.value("enabledDiags").toStringList();
        int diagCount = DiagnosticConfig::allDiagIds().size();
        for (int i = 0; i < diagCount; ++i) m_config.setDiagEnabled(i, false);
        for (const auto& str : enabledStrs) {
            bool ok = false; int id = str.toInt(&ok);
            // 5WHY: drop ids for tests that cannot run on this OS build
            // (e.g. a config exported on desktop enabling an iOS-only
            // diagnostic would otherwise linger in the enabled set).
            // 5WHY (2026-09-05 可运行性单一入口): 曾仅查 DeviceCapability
            // （除 3 个硬件探测外恒 true）——平台无适配器的 id（如桌面
            // 导出的 G5Mysql 在 iOS 恢复）被静默复活为"永远自跳过"的
            // 幽灵启用项。与调度/可见性同源：registry select + capability。
            if (ok && DiagnosticBase::runnable(static_cast<DiagId>(id)))
                m_config.setDiagEnabled(id, true);
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
