// =============================================================================
// AppState.h — QML 可追踪的应用状态桥接层（UI 重建 P0）
//
// 观察 DiagnosticSuite 的五个组运行，向 QML 暴露：
//   · runStatus / currentRunningGroup / totalCompleted / currentDiagLabel
//     / errorMessage（Q_PROPERTY 只读 + NOTIFY，QML 可追踪——UI-2）
//   · allDiagsForGroup / groupStats / visibleGroups / resultFor（Q_INVOKABLE，
//     只在 JS 命令式处理器中调用，禁止出现在绑定表达式——UI-2）
// 运行协议：runDiagnostics() 按 G1..G5 顺序逐组执行（与 selftest 相同的
// 组顺序），每组结果实时更新 items model 与统计。
// =============================================================================
#pragma once

#include <QObject>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

#include "Common/Model/DiagId.h"
#include "Common/Model/DiagnosticResult.h"

class DiagnosticSuite;
class ConfigurationController;

class AppState : public QObject {
    Q_OBJECT
public:
    enum RunStatus { Idle = 0, Running = 1, Completed = 2, Cancelled = 3, Error = 4 };
    Q_ENUM(RunStatus)

    explicit AppState(QObject* parent = nullptr);

    int     runStatus() const { return m_runStatus; }
    int     currentRunningGroup() const { return m_currentGroup; }
    int     totalCompleted() const { return m_results.size(); }
    QString currentDiagLabel() const { return m_currentDiagLabel; }
    QString errorMessage() const { return m_errorMessage; }
    QString targetHost() const { return m_targetHost; }
    QString targetPath() const { return m_targetPath; }
    QString targetValidationErrorText() const { return m_targetError; }

    // ── P1：Config/语言/主题 ──
    int     stateVersion() const { return m_stateVersion; }
    int     languageIndex() const { return m_languageIndex; }
    int     themeMode() const { return m_themeMode; }
    bool    isPremiumPlatform() const { return m_isPremiumPlatform; }
    bool    hasData() const { return !m_results.isEmpty(); }
    QStringList langItems() const;

    Q_PROPERTY(int runStatus READ runStatus NOTIFY runStatusChanged)
    Q_PROPERTY(int currentRunningGroup READ currentRunningGroup NOTIFY currentRunningGroupChanged)
    Q_PROPERTY(int totalCompleted READ totalCompleted NOTIFY progressChanged)
    Q_PROPERTY(QString currentDiagLabel READ currentDiagLabel NOTIFY progressChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY runStatusChanged)
    Q_PROPERTY(QString targetHost READ targetHost NOTIFY targetChanged)
    Q_PROPERTY(QString targetPath READ targetPath NOTIFY targetChanged)
    Q_PROPERTY(QString targetValidationErrorText READ targetValidationErrorText NOTIFY targetChanged)
    Q_PROPERTY(int stateVersion READ stateVersion NOTIFY stateVersionChanged)
    Q_PROPERTY(int languageIndex READ languageIndex NOTIFY languageChanged)
    Q_PROPERTY(QStringList langItems READ langItems NOTIFY languageChanged)
    Q_PROPERTY(int themeMode READ themeMode NOTIFY themeModeChanged)
    Q_PROPERTY(bool isPremiumPlatform READ isPremiumPlatform CONSTANT)
    Q_PROPERTY(bool hasData READ hasData NOTIFY progressChanged)

    Q_INVOKABLE void runDiagnostics();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE QVariantList allDiagsForGroup(int groupInt) const;
    Q_INVOKABLE QVariantList resultsForGroup(int groupInt) const;
    Q_INVOKABLE QVariantMap groupStats(int groupInt) const;   // -1 = 聚合
    Q_INVOKABLE QVariantList visibleGroups() const;
    Q_INVOKABLE QVariantMap resultFor(int diagIdInt) const;
    Q_INVOKABLE QVariantMap contractFor(int diagIdInt) const;   // A3 视图（DetailPage 区块开关）
    Q_INVOKABLE void setTarget(const QString& host, const QString& scheme);
    Q_INVOKABLE QStringList supportedSchemes() const;

    // ── P1：Config 页桥接 ──
    Q_INVOKABLE QVariantList allDiagIdsForGroup(int groupInt) const;
    Q_INVOKABLE int diagCountForGroup(int groupInt) const;
    Q_INVOKABLE bool isDiagEnabled(int diagIdInt) const;
    Q_INVOKABLE bool setDiagEnabled(int diagIdInt, bool enabled);
    Q_INVOKABLE bool setGroupEnabled(int groupInt, bool enabled);
    Q_INVOKABLE bool isGroupAllEnabled(int groupInt) const;
    Q_INVOKABLE bool isGroupAnyEnabled(int groupInt) const;
    Q_INVOKABLE void setGroupActive(int groupInt, bool active);
    Q_INVOKABLE bool isGroupActive(int groupInt) const;
    Q_INVOKABLE void setLanguage(int index);
    Q_INVOKABLE void setThemeMode(int mode);

    // ── 剪贴板（Detail 复制 / 报告分享）──
    Q_INVOKABLE void copyDetailToClipboard(int diagIdInt);
    Q_INVOKABLE void copyReportToClipboard();
    Q_INVOKABLE QString buildReportText() const;

signals:
    void progressChanged();
    void runStatusChanged();
    void currentRunningGroupChanged();
    void targetChanged();
    void stateVersionChanged();
    void languageChanged();
    void themeModeChanged();

private:
    void runNextGroup();
    void onSuiteFinished();
    void updateItemModel(DiagId id, const DiagnosticResult& r);
    QVariantMap itemFor(DiagId id) const;
    void loadPreferences();
    void savePreferences();
    void bumpState();

    QString m_targetHost;
    QString m_targetPath;
    QString m_targetError;
    QString m_targetScheme = QStringLiteral("https");

    int  m_runStatus = Idle;
    int  m_currentGroup = -1;
    QString m_currentDiagLabel;
    QString m_errorMessage;

    // ── P1：Config/语言/主题 ──
    ConfigurationController* m_config = nullptr;
    QSet<int> m_activeGroups;      // 默认全激活，持久化
    int  m_stateVersion = 0;
    int  m_languageIndex = 7;      // 与 translations.json 一致：7 = English
    int  m_themeMode = 2;          // 2 = Dark（ThemeEngine.drkMode）
    bool m_isPremiumPlatform = false;

    QHash<DiagId, DiagnosticResult> m_results;
    QHash<int, bool> m_groupDone;      // group → finished (any status)

    DiagnosticSuite* m_suite = nullptr;
    QVector<int> m_pendingGroups;
};
