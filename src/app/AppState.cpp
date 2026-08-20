// =============================================================================
// AppState.cpp — Suite observer implementation
// =============================================================================
#include "app/AppState.h"

#include "Common/Model/DiagnosticMeta.h"
#include "Common/Model/DiagNames.h"
#include "Common/Model/OutputContract.h"
#include "Common/Platform/DeviceCapability.h"
#include "Common/Platform/PlatformFlags.h"
#include "Common/Services/PlatformAdapter.h"
#include "Configuration/Controller/ConfigurationController.h"
#include "Diagnostics/Model/DiagnosticSuite.h"
#include "Report/Model/ReportEngine.h"
#include "Settings/Model/PremiumStore.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QVector>

#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID)
#include "Common/Platform/PlatformShare.h"   // 移动端系统分享单
#endif

namespace {
// 组索引 → DiagGroup（visibleGroups 用）
DiagGroup groupForIndex(int idx) {
    switch (idx) {
        case 0: return DiagGroup::G1;
        case 1: return DiagGroup::G2;
        case 2: return DiagGroup::G3;
        case 3: return DiagGroup::G4;
        default: return DiagGroup::G5;
    }
}

// NEW-3/DIAG-4：可运行性单一入口 = AdapterRegistry::select()（平台+scheme）
// + DeviceCapability（硬件探测）——UI 可见性与调度共用，杜绝双源。
bool runnableFor(DiagId id, const QString& schemeLower) {
    return AdapterRegistry::select(id, schemeLower) != nullptr
        && DeviceCapability::diagSupportedOnDevice(id);
}
} // namespace

AppState::AppState(QObject* parent) : QObject(parent) {
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS) || defined(Q_OS_MACOS)
    m_isPremiumPlatform = true;   // Premium 仅在移动/Apple 平台提供（spec：Windows/Linux 隐藏）
#endif
    loadPreferences();
    // 8-1：启动不恢复上次结果——诊断页与仪表板启动时保持一致的空态
    // （重构前行为）。persistResults 保留：完成/取消时落盘供后续需要时恢复。
    // loadCachedResults();
    m_config = new ConfigurationController(this, this);
    m_config->loadSettings();
    m_premiumStore = new PremiumStore(this);   // Premium 后端恢复（StoreKit/持久化）
    // 8-15：运行墙钟 1s 节拍——仪表盘"运行时间"实时刷新
    m_elapsedTicker = new QTimer(this);
    m_elapsedTicker->setInterval(1000);
    connect(m_elapsedTicker, &QTimer::timeout, this, [this] {
        if (m_runStatus == Running) emit runElapsedChanged();
    });
}

qint64 AppState::runDurationMs() const {
    if (m_runStatus == Running)
        return m_runElapsedMs + (m_runTimer.isValid() ? m_runTimer.elapsed() : 0);
    return m_runElapsedMs;
}

// ── 结果持久化（重启后保留上次结果；JSON 快照）──
static QString resultsCachePath() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("last_results.json"));
}

