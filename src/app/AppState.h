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
#include <QElapsedTimer>
#include <QTimer>

#include "Common/Model/DiagId.h"
#include "Common/Model/DiagnosticResult.h"
#include "Common/Services/MonotonicClock.h"

class DiagnosticSuite;
class ConfigurationController;
class PremiumStore;

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
    // 目标凭据（target 设置弹窗：FTP/SSH/DB 等协议认证）
    QString targetUser() const { return m_targetUser; }
    QString targetPassword() const { return m_targetPassword; }
    QString targetPort() const { return m_targetPort; }
    bool    targetHasCredentials() const { return !m_targetUser.isEmpty() || !m_targetPort.isEmpty(); }

    // ── P1：Config/语言/主题 ──
    int     stateVersion() const { return m_stateVersion; }
    int     languageIndex() const { return m_languageIndex; }
    int     themeMode() const { return m_themeMode; }
    bool    isPremiumPlatform() const { return m_isPremiumPlatform; }
    bool    cellularWarnVisible() const { return m_cellularWarnVisible; }
    bool    hasData() const { return !m_results.isEmpty(); }
    qint64  monotonicNowMs() const { return monotonicMsSinceAppStart(); }
    QStringList langItems() const;

    Q_PROPERTY(int runStatus READ runStatus NOTIFY runStatusChanged)
    Q_PROPERTY(int currentRunningGroup READ currentRunningGroup NOTIFY currentRunningGroupChanged)
    Q_PROPERTY(int totalCompleted READ totalCompleted NOTIFY progressChanged)
    Q_PROPERTY(QString currentDiagLabel READ currentDiagLabel NOTIFY progressChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY runStatusChanged)
    Q_PROPERTY(QString targetHost READ targetHost NOTIFY targetChanged)
    Q_PROPERTY(QString targetPath READ targetPath NOTIFY targetChanged)
    Q_PROPERTY(QString targetValidationErrorText READ targetValidationErrorText NOTIFY targetChanged)
    Q_PROPERTY(QString targetUser READ targetUser NOTIFY targetChanged)
    Q_PROPERTY(QString targetPassword READ targetPassword NOTIFY targetChanged)
    Q_PROPERTY(QString targetPort READ targetPort NOTIFY targetChanged)
    Q_PROPERTY(bool targetHasCredentials READ targetHasCredentials NOTIFY targetChanged)
    Q_PROPERTY(int stateVersion READ stateVersion NOTIFY stateVersionChanged)
    Q_PROPERTY(int languageIndex READ languageIndex NOTIFY languageChanged)
    Q_PROPERTY(QStringList langItems READ langItems NOTIFY languageChanged)
    Q_PROPERTY(int themeMode READ themeMode NOTIFY themeModeChanged)
    Q_PROPERTY(bool isPremiumPlatform READ isPremiumPlatform CONSTANT)
    Q_PROPERTY(bool cellularWarnVisible READ cellularWarnVisible NOTIFY cellularWarnVisibleChanged)
    // 5WHY (复核 2026-08-20 墙钟步进): 进程级单调毫秒（MonotonicClock 基准，
    // 无 NOTIFY——QML 以每秒 _elapsed tick 为依赖钩读取现值；NTP/手动校时
    // 步进不影响计时圆点与颜色阈值）。
    Q_PROPERTY(qint64 monotonicNowMs READ monotonicNowMs)
    Q_PROPERTY(bool hasData READ hasData NOTIFY progressChanged)

    Q_INVOKABLE void runDiagnostics();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void continueAfterCellularWarn();   // 移动数据警告确认（H1）
    // 5WHY (review round 3): 遮罩点击应仅关闭警告而不确认运行——误触遮罩即
    // 启动整轮大流量诊断不可接受，故提供与"确认"分离的纯关闭语义。
    Q_INVOKABLE void dismissCellularWarn();
    Q_INVOKABLE QVariantList allDiagsForGroup(int groupInt) const;
    Q_INVOKABLE QVariantList resultsForGroup(int groupInt) const;
    Q_INVOKABLE QVariantMap groupStats(int groupInt) const;   // -1 = 聚合
    Q_INVOKABLE QVariantList visibleGroups() const;
    Q_INVOKABLE QVariantMap resultFor(int diagIdInt) const;
    Q_INVOKABLE QVariantMap contractFor(int diagIdInt) const;   // A3 视图（DetailPage 区块开关）
    Q_INVOKABLE void setTarget(const QString& host, const QString& scheme);
    Q_INVOKABLE void setTargetCredentials(const QString& user, const QString& password, const QString& port);
    Q_INVOKABLE QString targetScheme() const { return m_targetScheme; }
    Q_INVOKABLE qint64 runDurationMs() const;
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
    Q_INVOKABLE QString buildReportHtml() const;   // HTML 报告（ReportEngine 最小恢复）

    // ── 报告/分享/动画（ReportEngine + Premium 后端恢复）──
    Q_INVOKABLE QString diagAnimationUrl(int diagIdInt) const;
    // 5WHY (复核 2026-08-19): 动画锚点元数据（母版 SVG 几何事实）与 URL
    // 同层单一来源——DiagAnimator 装载时下发（GeoRadar: cx/cy/maxR）。
    Q_INVOKABLE QVariantMap diagAnimationAnchor(int diagIdInt) const;
    Q_INVOKABLE QString previewReportHtml() const;         // 预览 HTML（RichText 子集）
    Q_INVOKABLE QString renderPreviewImage(int widthPx) const;  // 预览 PNG 路径
    Q_INVOKABLE QString exportHtmlReport() const;
    Q_INVOKABLE QString exportPdfReport() const;
    Q_INVOKABLE QString shareReportFile(const QString& format);  // text/html/pdf（非 const：text 写剪贴板）
    Q_INVOKABLE QString appVersion() const { return QStringLiteral(PROJECT_VERSION); }
    Q_INVOKABLE QString appEdition() const { return QStringLiteral(APP_EDITION); }
    Q_INVOKABLE QString buildNumber() const { return QStringLiteral(ND_BUILD_NUMBER); }
    Q_INVOKABLE QString gitHash() const {
#if defined(ND_GIT_HASH)
        return QStringLiteral(ND_GIT_HASH);
#else
        return QStringLiteral("dev");
#endif
    }
    PremiumStore* premiumStore() const { return m_premiumStore; }

