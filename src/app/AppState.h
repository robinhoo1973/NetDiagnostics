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
#include <atomic>

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
    // 5WHY (simplify 2026-09-05 每 tick 双扫): QML 统计面板每 progressChanged
    // 调用 groupStats（C++ 全量重建）再 statsEqual 判等——门付出了全量重建
    // 才发现相等。结果插入/清屏递增版本号，QML 以 int 读取早退（无 NOTIFY，
    // 进度信号本身即读取钩子）。
    int     statsVersion() const { return m_statsVersion; }
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
    Q_PROPERTY(int statsVersion READ statsVersion)
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
    // 5WHY (simplify 2026-09-05): contractFor Q_INVOKABLE 删除——零调用方，
    // 契约经 resultFor 的 DetailProfile 下发（见 AppState.cpp 同段注释）。
    Q_INVOKABLE void setTarget(const QString& host, const QString& scheme);
    Q_INVOKABLE void setTargetCredentials(const QString& user, const QString& password, const QString& port);
    Q_INVOKABLE QString targetScheme() const { return m_targetScheme; }
    Q_INVOKABLE qint64 runDurationMs() const;
    Q_INVOKABLE QStringList supportedSchemes() const;
    // 桌面窗口几何持久化（P0-2, review/ui-ux-audit-plan §4）：restore 返回
    // {x,y,width,height,maximized}，无存档时各键缺省（QML 侧走默认布局）。
    Q_INVOKABLE QVariantMap restoreWindowGeometry() const;
    Q_INVOKABLE void saveWindowGeometry(int x, int y, int width, int height, bool maximized);
    // 5WHY (2026-09-05 最大化几何污染): 关闭时若窗口处于最大化，QML 的
    // x/y/width/height 读到的就是最大化帧——把它当"还原尺寸"落盘后，
    // 还原按钮永远回到最大化尺寸。最大化关闭只落 max 标志，不动已存的
    // 正常几何。
    Q_INVOKABLE void saveWindowMaximized(bool maximized);

    // ── P1：Config 页桥接 ──
    Q_INVOKABLE QVariantList allDiagIdsForGroup(int groupInt) const;
    // 5WHY (simplify 2026-09-05): diagCountForGroup 删除——零调用方（见 .cpp 注释）。
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
    // 5WHY (simplify 2026-09-05): buildReportHtml 删除——零调用方，活跃
    // 路径为 previewReportHtml + ReportEngine（见 AppState.cpp 同段注释）。

    // ── 报告/分享/动画（ReportEngine + Premium 后端恢复）──
    Q_INVOKABLE QString diagAnimationUrl(int diagIdInt) const;
    // 5WHY (复核 2026-08-20 锚点管道删除): 锚点几何单一来源为
    // AnimationTokens.js（QML 默认直读，见 AppState.cpp 同段注释）——
    // C++ 下发链（diagAnimationAnchor + DiagAnimator 注入）删除。
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
    // 5WHY (2026-08-23 点击 Run 3 秒才启动): 同步 isOnWifi/hasCellularUp
    // 曾阻塞主线程（iOS iosCopyWiFiSSID semaphore 最长 5s）——改后台
    // 刷新 + Run 时零阻塞读缓存（m_wifiUp/m_cellularUp）。
    void refreshConnectivityAsync();
    QVariantMap itemFor(DiagId id, const QHash<DiagId, qint64>* startsMono = nullptr) const;
    void loadPreferences();
    void savePreferences();
    // H2 (5WHY): 密码持久化只走平台安全存储——仅在凭证实际变更时调用
    // （setTargetCredentials），避免每次主题/语言切换都写 Keychain/DPAPI；
    // 成功后同步清除 QSettings 遗留明文键，防止迁移回退复活旧密码。
    void persistCredentials();
    void bumpState();
    // 队列化语义信号广播（5WHY 复核 2026-08-19 反模式 #4：QML 点击栈上
    // 同步发射会驱动 Repeater 委托销毁；延迟一帧语义不变）
    void queueFilteredChanged();
    void queueStateBroadcast();
    // 结果持久化（ProbeDatabase 磁盘缓存能力恢复：重启后保留上次结果）
    void persistResults();
    void loadCachedResults();
    // 5WHY (2026-08-22 P0-2): 出口红线——报告/剪贴板/落盘/预览统一在
    // 出口处替换 user:pass@ 为 user:***@（探针层与屏幕终端保持历史原样）。
    QString redactCredentials(const QString& text) const;
    struct CredForms { QString auth; QString masked; };
    CredForms credentialForms() const;

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
    int  m_statsVersion = 0;    // 结果插入/清屏递增（QML 统计门早退）
    int  m_languageIndex = 7;      // 与 translations.json 一致：7 = English
    int  m_themeMode = 0;          // 0 = System（跟随 OS 深浅；1=Light 2=Dark）
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
    // 连通性异步缓存（后台刷新，Run 时零阻塞读）：默认假定 WiFi 连接
    // ——首轮刷新落地前不误弹流量警告（宁可漏提不可误提）。
    std::atomic<bool> m_wifiUp{true};
    std::atomic<bool> m_cellularUp{false};
    // H1 (5WHY): QtConcurrent::run 捕获裸 this——对象析构时线程仍在运行
    // 导致 use-after-free。守卫在 refreshConnectivityAsync() 内以局部
    // QPointer 创建并捕获（5WHY 2026-09-04 复核：曾加 m_selfGuard 成员
    // 但 lambda 实际捕获的是局部变量，成员是死代码，已移除）。
    // 5WHY (2026-09-04 修正复核): QPointer "检查后解引用"仍是 TOCTOU——对象
    // 可在检查与写入之间被析构。写回改经 QMetaObject::invokeMethod 队列化
    // 回主线程（上下文为 AppState，析构时挂起调用自动丢弃），工作线程
    // 不再解引用对象。
    // 5WHY (复核 2026-08-20 去重): 队列广播待发标志——runNextGroup 的组推进
    // 路径与 runDiagnostics 主路径广播同帧双发 currentRunningGroupChanged
    // （面板双载）。主路径广播待发时组推进跳过自有发射；lambda 送达即清。
    bool m_broadcastPending = false;
};