void AppState::persistResults() {
    if (m_results.isEmpty()) return;
    QJsonArray arr;
    for (const DiagnosticResult& r : m_results) {
        QJsonObject o;
        o[QStringLiteral("id")] = static_cast<int>(r.id);
        o[QStringLiteral("status")] = static_cast<int>(r.status);
        o[QStringLiteral("summary")] = r.summary;
        o[QStringLiteral("details")] = r.details;
        o[QStringLiteral("rawOutput")] = r.rawOutput;
        o[QStringLiteral("errorOutput")] = r.errorOutput;
        o[QStringLiteral("durationMs")] = r.durationMs;
        o[QStringLiteral("timestamp")] = r.timestamp.toString(Qt::ISODate);
        arr.append(o);
    }
    QDir().mkpath(QFileInfo(resultsCachePath()).absolutePath());
    QFile f(resultsCachePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void AppState::loadCachedResults() {
    QFile f(resultsCachePath());
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray()) return;
    for (const QJsonValue& v : doc.array()) {
        const QJsonObject o = v.toObject();
        DiagnosticResult r;
        r.id = static_cast<DiagId>(o.value(QStringLiteral("id")).toInt());
        r.displayName = diagDisplayName(r.id);
        r.group = diagGroup(r.id);
        // 5WHY (复核 2026-08-18): 盘上 int 无范围校验——旧版本枚举重排产出
        // 越界值时 completed 计数了、7 状态 switch 不落账，X/Y 与徽标分叉。
        // 摄入边界钳制：未知状态按 Error 呈现（可见且可计数）。
        // 5WHY (复核 2026-08-18 缺失键洞): toInt() 对缺失/非数值键返回 0=Pass
        // ——残缺缓存条目会以最误导的"全通过"形态摄入。显式判键存在+数值型。
        const QJsonValue statusVal = o.value(QStringLiteral("status"));
        const int rawStatus = statusVal.isDouble() ? statusVal.toInt() : -1;
        r.status = isValidDiagStatus(rawStatus)
            ? static_cast<DiagStatus>(rawStatus) : DiagStatus::Error;
        r.summary = o.value(QStringLiteral("summary")).toString();
        r.details = o.value(QStringLiteral("details")).toString();
        r.rawOutput = o.value(QStringLiteral("rawOutput")).toString();
        r.errorOutput = o.value(QStringLiteral("errorOutput")).toString();
        r.durationMs = o.value(QStringLiteral("durationMs")).toDouble();
        r.timestamp = QDateTime::fromString(o.value(QStringLiteral("timestamp")).toString(), Qt::ISODate);
        m_results.insert(r.id, r);
    }
}

QStringList AppState::supportedSchemes() const {
    // diag-g5 §2.21 映射表的全部合法 target scheme（下拉框与映射表单一来源）。
    return {QStringLiteral("https"), QStringLiteral("http"), QStringLiteral("ftp"),
            QStringLiteral("ftps"), QStringLiteral("ssh"), QStringLiteral("sftp"),
            QStringLiteral("smtp"), QStringLiteral("smtps"), QStringLiteral("imap"),
            QStringLiteral("imaps"), QStringLiteral("pop3"), QStringLiteral("pop3s"),
            QStringLiteral("telnet"), QStringLiteral("mysql"), QStringLiteral("postgresql"),
            QStringLiteral("redis"), QStringLiteral("mongodb"), QStringLiteral("ldap"),
            QStringLiteral("mqtt")};
}

void AppState::setTarget(const QString& host, const QString& scheme) {
    // 输入解析：host 中可能带 path / 完整 URL；scheme 来自下拉框。
    QString h = host.trimmed();
    QString effScheme = scheme;
    // 允许粘贴完整 URL：其中的 scheme 覆盖下拉框选择
    const int proto = h.indexOf(QLatin1String("://"));
    if (proto > 0) {
        effScheme = h.left(proto).toLower();
        h = h.mid(proto + 3);
    }
    QString path;
    const int slash = h.indexOf(QLatin1Char('/'));
    if (slash > 0) { path = h.mid(slash); h = h.left(slash); }
    // 5WHY (复核 2026-08-19 归一化): 下拉路径不保证小写（仅粘贴 URL 分支
    // toLower）——本处统一小写存储/比较：大小写差异不再触发 schemeChanged
    // 逐键风暴，AdapterRegistry 匹配也拿到归一化输入。
    const QString finalScheme = (effScheme.isEmpty() ? QStringLiteral("https") : effScheme).toLower();
    // 5WHY (复核 2026-08-19 入闸同源): 入闸条件曾比裸 effScheme——大写输入
    // 会令存储值 "https" ≠ "HTTPS" 恒真，每键重入分支（bumpState+targetChanged
    // 逐键风暴，filteredDataChanged 虽正确门控但其余消费仍空转）。入闸与
    // schemeChanged 同用归一化值。
    if (m_targetHost != h || m_targetPath != path || m_targetScheme != finalScheme) {
        // 5WHY (复核 2026-08-18): 过滤视图只随 scheme 变——host/path 逐键编辑
        // （DiagnosticToolbar.onTextChanged 每键一次）不再驱动 5 面板全量重载；
        // 语义信号只在过滤集实际变化时单次发射。
        const bool schemeChanged = (m_targetScheme != finalScheme);
        m_targetHost = h;
        m_targetPath = path;
        m_targetScheme = finalScheme;
        m_targetError = QString();   // 8-4：无目标不再报错（空目标仅跑 G1-G3）
        bumpState();   // 列表/统计按 scheme 过滤，目标变更驱动 UI 重估
        emit targetChanged();
        // 5WHY (复核 2026-08-19 队列化统一): scheme 变更是唯一在可见屏上
        // 触发的 filteredDataChanged 发射点（下拉框/粘贴 URL 的输入栈）——
        // 同步发射在输入栈上驱动 5 面板 _reload（瓦片墙重建）。与其余
        // 突变点统一经队列助手延迟一帧。
        if (schemeChanged) queueFilteredChanged();
    }
}

void AppState::setTargetCredentials(const QString& user, const QString& password, const QString& port) {
    const QString u = user.trimmed();
    const QString p = password.trimmed();
    // 端口仅接受 1-65535；非法输入忽略（不清空已有值）
    QString pt;
    if (!port.trimmed().isEmpty()) {
        bool ok = false;
        const int n = port.trimmed().toInt(&ok);
        if (ok && n >= 1 && n <= 65535) pt = QString::number(n);
    }
    if (m_targetUser != u || m_targetPassword != p || m_targetPort != pt) {
        m_targetUser = u;
        m_targetPassword = p;
        m_targetPort = pt;
        savePreferences();
        bumpState();
        emit targetChanged();
    }
}

void AppState::runDiagnostics() {
    if (m_runStatus == Running) return;
    // 8-4：无目标时仅运行 G1-G3（系统/适配器、连接与安全、互联网与 DNS），
    // G4/G5 依赖目标主机。
    const bool noTarget = m_targetHost.isEmpty();
    // 8-18（检测组显示时机 5WHY）：清屏语义 = 清结果 + 清组状态 + 清当前组。
    // m_currentGroup 必须在 runStatusChanged 之前归 -1——QML 的 _refreshGroups()
    // 在同一轮信号派发里同步执行，残留旧值会让 visibleGroups() 把上一轮全部
    // 组在新 run 起点闪现出来（违反“先清屏，再逐组出现”）。
    m_results.clear();
    m_groupDone.clear();
    m_errorMessage.clear();
    m_currentDiagLabel.clear();   // 上一轮残留的“当前测试”标签清零
    m_currentGroup = -1;
    if (m_cellularWarnVisible) {
        m_cellularWarnVisible = false;
        emit cellularWarnVisibleChanged();
    }
    // P1：可用检测组 = target 决定上限（无目标 G1-G3；有目标 G1-G5），
    // Config 页 groupActive 在此基础上再过滤。
    m_pendingGroups.clear();
    const int maxG = noTarget ? 2 : 4;
    for (int i = 0; i <= maxG; ++i)
        if (isGroupActive(i))
            m_pendingGroups.append(i);
    if (m_pendingGroups.isEmpty()) {
        m_runStatus = Error;
        m_errorMessage = QStringLiteral("All groups are disabled");
        // 5WHY (复核 2026-08-19 反模式 #4 同源): 曾同步发射——本函数由 QML
        // Run 按钮 onClicked 调用，runStatusChanged 同步驱动
        // _refreshGroups 替换数组 → Repeater 在点击栈未退栈时销毁 5 面板
        // 委托（与蜂窝分支同类别）。统一经队列化广播延迟一帧。
        queueStateBroadcast();
        return;
    }
    // 8-18（5WHY 死机根因 1/3）：蜂窝数据警告前移到整轮开始前。原实现在 G3 前
    // （runNextGroup 内）中途弹窗并 return 等待，配合遮罩不可见缺陷造成
    // “界面卡死、无法切页、组不推进”。前移后：清屏 → 弹窗 → 确认/取消。
    if (m_isPremiumPlatform && !noTarget && !m_cellularWarnAcked) {
        m_cellularWarnVisible = true;
        // 5WHY (复核 2026-08-19 状态语义): 此路径已清空
        // m_results/m_groupDone/m_currentGroup 却只发
        // cellularWarnVisibleChanged——所有统计消费方（状态头/组面板/摘要卡）
        // 不监听它，上一轮计数与瓦片墙在"空结果集"上滞留；hasData 的 NOTIFY
        // 是 progressChanged 也不触发。清屏即过滤数据变更：补发信号。
        // 5WHY (复核 2026-08-19 终态残留): 若上一轮 Completed(2) 后再次 Run，
        // runStatus 停留在 2——清屏后状态头仍显示"已完成 0/0"、空态隐藏。
        // 弹窗等待期间尚未运行，归 Idle；runStatusChanged 随广播队列化。
        // 5WHY (复核 2026-08-19 栈上销毁): 本函数由 QML onClicked 调用——同步
        // 发射会驱动 _refreshGroups 替换数组 → Repeater 在按钮信号栈未退栈时
        // 销毁重建面板委托（CLAUDE.md 反模式 #4）。队列化延迟一帧：语义不变
        // （消费方读到的都是已清屏状态），栈上无销毁。
        // 5WHY (复核 2026-08-19 瞬态窗口): Idle 赋值先于 cellularWarnVisibleChanged
        // 发射——同步监听方（如未来弹窗处理器读 runStatus）不再看到
        // 「已清屏 + 旧终态」的混合视图。
        m_runStatus = Idle;
        m_runElapsedMs = 0;   // 墙钟随清屏复位（否则弹窗期间显示上一轮时长）
        // 5WHY (复核 2026-08-20 陈旧计时器): m_runTimer 从未失效——上一轮
        // 的 QElapsedTimer 在弹窗期间仍有效，cancel() 的
        // `m_runTimer.isValid() ? elapsed() : m_runElapsedMs` 会把分钟级旧
        // 时长写入（未开始即取消却显示数分钟"总时间"）。清屏即失效计时器。
        m_runTimer.invalidate();
        emit cellularWarnVisibleChanged();
        queueStateBroadcast();
        return;   // 等待 continueAfterCellularWarn() → 重新 runDiagnostics()
    }
    m_runStatus = Running;
    m_runTimer.start();      // 8-15：墙钟计时开始
    m_runElapsedMs = 0;
    if (m_elapsedTicker) m_elapsedTicker->start();
    // 5WHY (复核 2026-08-19 主路径队列化): 曾同步发射——本函数由 QML Run
    // 按钮 onClicked 调用，同步 runStatusChanged 驱动 _refreshGroups 在
    // 点击栈未退栈时销毁 5 面板委托（反模式 #4；早退两分支已队列化而主
    // 路径漏网）。队列化延迟一帧：消费方送达时读到的都是已启动状态；
    // runNextGroup 保持同步（组 0 创建而非销毁，栈上创建安全）。
    queueStateBroadcast();
    runNextGroup();
}

void AppState::runNextGroup() {
    if (m_pendingGroups.isEmpty()) {
        m_runStatus = Completed;
        m_currentGroup = -1;
        m_runElapsedMs = m_runTimer.isValid() ? m_runTimer.elapsed() : m_runElapsedMs;
        if (m_elapsedTicker) m_elapsedTicker->stop();
        m_cellularWarnAcked = false;   // 8-18：下一轮 run 重新询问
        persistResults();   // 运行完成：落盘结果快照（重启恢复）
        // 5WHY (复核 2026-08-20 同步发射残余): 曾同步发射三信号——全组自动
        // 跳过（无适配器/能力不符）时本分支经 onSuiteFinished 在 QML Run
        // 按钮点击栈上可达：同步驱动 _refreshGroups/面板重载在栈未退栈时
        // 销毁委托（反模式 #4）。与其余路径统一队列化（expected 守卫：
        // 此时状态已是 Completed，过期广播不干扰）。
        queueStateBroadcast();
        return;
    }
    const int gi = m_pendingGroups.takeFirst();
    const DiagGroup g = groupForIndex(gi);
    m_currentGroup = gi;
    // 8-18（5WHY 死机根因 2/3）：先登记组再广播。旧顺序在 emit 之后才
    // m_groupDone.insert——currentRunningGroupChanged 派发时该组既不在 pending
    // 也不在 groupDone，visibleGroups() 的 scheduled 判空将其排除，运行组要等
    // 下一组开始才补显（"正在运行的检测组没有显示"、界面看似卡死）。
    m_groupDone.insert(gi, false);
    // 5WHY (复核 2026-08-19 反模式 #4 覆盖): 曾同步发射——runDiagnostics 由
    // QML Run 按钮 onClicked 调用，重跑时（上一轮 Completed，_groups 含 5 组）
    // visibleGroups() 收窄为 [0]，DiagnosticScreen._refreshGroups 在点击栈
    // 未退栈时销毁 4 个组面板委托（含瓦片）——"组 0 创建而非销毁"前提只对
    // 首轮成立。队列化延迟一帧：消费方送达时读到的是已登记状态；
    // 工作线程路径（onSuiteFinished → runNextGroup）经 invokeMethod 同样
    // 归主线程派发，语义不变。
    // 5WHY (复核 2026-08-20 去重): runDiagnostics 主路径的广播已含
    // currentRunningGroupChanged——待发时跳过自有发射（曾同帧双发、
    // 面板双载）；组推进路径（onSuiteFinished）无广播待发，正常发射。
    if (!m_broadcastPending) {
        // 5WHY (复核 2026-08-20 陈旧守卫): 送达时组指针已变（取消/新轮
        // 抢在 lambda 前推进）即陈旧——捕获排队时的组号比对，避免对新
        // 当前组触发无谓全量重载。
        const int expectedGroup = gi;
        QMetaObject::invokeMethod(this, [this, expectedGroup] {
            if (m_currentGroup != expectedGroup) return;
            emit currentRunningGroupChanged();
        }, Qt::QueuedConnection);
    }

    // H3：每次建 suite 递增 generation，迟到信号按 generation 丢弃
    const qint64 gen = ++m_runGeneration;
    m_suite = new DiagnosticSuite(g, this);
    QVector<DiagId> ids;
    for (DiagId id : allDiagIds())
        if (diagGroup(id) == g && isSchedulable(id)
            && (m_config == nullptr || m_config->isDiagEnabled(static_cast<int>(id)))
            && runnableFor(id, m_targetScheme.toLower()))   // NEW-3/DIAG-4
            ids.append(id);
    m_suite->setDiagIds(ids);
    connect(m_suite, &DiagnosticSuite::resultReady, this, [this, gen](const DiagnosticResult& r) {
        if (gen != m_runGeneration) return;   // H3：跨 run 迟到结果丢弃
        // 8-18（详情页无名 5WHY）：摄入边界归一化——部分平台适配器（iOS .mm
        // 族：DHCP/网关/路由/DNS/HTTP）不填 displayName/group，详情页 header 与
        // HeroCard 因此空白。此处统一补齐，单一来源覆盖全部生产者。
        DiagnosticResult nr = r;
        if (nr.displayName.isEmpty()) nr.displayName = diagDisplayName(nr.id);
        nr.group = diagGroup(nr.id);
        m_results.insert(nr.id, nr);
        m_currentDiagLabel = nr.displayName;
        updateItemModel(nr.id, nr);
        emit progressChanged();
    });
    connect(m_suite, &DiagnosticSuite::suiteFinished, this, [this, gen]() {
        auto* s = qobject_cast<DiagnosticSuite*>(sender());
        if (!s) return;
        if (gen != m_runGeneration || s != m_suite) {
            s->deleteLater();   // H3：旧 suite 迟到 finished → 清理旧实例
            return;
        }
        onSuiteFinished();
    });
    m_groupDone.insert(gi, false);
    // C4：scheme 拼回 target——协议探针（ftp/ssh/mysql/redis/mqtt…）依赖 scheme
    // 判定协议并选端口，丢 scheme 会让全部 G5 协议族被判 "Not X" 跳过。
    // 目标凭据注入：user:pass@ 前綴 + 显式端口（host 已带端口时不重复追加）。
    // 5WHY (review 2026-08-17): 显式端口对 IPv6 字面量被静默丢弃——旧守卫
    // contains(':') 把 IPv6 冒号误判为已有端口，2001:db8::1 跳过端口注入后
    // 所有 G4/G5 探针落在 scheme 默认端口，诊断结果误导。
    QString host = m_targetHost;
    if (!m_targetPort.isEmpty()) {
        // 已带端口：括号+端口（]：）或恰好单冒号（host:port）；IPv6 有 ≥2 冒号
        const bool alreadyPort = host.contains(QLatin1String("]:"))
            || host.count(QLatin1Char(':')) == 1;
        if (!alreadyPort) {
            if (host.contains(QLatin1Char(':')) && !host.startsWith(QLatin1Char('[')))
                host = QLatin1Char('[') + host + QLatin1Char(']');
            host += QLatin1Char(':') + m_targetPort;
        }
    }
    QString auth;
    if (!m_targetUser.isEmpty())
        auth = m_targetUser + QLatin1Char(':') + m_targetPassword + QLatin1Char('@');
    const QString target = m_targetScheme + QLatin1String("://") + auth + host + m_targetPath;
    m_suite->run(target, m_targetScheme.toLower());
}

void AppState::onSuiteFinished() {
    if (!m_suite) return;
    // 5WHY（取消竞态）：cancel() 已把状态置为 Cancelled，但 in-flight suite 的
    // suiteFinished 仍会到达。若继续推进 runNextGroup()（pending 已清空），
    // 状态会被覆写回 Completed，且 m_groupDone 写入 -1 脏键。
    if (m_runStatus != Running) {
        m_suite->deleteLater();
        m_suite = nullptr;
        return;
    }
    const int gi = m_currentGroup;
    m_groupDone.insert(gi, true);
    m_suite->deleteLater();
    m_suite = nullptr;
    emit progressChanged();
    runNextGroup();
}

void AppState::cancel() {
    // 8-18：run 开始前的蜂窝警告取消——run 尚未开始（Idle），仅收起对话框。
    if (m_runStatus != Running) {
        if (m_cellularWarnVisible) {
            m_cellularWarnVisible = false;
            m_cellularWarnAcked = false;   // 下次 Run 重新询问
            emit cellularWarnVisibleChanged();
        }
        return;
    }
    if (m_suite) m_suite->cancel();
    m_pendingGroups.clear();
    persistResults();   // 取消也保存已完成部分
    m_cellularWarnAcked = false;   // 8-18：下一轮 run 重新询问
    if (m_cellularWarnVisible) {
        m_cellularWarnVisible = false;
        emit cellularWarnVisibleChanged();
    }
    m_runStatus = Cancelled;
    m_currentGroup = -1;
    m_runElapsedMs = m_runTimer.isValid() ? m_runTimer.elapsed() : m_runElapsedMs;
    if (m_elapsedTicker) m_elapsedTicker->stop();
    // 5WHY (复核 2026-08-19 主路径队列化): cancel 由 QML 取消按钮 onClicked
    // 调用——同步发射在点击栈未退栈时驱动可见组集合翻转（部分揭示→全量）
    // 与 5 面板重建（反模式 #4，与 runDiagnostics 同源）。统一队列化
    // （含 currentRunningGroupChanged，见 queueStateBroadcast 注释）。
    queueStateBroadcast();
}

void AppState::continueAfterCellularWarn() {
    if (!m_cellularWarnVisible) return;
    m_cellularWarnVisible = false;
    m_cellularWarnAcked = true;
    emit cellularWarnVisibleChanged();
    // 8-18：警告前移到整轮开始前——Idle 状态确认即从 runDiagnostics 整轮启动；
    // Running 分支为兼容保留（新流程不会进入）。
    if (m_runStatus == Running) runNextGroup();
    else runDiagnostics();
}

void AppState::dismissCellularWarn() {
    // 纯关闭（不确认不启动）：5WHY review round 3——遮罩点击走此路径，
    // 误触不会触发整轮大流量诊断。
    if (!m_cellularWarnVisible) return;
    m_cellularWarnVisible = false;
    emit cellularWarnVisibleChanged();
}

QVariantMap AppState::itemFor(DiagId id) const {
    QVariantMap m;
    m[QStringLiteral("diagId")] = static_cast<int>(id);
    m[QStringLiteral("label")] = diagDisplayName(id);
    m[QStringLiteral("iconName")] = diagnosticMeta(id).iconName;
    m[QStringLiteral("group")] = static_cast<int>(diagGroup(id));
    const auto it = m_results.constFind(id);
    if (it == m_results.constEnd()) {
        m[QStringLiteral("status")] = -1;
        m[QStringLiteral("isPending")] = true;
        m[QStringLiteral("isDone")] = false;
        m[QStringLiteral("isDisabled")] = false;
        m[QStringLiteral("durationMs")] = 0;
        // 5WHY (复核 2026-08-18 计时连续性): 运行中探针注入墙钟起点——并行
        // Suite 下兄弟结果落地触发网格重建，委托本地计时归零；UI 以
        // startedAtMs 反推真实已运行时长，重建不再重置显示。
        m[QStringLiteral("startedAtMs")] = 0;
        if (m_suite && m_suite->isRunning()) {
            const auto starts = m_suite->runningStartTimes();
            if (const auto sit = starts.constFind(id); sit != starts.constEnd())
                m[QStringLiteral("startedAtMs")] = sit.value();
        }
        m[QStringLiteral("summary")] = QString();
    } else {
        m[QStringLiteral("status")] = static_cast<int>(it->status);
        m[QStringLiteral("isPending")] = false;
        m[QStringLiteral("isDone")] = true;
        m[QStringLiteral("isDisabled")] = false;
        m[QStringLiteral("durationMs")] = it->durationMs;
        m[QStringLiteral("startedAtMs")] = 0;   // 已完成项无运行计时
        m[QStringLiteral("summary")] = it->summary;
    }
    return m;
}

QVariantList AppState::allDiagsForGroup(int groupInt) const {
    QVariantList out;
    const DiagGroup g = groupForIndex(groupInt);
    // 5WHY (复核 2026-08-19 效率): toLower 曾在循环体逐项重算——45 次 QString
    // 堆分配/调用（groupStats 同病）；归一化一次循环外复用。
    const QString schemeLower = m_targetScheme.toLower();
    for (DiagId id : allDiagIds())
        if (diagGroup(id) == g && isSchedulable(id)
            && runnableFor(id, schemeLower))
            out.append(itemFor(id));
    return out;
}

QVariantMap AppState::groupStats(int groupInt) const {
    QVariantMap s;
    int total = 0, completed = 0, pass = 0, warn = 0, fail = 0,
        skip = 0, info = 0, error = 0, cancelled = 0;
    const bool aggregate = (groupInt < 0);
    const DiagGroup g = groupForIndex(qMax(0, groupInt));
    // 5WHY (复核 2026-08-19 效率): toLower 曾两个循环体各逐项重算（90 次
    // QString 堆分配/调用）；归一化一次复用。
    const QString schemeLower = m_targetScheme.toLower();
    for (DiagId id : allDiagIds()) {
        if (!isSchedulable(id)) continue;
        if (!aggregate && diagGroup(id) != g) continue;
        // NEW-3：统计与调度同源——不可运行（平台/scheme/设备）不计入总数
        if (!runnableFor(id, schemeLower)) continue;
        ++total;
        const auto it = m_results.constFind(id);
        if (it == m_results.constEnd()) continue;
        switch (it->status) {
            case DiagStatus::Pass:      ++pass; break;
            case DiagStatus::Warning:   ++warn; break;
            case DiagStatus::Fail:      ++fail; break;
            case DiagStatus::Skipped:   ++skip; break;
            case DiagStatus::Info:      ++info; break;
            case DiagStatus::Error:     ++error; break;
            case DiagStatus::Cancelled: ++cancelled; break;
            // 5WHY (复核 2026-08-18 不变式兜底): 越界状态落 Error 账，
            // 杜绝"completed 有数、徽标无处落账"的分叉。
            default:                    ++error; break;
        }
    }
    // 5WHY (复核 2026-08-18 用户诉求 "5/5 但徽标仅 4"): completed 曾独立于
    // 状态计数逐条累加——任一结果状态越界即与 Σ徽标分叉。改为单一推导点：
    // completed ≡ Σ7 状态，X/Y 与徽标求和结构上恒等。
    completed = pass + warn + fail + skip + info + error + cancelled;
    s[QStringLiteral("total")] = total;
    s[QStringLiteral("completed")] = completed;
    // 5WHY (复核 2026-08-18 completed 双语义): completed 含 Cancelled/Skipped
    // （"有结果"的进度语义）；UI 的"成功完成"叙事需排除取消——单一推导点
    // 暴露派生字段，取代 DashboardRowHeader 的手工减法（此前仅在进度条一处
    // 排除，头部 X/Y 与簇标签仍算入，同屏数字互相矛盾）。
    s[QStringLiteral("completedExclCancelled")] = completed - cancelled;
    s[QStringLiteral("pass")] = pass;
    s[QStringLiteral("warn")] = warn;
    s[QStringLiteral("fail")] = fail;
    s[QStringLiteral("skip")] = skip;
    s[QStringLiteral("info")] = info;
    s[QStringLiteral("error")] = error;
    s[QStringLiteral("cancelled")] = cancelled;
    qint64 totalMs = 0;
    for (const DiagnosticResult& r : m_results) {
        // 5WHY (review 2026-08-17): 计数按组过滤但时长循环遍历全部 m_results——
        // 每个组的 durationMs 都等于整轮总时长，Layer Timings 五行显示同一数字。
        // 与计数循环同源过滤（DiagnosticResult.group 字段直接可用）。
        if (!aggregate && r.group != g) continue;
        // 5WHY (复核 2026-08-18 时长与计数同源): 计数循环按 isSchedulable +
        // runnableFor 过滤，时长循环此前只按组——换 scheme 不重跑时旧结果时长
        // 混入新过滤集（"3/20 徽标配 20 项时长"）。补齐同一过滤。
        if (!isSchedulable(r.id)) continue;
        if (!runnableFor(r.id, schemeLower)) continue;
        totalMs += r.durationMs;
    }
    s[QStringLiteral("durationMs")] = static_cast<qlonglong>(totalMs);
    return s;
}

QVariantList AppState::visibleGroups() const {
    // H8：archive 语义 = isGroupActive(i) && enabled>0——组内全停用时隐藏面板。
    // 8-15（渐进呈现）：运行中只显示已开始的组（已完成或当前组）；未调度的
    // 组（如无目标时的 G4/G5）不显示；完成/取消后显示全部已调度组。
    QVariantList out;
    if (m_runStatus == Idle) return out;
    for (int i = 0; i < 5; ++i) {
        if (!isGroupActive(i) || !isGroupAnyEnabled(i)) continue;
        const bool scheduled = m_pendingGroups.contains(i) || m_groupDone.contains(i);
        if (!scheduled) continue;
        const bool revealed = m_runStatus != Running
            ? true
            : (m_groupDone.contains(i) || i <= m_currentGroup);
        if (revealed) out.append(i);
    }
    return out;
}

QVariantMap AppState::resultFor(int diagIdInt) const {
    const auto it = m_results.constFind(static_cast<DiagId>(diagIdInt));
    if (it == m_results.constEnd()) return {};
    QVariantMap m;
    m[QStringLiteral("diagId")] = static_cast<int>(it->id);
    m[QStringLiteral("displayName")] = it->displayName;
    // 5WHY (review 2026-08-17): iconName 仅 itemFor() 提供，resultFor() 缺失——
    // DetailPage/PageHeroSection/PageDetailSheet 的 45 图标全彩头部永远走到回退
    // 分支，新头部从未激活。与 itemFor() 对齐（诊断元数据同一来源）。
    m[QStringLiteral("iconName")] = diagnosticMeta(it->id).iconName;
    m[QStringLiteral("status")] = static_cast<int>(it->status);
    m[QStringLiteral("summary")] = it->summary;
    m[QStringLiteral("details")] = it->details;
    m[QStringLiteral("rawOutput")] = it->rawOutput;
    m[QStringLiteral("narrative")] = it->narrative;
    m[QStringLiteral("durationMs")] = it->durationMs;
    m[QStringLiteral("errorOutput")] = it->errorOutput;
    QVariantList props;
    for (const auto& p : it->properties) {
        QVariantMap pm;
        pm[QStringLiteral("label")] = p.label;
        pm[QStringLiteral("value")] = p.value;
        pm[QStringLiteral("severity")] = static_cast<int>(p.severity);
        QVariantList children;
        for (const auto& c : p.children) {
            QVariantMap cm;
            cm[QStringLiteral("label")] = c.label;
            cm[QStringLiteral("value")] = c.value;
            children.append(cm);
        }
        pm[QStringLiteral("children")] = children;
        props.append(pm);
    }
    m[QStringLiteral("properties")] = props;
    // 5WHY（图表不显示根因）：viz/ResultChart 依赖 data.templateType 选图，
    // 但重建的契约层不再向结果注入该字段——在此由 meta 注入（不改探针结果）。
    QVariantMap data = it->data;
    data[QStringLiteral("templateType")] = static_cast<int>(diagnosticMeta(it->id).tmplType);
    // 5WHY (复核 2026-08-19 用户诉求 "详情页不单独列出 Duration 区块"):
    // keyMetricField 注入与 KeyMetric.js 时长兜底成对出现——失败结果以
    // 独立 Duration 大卡重复 hero 时长行。兜底已移除（详情页失败结果改
    // 由错误区块 + 属性卡承载数据），注入链随之一并删除（零剩余消费方）。
    m[QStringLiteral("data")] = data;
    // 8-16：契约开关注入——详情区块（错误/属性/终端）按 meta DetailProfile
    // 自门控，避免模板不支持的区块出现空卡/错误内容。
    const DetailProfile& dp = diagnosticMeta(it->id).detail;
    m[QStringLiteral("showErrorOutput")] = dp.showErrorOutput;
    m[QStringLiteral("showProperties")] = dp.showProperties;
    m[QStringLiteral("showCharts")] = dp.showCharts;
    // 5WHY (2026-08-19 用户诉求 "不单独列出 During 区块"): G1 探针的终端
    // 转储由属性自动生成（propsDump 标记）——与属性卡逐字重复。终端区块
    // 对属性派生转储让位（数据已以结构化属性卡呈现）；真实原始输出
    // （G4/G5 逐跳/证书链等）不受影响。
    m[QStringLiteral("showTerminal")] = dp.showTerminal
        && !data.value(QStringLiteral("propsDump")).toBool();
    // 属性布局（Kv 扁平 / Grouped 分组卡）——PagePropertiesSection 渲染模式
    m[QStringLiteral("propLayout")] = static_cast<int>(dp.propLayout);
    return m;
}

QVariantMap AppState::contractFor(int diagIdInt) const {
    const DiagId id = static_cast<DiagId>(diagIdInt);
    const OutputContract c = ::contractFor(id);
    QVariantMap m;
    m[QStringLiteral("metricField")] = c.metric.field ? QString::fromLatin1(c.metric.field) : QString();
    m[QStringLiteral("metricUnit")] = c.metric.unit ? QString::fromLatin1(c.metric.unit) : QString();
    m[QStringLiteral("metricPrecision")] = c.metric.precision;
    m[QStringLiteral("chartType")] = static_cast<int>(c.chart.type);
    m[QStringLiteral("chartField")] = c.chart.field ? QString::fromLatin1(c.chart.field) : QString();
    m[QStringLiteral("showError")] = c.showError;
    m[QStringLiteral("showProps")] = c.showProps;
    m[QStringLiteral("showTerminal")] = c.showTerminal;
    return m;
}

void AppState::updateItemModel(DiagId id, const DiagnosticResult& r) {
    Q_UNUSED(id);
    Q_UNUSED(r);
    // items model 为惰性视图（allDiagsForGroup 每次由 m_results 推导），
    // 此钩子保留给未来增量通知（progressChanged 已驱动 UI 刷新）。
}

// ═══════════════════════════════════════════════════════════════════════
// P1：Config / 语言 / 主题 桥接
// ═══════════════════════════════════════════════════════════════════════

QStringList AppState::langItems() const {
    // 与 translations.json 语言顺序一致（0=ZH_CN … 14=AR）
    return {
        QStringLiteral("简体中文"), QStringLiteral("繁體中文"), QStringLiteral("日本語"),
        QStringLiteral("한국어"), QStringLiteral("हिन्दी"), QStringLiteral("Tiếng Việt"),
        QStringLiteral("Türkçe"), QStringLiteral("English"), QStringLiteral("Français"),
        QStringLiteral("Deutsch"), QStringLiteral("Русский"), QStringLiteral("Italiano"),
        QStringLiteral("Español"), QStringLiteral("Português"), QStringLiteral("العربية")
    };
}

QVariantList AppState::allDiagIdsForGroup(int groupInt) const {
    QVariantList out;
    const DiagGroup g = groupForIndex(groupInt);
    for (DiagId id : allDiagIds())
        if (diagGroup(id) == g && isSchedulable(id)
            && runnableFor(id, m_targetScheme.toLower()))   // NEW-3：Config 可见性同源
            out.append(static_cast<int>(id));
    return out;
}

int AppState::diagCountForGroup(int groupInt) const {
    return allDiagIdsForGroup(groupInt).size();
}

QVariantList AppState::resultsForGroup(int groupInt) const {
    QVariantList out;
    const DiagGroup g = groupForIndex(groupInt);
    for (DiagId id : allDiagIds()) {
        if (diagGroup(id) != g || !isSchedulable(id)) continue;
        // 5WHY (复核 2026-08-18 瓦片墙/统计同源): 换 scheme 不重跑时统计已按
        // runnableFor 过滤成新集（0/0），Dashboard 完成态瓦片墙却仍显示旧
        // scheme 结果——头部/组头与瓦片可见分叉。补齐与 groupStats/
        // allDiagsForGroup 相同的 scheme 过滤。
        if (!runnableFor(id, m_targetScheme.toLower())) continue;
        const auto it = m_results.constFind(id);
        if (it != m_results.constEnd())
            out.append(itemFor(id));
    }
    return out;
}

void AppState::setLanguage(int index) {
    if (index < 0 || index >= langItems().size()) return;
    if (m_languageIndex == index) return;
    m_languageIndex = index;
    savePreferences();
    bumpState();
    emit languageChanged();
}

void AppState::setThemeMode(int mode) {
    if (m_themeMode == mode) return;
    m_themeMode = mode;
    savePreferences();
    emit themeModeChanged();
}

void AppState::setGroupActive(int groupInt, bool active) {
    if (groupInt < 0 || groupInt > 4) return;
    if (m_activeGroups.contains(groupInt) == active) return;
    if (active) m_activeGroups.insert(groupInt);
    else m_activeGroups.remove(groupInt);
    savePreferences();
    bumpState();
    // visibleGroups() 按激活组过滤——组面板列表消费方需语义信号刷新
    // （与 setDiagEnabled/setGroupEnabled 同一队列化策略，5WHY 见 runDiagnostics）
    queueFilteredChanged();
}

bool AppState::isGroupActive(int groupInt) const {
    return groupInt >= 0 && groupInt <= 4 && m_activeGroups.contains(groupInt);
}

bool AppState::isDiagEnabled(int diagIdInt) const {
    return m_config && m_config->isDiagEnabled(diagIdInt);
}
bool AppState::setDiagEnabled(int diagIdInt, bool enabled) {
    const bool ok = m_config && m_config->setDiagEnabled(diagIdInt, enabled);
    // 5WHY (复核 2026-08-19 语义信号缺口): visibleGroups() 经 isGroupAnyEnabled
    // 过滤（按 diag 级启用态）——切换"组内最后一个启用的检测项"会改变面板
    // 集合，而消费方已不监听 stateVersionChanged。与 setGroupActive 同级补发。
    // 5WHY (复核 2026-08-19 反模式 #4 一致性): 本函数由 QML 点击处理器调用，
    // 当前消费方（面板 Repeater 模型）均被页面可见性门控故同步发射安全——
    // 但风险类别与 runDiagnostics 蜂窝路径（队列化）相同。同样队列化：
    // 未来新增可见屏消费方（如诊断页内嵌配置浮层）不会把委托销毁推进
    // QML 点击栈（CLAUDE.md 反模式 #4）。
    if (ok) {
        bumpState();
        queueFilteredChanged();   // 队列化广播（5WHY 反模式 #4，见 runDiagnostics）
    }
    return ok;
}
bool AppState::setGroupEnabled(int groupInt, bool enabled) {
    const bool ok = m_config && m_config->setGroupEnabled(groupInt, enabled);
    if (ok) {
        bumpState();
        queueFilteredChanged();
    }
    return ok;
}
// 5WHY (复核 2026-08-19 单一广播点): 三处 QMetaObject::invokeMethod 队列化
// 发射逐字复制——收敛为私有助手。QML 点击栈上同步发射会驱动 Repeater 委托
// 销毁（CLAUDE.md 反模式 #4）；队列化延迟一帧，消费方读到的都是新状态。
void AppState::queueFilteredChanged() {
    QMetaObject::invokeMethod(this, [this] {
        emit filteredDataChanged();
    }, Qt::QueuedConnection);
}

void AppState::queueStateBroadcast() {
    // 5WHY (复核 2026-08-19 过期广播守卫): 广播排队后用户可能已 Continue——
    // continueAfterCellularWarn 同步重启 runDiagnostics（Running 路径另行
    // 广播）后，此处过期 lambda 仍会按当前状态再发一轮（5 面板三次重扫）。
    // 捕获排队时的 runStatus：送达时状态已变即跳过（Continue 后 Running≠
    // Idle；dismiss 后仍 Idle 正常送达）。
    const int expected = m_runStatus;   // m_runStatus 为 int 成员（Q_PROPERTY）
    m_broadcastPending = true;
    QMetaObject::invokeMethod(this, [this, expected] {
        m_broadcastPending = false;
        if (m_runStatus != expected) return;
        // currentRunningGroupChanged 一并队列：cancel 路径的可见组集合翻转
        // （部分揭示→全量）也经同一延迟帧送达，点击栈上无任何消费方重建。
        emit currentRunningGroupChanged();
        emit runStatusChanged();
        emit progressChanged();
        emit filteredDataChanged();
    }, Qt::QueuedConnection);
}
bool AppState::isGroupAllEnabled(int groupInt) const {
    return m_config && m_config->isGroupAllEnabled(groupInt);
}
bool AppState::isGroupAnyEnabled(int groupInt) const {
    return m_config && m_config->isGroupAnyEnabled(groupInt);
}

void AppState::loadPreferences() {
    QSettings s;
    s.beginGroup(QStringLiteral("AppSettings"));
    // activeGroups：默认全部激活（缺省键 → 全量）
    const QStringList active = s.value(QStringLiteral("activeGroups")).toStringList();
    if (active.isEmpty()) {
        for (int i = 0; i < 5; ++i) m_activeGroups.insert(i);
    } else {
        for (const QString& str : active) {
            bool ok = false;
            const int g = str.toInt(&ok);
            if (ok && g >= 0 && g < 5) m_activeGroups.insert(g);
        }
    }
    m_languageIndex = s.value(QStringLiteral("languageIndex"), 7).toInt();
    m_themeMode = s.value(QStringLiteral("themeMode"), 2).toInt();
    m_targetUser = s.value(QStringLiteral("targetUser")).toString();
    m_targetPassword = s.value(QStringLiteral("targetPassword")).toString();
    m_targetPort = s.value(QStringLiteral("targetPort")).toString();
    s.endGroup();
}

void AppState::savePreferences() {
    QSettings s;
    s.beginGroup(QStringLiteral("AppSettings"));
    QStringList active;
    for (int i = 0; i < 5; ++i)
        if (m_activeGroups.contains(i))
            active.append(QString::number(i));
    s.setValue(QStringLiteral("activeGroups"), active);
    s.setValue(QStringLiteral("languageIndex"), m_languageIndex);
    s.setValue(QStringLiteral("themeMode"), m_themeMode);
    s.setValue(QStringLiteral("targetUser"), m_targetUser);
    s.setValue(QStringLiteral("targetPassword"), m_targetPassword);
    s.setValue(QStringLiteral("targetPort"), m_targetPort);
    s.endGroup();
}

void AppState::bumpState() {
    ++m_stateVersion;
    emit stateVersionChanged();
}

// ═══════════════════════════════════════════════════════════════════════
// 剪贴板 / 报告文本
// ═══════════════════════════════════════════════════════════════════════

static QString statusToken(DiagStatus s) {
    switch (s) {
        case DiagStatus::Pass:      return QStringLiteral("PASS");
        case DiagStatus::Warning:   return QStringLiteral("WARN");
        case DiagStatus::Fail:      return QStringLiteral("FAIL");
        case DiagStatus::Skipped:   return QStringLiteral("SKIP");
        case DiagStatus::Info:      return QStringLiteral("INFO");
        case DiagStatus::Error:     return QStringLiteral("ERROR");
        case DiagStatus::Cancelled: return QStringLiteral("CANCELLED");
    }
    return QString();
}

QString AppState::buildReportText() const {
    QStringList lines;
    lines.append(QStringLiteral("NetDiagnostics report"));
    lines.append(QStringLiteral("Target: %1%2").arg(m_targetHost, m_targetPath));
    lines.append(QString());
    for (int gi = 0; gi < 5; ++gi) {
        const DiagGroup g = groupForIndex(gi);
        QStringList gl;
        for (DiagId id : allDiagIds()) {
            if (diagGroup(id) != g || !isSchedulable(id)) continue;
            const auto it = m_results.constFind(id);
            if (it == m_results.constEnd()) continue;
            gl.append(QStringLiteral("[%1] %2 — %3 (%4ms)")
                          .arg(statusToken(it->status), it->displayName,
                               it->summary, QString::number(it->durationMs)));
        }
        if (gl.isEmpty()) continue;
        lines.append(diagGroupLabel(g));
        for (const QString& l : gl) lines.append(QStringLiteral("  ") + l);
        lines.append(QString());
    }
    return lines.join(QLatin1Char('\n'));
}

void AppState::copyReportToClipboard() {
    QGuiApplication::clipboard()->setText(buildReportText());
}

// ── HTML 报告（ReportEngine 最小恢复：快照式、纯文本输出；PDF/预览 UI 延后）──
// 归档 ReportEngine 的 HTML 导出能力以轻量形态保留：每次调用基于当前 m_results
// 快照生成完整 HTML，不做增量缓存。
QString AppState::buildReportHtml() const {
    static const QHash<DiagStatus, QString> kStatusColor = {
        {DiagStatus::Pass,      QStringLiteral("#22c55e")},
        {DiagStatus::Warning,   QStringLiteral("#f59e0b")},
        {DiagStatus::Fail,      QStringLiteral("#ef4444")},
        {DiagStatus::Skipped,   QStringLiteral("#94a3b8")},
        {DiagStatus::Info,      QStringLiteral("#3b82f6")},
        {DiagStatus::Error,     QStringLiteral("#ef4444")},
        {DiagStatus::Cancelled, QStringLiteral("#94a3b8")},
    };
    auto esc = [](const QString& s) {
        return s.toHtmlEscaped();
    };
    QStringList h;
    h.append(QStringLiteral("<!DOCTYPE html><html><head><meta charset=\"utf-8\">"));
    h.append(QStringLiteral("<title>NetDiagnostics Report</title><style>"));
    h.append(QStringLiteral("body{font-family:system-ui,sans-serif;background:#0f172a;color:#e2e8f0;padding:24px}"));
    h.append(QStringLiteral("h1{font-size:22px}h2{font-size:16px;margin-top:24px;border-bottom:1px solid #334155;padding-bottom:6px}"));
    h.append(QStringLiteral("table{width:100%%;border-collapse:collapse;font-size:13px}"));
    h.append(QStringLiteral("td,th{padding:6px 10px;border-bottom:1px solid #1e293b;text-align:left}"));
    h.append(QStringLiteral("th{color:#94a3b8;font-weight:600}.st{font-weight:700}.meta{color:#94a3b8;font-size:12px}"));
    h.append(QStringLiteral("</style></head><body>"));
    h.append(QStringLiteral("<h1>NetDiagnostics Report</h1>"));
    h.append(QStringLiteral("<div class=\"meta\">Target: %1 &middot; Generated: %2</div>")
        .arg(esc(m_targetHost + m_targetPath),
             esc(QDateTime::currentDateTime().toString(Qt::ISODate))));
    for (int gi = 0; gi < 5; ++gi) {
        const DiagGroup g = groupForIndex(gi);
        QStringList rows;
        for (DiagId id : allDiagIds()) {
            if (diagGroup(id) != g || !isSchedulable(id)) continue;
            const auto it = m_results.constFind(id);
            if (it == m_results.constEnd()) continue;
            rows.append(QStringLiteral("<tr><td class=\"st\" style=\"color:%1\">%2</td>"
                                       "<td>%3</td><td>%4</td><td>%5 ms</td></tr>")
                .arg(kStatusColor.value(it->status, QStringLiteral("#94a3b8")),
                     statusToken(it->status), esc(it->displayName),
                     esc(it->summary), QString::number(it->durationMs)));
        }
        if (rows.isEmpty()) continue;
        h.append(QStringLiteral("<h2>%1</h2>").arg(esc(diagGroupLabel(g))));
        h.append(QStringLiteral("<table><tr><th>Status</th><th>Test</th><th>Result</th><th>Time</th></tr>"));
        h.append(rows.join(QString()));
        h.append(QStringLiteral("</table>"));
    }
    h.append(QStringLiteral("</body></html>"));
    return h.join(QLatin1Char('\n'));
}

void AppState::copyDetailToClipboard(int diagIdInt) {
    const auto it = m_results.constFind(static_cast<DiagId>(diagIdInt));
    if (it == m_results.constEnd()) return;
    QStringList lines;
    lines.append(QStringLiteral("[%1] %2").arg(statusToken(it->status), it->displayName));
    if (!it->summary.isEmpty()) lines.append(it->summary);
    // 摘要卡叙述（结论 + 依据）——摘要与明细之间，剪贴板与详情页一致
    if (!it->narrative.isEmpty()) lines.append(it->narrative);
    // 5WHY (复核 2026-08-20 剪贴板双份): G1 的属性派生转储（propsDump）与
    // 属性循环输出逐字相同——曾双双追加，粘贴的票据每条属性出现两次。
    // 转储存在时以其为准、跳过属性循环；否则照旧逐属性输出。
    const bool isDump = it->data.value(QStringLiteral("propsDump")).toBool();
    if (!it->details.isEmpty()) lines.append(it->details);
    if (!isDump) {
        for (const auto& p : it->properties) {
            lines.append(QStringLiteral("%1: %2").arg(p.label, p.value));
            for (const auto& c : p.children)
                lines.append(QStringLiteral("  %1: %2").arg(c.label, c.value));
        }
    }
    QGuiApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
}

// ═══════════════════════════════════════════════════════════════════════
// 报告 / 分享 / 动画（ReportEngine + Premium 后端恢复）
// ═══════════════════════════════════════════════════════════════════════

QString AppState::diagAnimationUrl(int diagIdInt) const {
    // DiagAnimType → QRC URL 单一来源（无 QML 侧 switch）
    const DiagId id = static_cast<DiagId>(diagIdInt);
    switch (diagnosticMeta(id).animType) {
        case DiagAnimType::Bounce: return QStringLiteral("qrc:/qt/qml/widgets/animations/BounceAnimation.qml");
        case DiagAnimType::Path:   return QStringLiteral("qrc:/qt/qml/widgets/animations/PathAnimation.qml");
        case DiagAnimType::Pulse:  return QStringLiteral("qrc:/qt/qml/widgets/animations/PulseAnimation.qml");
        case DiagAnimType::Type:   return QStringLiteral("qrc:/qt/qml/widgets/animations/TypeAnimation.qml");
        case DiagAnimType::Lock:   return QStringLiteral("qrc:/qt/qml/widgets/animations/LockAnimation.qml");
        case DiagAnimType::Tick:  return QStringLiteral("qrc:/qt/qml/widgets/animations/CheckAnimation.qml");
        case DiagAnimType::WifiWave: return QStringLiteral("qrc:/qt/qml/widgets/animations/WifiWaveAnimation.qml");
        case DiagAnimType::Converge: return QStringLiteral("qrc:/qt/qml/widgets/animations/ConvergeAnimation.qml");
        case DiagAnimType::GeoRadar: return QStringLiteral("qrc:/qt/qml/widgets/animations/GeoLocateAnimation.qml");
        default:                   return QStringLiteral("qrc:/qt/qml/widgets/animations/JiggleAnimation.qml");
    }
}

QVariantMap AppState::diagAnimationAnchor(int diagIdInt) const {
    // 5WHY (复核 2026-08-19 锚点元数据): 动画锚点是母版 SVG 图形的几何事实
    // （GeoIP 定位针头位置/扩散半径），此前硬编码在动画 QML 内——母版再生成
    // 位移时静默错位（WifiWave 曾以 Meter 表针形式因错误假设整体重做）。与 animType→URL 同库
    // 同层收进 C++：DiagAnimator 装载时下发给动画（动画保留同值默认供
    // 直接实例化回退）。
    const DiagId id = static_cast<DiagId>(diagIdInt);
    QVariantMap a;
    if (diagnosticMeta(id).animType == DiagAnimType::GeoRadar) {
        // geoip 母版定位针头中心 + 到最近边缘的半径（QSvgRenderer 逐通道
        // 实测 viewBox ≈(0.71, 0.30)；右缘距 0.29 为约束紧侧）
        a[QStringLiteral("cx")] = 0.71;
        a[QStringLiteral("cy")] = 0.30;
        a[QStringLiteral("maxR")] = 0.29;
    } else if (diagnosticMeta(id).animType == DiagAnimType::WifiWave) {
        // 5WHY (2026-08-20 用户诉求 "右下角 wifi 信号弧逐条明灭"): internet
        // 母版右下侧三道红色信号弧（350 系 y≈216/236/243 弧组）焦点≈(296,244)、
        // 外弧半径≈52/350——按 viewBox 归一化为焦点 (0.85,0.70)、外半径 0.155。
        // WifiWaveAnimation 保留同值默认，母版再生成位移时仅改此一处。
        a[QStringLiteral("cx")] = 0.85;
        a[QStringLiteral("cy")] = 0.70;
        a[QStringLiteral("maxR")] = 0.155;
    }
    return a;
}

// 快照构建（ReportEngine 零耦合当前可变状态）
QString AppState::previewReportHtml() const {
    ReportData d;
    d.target = m_targetHost + m_targetPath;
    d.timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    d.appVersion = QStringLiteral(PROJECT_VERSION);
    d.buildNumber = QStringLiteral(ND_BUILD_NUMBER);
    d.gitHash = QStringLiteral("dev");
    for (int gi = 0; gi < 5; ++gi) {
        const DiagGroup g = groupForIndex(gi);
        d.groupLabels.append(diagGroupLabel(g));
        QList<DiagId> ids;
        QVariantMap st;
        int pass = 0, warn = 0, fail = 0, skip = 0, info = 0, error = 0, cancelled = 0, total = 0;
        for (DiagId id : allDiagIds()) {
            if (diagGroup(id) != g || !isSchedulable(id)) continue;
            ids.append(id);
            d.displayNames[id] = diagDisplayName(id);
            const auto it = m_results.constFind(id);
            if (it == m_results.constEnd()) continue;
            d.results[id] = *it;
        }
        for (const auto& r : m_results) {
            if (diagGroup(r.id) != g) continue;
            ++total;
            switch (r.status) {
                case DiagStatus::Pass: ++pass; break;
                case DiagStatus::Warning: ++warn; break;
                case DiagStatus::Fail: ++fail; break;
                case DiagStatus::Skipped: ++skip; break;
                case DiagStatus::Info: ++info; break;
                case DiagStatus::Error: ++error; break;
                case DiagStatus::Cancelled: ++cancelled; break;
                // 5WHY (复核 2026-08-18 一致性): 与 groupStats 的 default→error
                // 同一不变式——越界状态在报告表面也可见且可计数，不再静默消失。
                default: ++error; break;
            }
        }
        st[QStringLiteral("pass")] = pass; st[QStringLiteral("warn")] = warn;
        st[QStringLiteral("fail")] = fail; st[QStringLiteral("skip")] = skip;
        st[QStringLiteral("info")] = info; st[QStringLiteral("error")] = error;
        st[QStringLiteral("cancelled")] = cancelled; st[QStringLiteral("total")] = total;
        d.groupStats[gi] = st;
        d.diagIdsInGroup[g] = ids;
    }
    return ReportEngine::buildHtml(d, true, true);
}

QString AppState::renderPreviewImage(int widthPx) const {
    const QString html = previewReportHtml();
    const QImage img = ReportEngine::renderHtmlToImage(html, widthPx > 0 ? widthPx : 960);
    if (img.isNull()) return {};
    const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("netdiag_preview.png"));
    if (img.save(path)) return path;
    return {};
}

QString AppState::exportHtmlReport() const {
    const QString html = previewReportHtml();
    const QString path = ReportEngine::defaultReportPath(QStringLiteral("html"));
    return ReportEngine::exportHtml(path, html);
}

QString AppState::exportPdfReport() const {
    const QString html = previewReportHtml();
    const QString path = ReportEngine::defaultReportPath(QStringLiteral("pdf"));
    return ReportEngine::exportPdf(path, html);
}

QString AppState::shareReportFile(const QString& format) {
    if (format == QLatin1String("text")) {
        copyReportToClipboard();
        return {};
    }
    QString path;
    if (format == QLatin1String("pdf")) path = exportPdfReport();
    else path = exportHtmlReport();
    if (path.isEmpty()) return {};
#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID)
    platformShareFile(path,
        format == QLatin1String("pdf") ? QStringLiteral("application/pdf") : QStringLiteral("text/html"),
        QStringLiteral("NetDiagnostics Report"));
#else
    // 桌面：交给系统默认应用（浏览器/PDF 阅读器）+ 邮件 handoff 语义
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
#endif
    return path;
}