signals:
    void progressChanged();
    void runStatusChanged();
    void currentRunningGroupChanged();
    void runElapsedChanged();
    void targetChanged();
    void stateVersionChanged();
    // 5WHY (复核 2026-08-18 语义信号): 过滤派生数据（groupStats/allDiagsForGroup/
    // resultsForGroup/visibleGroups）曾靠消费方自行拼凑 superset 信号组合
    // （targetChanged + stateVersionChanged 双发双刷、setLanguage/setCredentials
    // 误触发、DashboardScreen 漏接）。语义信号只在过滤集实际变化处发射
    // （scheme 变更 / 激活组变更），消费方单信号单次刷新。
    void filteredDataChanged();
    void languageChanged();
    void themeModeChanged();
    void cellularWarnVisibleChanged();

private:
    void runNextGroup();
    void onSuiteFinished();
    void updateItemModel(DiagId id, const DiagnosticResult& r);
    QVariantMap itemFor(DiagId id) const;
    void loadPreferences();
    void savePreferences();
    void bumpState();
    // 队列化语义信号广播（5WHY 复核 2026-08-19 反模式 #4：QML 点击栈上
    // 同步发射会驱动 Repeater 委托销毁；延迟一帧语义不变）
    void queueFilteredChanged();
    void queueStateBroadcast();
    // 结果持久化（ProbeDatabase 磁盘缓存能力恢复：重启后保留上次结果）
    void persistResults();
    void loadCachedResults();

    QString m_targetHost;
    QString m_targetPath;
    QString m_targetError;
    QString m_targetScheme = QStringLiteral("https");
    // 目标凭据（持久化；凭据仅注入运行 target，不进 UI 展示）
    QString m_targetUser;
    QString m_targetPassword;
    QString m_targetPort;

    int  m_runStatus = Idle;
    int  m_currentGroup = -1;
    QString m_currentDiagLabel;
    QString m_errorMessage;

    // ── P1：Config/语言/主题 ──
    ConfigurationController* m_config = nullptr;
    PremiumStore* m_premiumStore = nullptr;   // Premium 后端（Settings 购买/恢复）
    QSet<int> m_activeGroups;      // 默认全激活，持久化
    int  m_stateVersion = 0;
    int  m_languageIndex = 7;      // 与 translations.json 一致：7 = English
    int  m_themeMode = 2;          // 2 = Dark（ThemeEngine.drkMode）
    bool m_isPremiumPlatform = false;

    QHash<DiagId, DiagnosticResult> m_results;
    QHash<int, bool> m_groupDone;      // group → finished (any status)

    DiagnosticSuite* m_suite = nullptr;
    QVector<int> m_pendingGroups;

    // H3：跨 run 结果污染防护——每次 run 递增，迟到信号按 generation 丢弃
    qint64 m_runGeneration = 0;
    // 8-15：运行墙钟计时（诊断运行时间）——仪表盘总览实时刷新
    QElapsedTimer m_runTimer;
    qint64 m_runElapsedMs = 0;
    QTimer* m_elapsedTicker = nullptr;
    // H1：移动数据警告（G3 起大流量探测前暂停，移动/Apple 平台）
    bool m_cellularWarnVisible = false;
    bool m_cellularWarnAcked = false;
    // 5WHY (复核 2026-08-20 去重): 队列广播待发标志——runNextGroup 的组推进
    // 路径与 runDiagnostics 主路径广播同帧双发 currentRunningGroupChanged
    // （面板双载）。主路径广播待发时组推进跳过自有发射；lambda 送达即清。
    bool m_broadcastPending = false;
};
