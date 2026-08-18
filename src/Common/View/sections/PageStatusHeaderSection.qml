// PageStatusHeaderSection.qml — 状态/徽标/分享头部（§2.3）
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import widgets
import "../widgets/StatsUtil.js" as W   // 直接 JS 导入（qmldir 模块目录不可相对导入）
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Plain
    bottomMargin: ThemeEngine.spacing.sm

    // 页面注入的额外头部组件（Dashboard：摘要统计层）—— UI-5：依赖方向页面→Common
    property Component headerExtra: null
    // 状态呈现单次求值（5WHY review round 4: icon/color/label 各自调用
    // runStatusInfo——同一次变化最多 4 次对象构造；单一属性共享）
    readonly property var _statusInfo: ThemeEngine.runStatusInfo(AppState.runStatus)

    // UI-2：聚合统计命令式刷新（progressChanged/runStatusChanged 处理器赋值），
    // 绑定中不调用 groupStats(-1)。
    // 5WHY (复核 2026-08-18, 用户诉求): cancelled 缺 key——groupStats 把取消项
    // 计入 completed，出现 "5/5 完成但徽标不足 5 个"；补全 7 状态聚合。
    // 5WHY (复核 2026-08-18 X/Y 失配): X 曾读 AppState.totalCompleted
    // （m_results.size()，含换 scheme/停用后仍保留的旧结果），Y 读 _agg.total
    // （groupStats 按 runnableFor 过滤）——换 target scheme 不重跑时 X/Y 徽标
    // 对不上。统一为同一数据源：completed 字段补入 _agg，X=Y=_agg。
    // 5WHY (复核 2026-08-18 Reuse C3): 键集合归一化上移到 StatsUtil.js——
    // 新统计键只改一处。
    property var _agg: W.StatsUtil.normalize(null)
    function _refreshAgg() {
        _agg = W.StatsUtil.normalize(AppState.groupStats(-1))
    }
    Connections {
        target: AppState
        function onProgressChanged() { root._refreshAgg() }
        function onRunStatusChanged() { root._refreshAgg() }
    }
    Component.onCompleted: _refreshAgg()

    // 5WHY (复核 2026-08-18 一致性): active 门控曾读 AppState.totalCompleted
    // （m_results.size()，含换 scheme/停用后保留的旧结果）而内容读 _agg——
    // 换 scheme 不重跑时头仍显示但计数塌缩为 0/N。统一同源。
    active: root._agg.completed > 0 || AppState.runStatus === 1
    signal shareRequested(string fmt)

    // 5WHY (复核 2026-08-18, 用户诉求): 归档是两行头——第一行运行指示+状态
    // 词+X/Y 计数；第二行结果统计徽标左对齐（11px 缩进对位）且 7 状态全量。
    // 重构后压缩成单行（fillWidth 标签把徽标推到最右），横屏下徽标与标签挤
    // 在一行、取消态缺失——恢复归档两行结构。
    ColumnLayout {
        Layout.fillWidth: true
        Layout.leftMargin: ThemeEngine.spacing.md
        Layout.rightMargin: ThemeEngine.spacing.md
        spacing: 4

        // ── 第一行：运行指示 + 状态词 + X/Y 计数 ──
        RowLayout {
            Layout.fillWidth: true
            spacing: ThemeEngine.spacing.sm

            // 运行指示（spinner 或状态点）
            Item {
                Layout.preferredWidth: 16; Layout.preferredHeight: 16
                BusyIndicator {
                    visible: AppState.runStatus === 1
                    anchors.fill: parent
                    running: AppState.runStatus === 1
                }
                AppIcon {
                    visible: AppState.runStatus !== 1
                    anchors.fill: parent
                    // 5WHY (review round 3): 取消/错误态曾硬编码绿色 check——
                    // 与同行的 runStatusInfo 标签/文字色脱节（橙字配绿勾）
                    // 5WHY (review round 4): 曾误写 root._statusInfo——本组件
                    // 根 id 是 root（page 未定义 → 绑定求值 ReferenceError，
                    // 状态图标/标签永远损坏）
                    name: (root._statusInfo ? root._statusInfo.iconName : "badge-check"); size: 16
                    color: (root._statusInfo ? root._statusInfo.color : ThemeEngine.colors.success)
                }
            }
            Label {
                // M1：完成/取消/错误三态词 + X/Y 进度计数（归档语义）
                // 5WHY (复核 2026-08-18 X/Y 失配): X 统一改用 _agg.completed
                //（与徽标求和同源），消除换 scheme 后 X/Y 对不上的旧路径。
                text: {
                    if (AppState.runStatus === 1)
                        return (AppState.currentDiagLabel || T.tr("running"))
                               + (_agg.total > 0 ? " · " + ThemeEngine.xyLabel(_agg.completed, _agg.total) : "")
                    if (root._statusInfo) return T.tr(root._statusInfo.labelKey)
                    return ThemeEngine.xyLabel(_agg.completed, _agg.total) + " " + T.tr("completed")
                }
                color: (root._statusInfo ? root._statusInfo.color
                                           : ThemeEngine.colors.onSurfaceVariant)
                font.family: ThemeEngine.fontUi
                font.pixelSize: ThemeEngine.fontSize.body
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            // 8-2：分享按钮从状态头移除（文件图标不该出现在标题栏右侧；
            // 分享入口归位到 Dashboard 报告预览卡）
        }

        // ── 第二行：结果统计徽标，左对齐（全 7 状态）──
        RowLayout {
            Layout.fillWidth: true
            spacing: 4
            // 5WHY (复核 2026-08-18 缩进失配): 11px 是裸归档魔数——首行没有
            // accent bar，只有 16px 状态图标在 x=0；徽标行真正的对位目标是
            // 首行 Label 文本起点 = 图标 16px + spacing.sm 8px = 24px。
            // 行内 spacing 4px，故 Item 宽 = 24 - 4 = 20px。
            Item { Layout.preferredWidth: 20 }
            // 5WHY (复核 2026-08-18 三处复制收敛): 7 徽标行改用共享簇组件
            StatusBadgeCluster { stats: root._agg; compact: ThemeEngine.isMobile }
        }
    }

    // 注入的摘要层（Dashboard headerExtra）
    Loader {
        Layout.fillWidth: true
        active: root.headerExtra !== null
        sourceComponent: root.headerExtra
    }
}
