// =============================================================================
// DashboardSummaryComp.qml — 摘要统计层（Summary 单卡合并）
//
// 5WHY (2026-08-19 用户诉求 "两个 Summary 合一 + 零计数不显示"):
// 归档恢复曾平行迁回两张卡——"Summary+Total 头 + 7 类结果行"与"总览卡
// （3 统计 + 分层计时）"——同一页两个 Summary 语义重复、信息割裂。
// 合并为单卡内容：聚合统计（总数/总耗时/已完成）→ 7 类结果行（零计数
// 隐藏）→ 分层计时。卡标题由外层 PageCardSection.cardTitle 承担。
// 数据源：AppState.groupStats(-1) 聚合经 StatsBridge 归一化；总耗时/
// 分层时长由桥的 refreshed 驱动派生（单一刷新归属）。
// UI-2：命令式刷新（绑定不调 Q_INVOKABLE）。
// =============================================================================
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import widgets
import "../../widgets/StatsUtil.js" as W   // 数组身份门控（qrc:/qt/qml/Dashboard/View/ → ../../widgets/）

Item {
    id: root
    implicitHeight: sumCol.implicitHeight

    // 5WHY (复核 2026-08-19 单一归属): 总耗时/分层时长曾由 DashboardScreen
    // 计算后注入——同一张可见卡的数据由两个文件、两套 Connections 分别刷新
    // （5WHY 记录过的"漏接消费方"类缺陷的温床）。合并后本卡完全自持：聚合
    // _s 经 StatsBridge（订阅/归一化/离屏门控/揭示自愈单一订阅点），总耗时
    // _timeText 与分层时长 _layerMs 由桥的 refreshed 驱动派生；
    // DashboardScreen 不再为其做任何 groupStats 扫描。
    property string _timeText: "—"
    // 5WHY (复核 2026-08-19 单一策略): 分层时长的身份门控此前用本文件手写
    // 字符串签名（_layerSig）——与两屏 assignIfChanged 同策略却各写一份。
    // 收敛进 StatsUtil.assignIfChanged：内容未变不替换 _layerMs 身份
    // （Repeater 不重建 5 行）。
    // 5WHY (复核 2026-08-19 双数组合一): _layers（{index,ms} 对象数组）曾与
    // _layerMs（int 数组）平行维护同一数据——行序恒 0..4，Repeater 直接消费
    // _layerMs，index 即组序号、modelData 即 ms。
    property var _layerMs: []

    // 5WHY (复核 2026-08-19 单一订阅点): _s 直接读桥（变属性绑定，桥替换
    // 身份即重估）；离屏门控经桥生效。
    property bool screenVisible: true
    StatsBridge {
        id: stats
        screenVisible: root.screenVisible
    }
    // 5WHY (复核 2026-08-18 Reuse C3): 键集合归一化上移到 StatsUtil.js。
    property var _s: stats._s
    // 5WHY (复核 2026-08-18 单一推导点): _count 曾手算 7 状态求和——C++
    // completed ≡ Σ7 状态（groupStats 单一推导点）；此处复推会在新增第 8
    // 状态桶时与头部/徽标分叉。直接读 _s.completed。
    readonly property int _count: _s.completed

    // 派生层（分层时长 + 墙钟）：桥每次刷新后重算；墙钟另有每秒跳动。
    function _refreshDerived() {
        _refreshTimeOnly()
        // 5WHY (复核 2026-08-19 身份门控): 每事件无条件换新数组 → Repeater
        // 全量销毁重建 5 行（~50 事件/轮 × 5 行 × 6 对象）。分层时长只在
        // 结果落地时变化——ms 数组经 assignIfChanged 比较，未变不替换
        // _layerMs 身份（与组面板滚动哈希/两屏 assignIfChanged 同一策略）。
        var msArr = []
        for (var i = 0; i < 5; ++i) {
            var gs = AppState.groupStats(i)
            msArr.push(gs.durationMs || 0)
        }
        _layerMs = W.assignIfChanged(_layerMs, msArr)
    }
    function _refreshTimeOnly() {
        _timeText = ThemeEngine.formatDuration(AppState.runDurationMs())
    }
    Connections {
        target: stats
        function onRefreshed() { root._refreshDerived() }
    }
    // 5WHY (复核 2026-08-19 初始化时序): _s 是绑定（桥替换身份即重估，无
    // 时序风险），但 _timeText/_layerMs 是 refreshed() 信号驱动——桥的创建期
    // _refresh 发 refreshed() 的时刻与本 Connections 完成注册的时刻先后
    // （QML 子对象 onCompleted 顺序文档未定义）。消费方自身 onCompleted
    // 保证晚于全部子对象完成，幂等补一次派生（分层签名未变零成本）。
    Component.onCompleted: root._refreshDerived()
    Connections {
        target: AppState
        enabled: root.screenVisible
        function onRunElapsedChanged() { root._refreshTimeOnly() }
    }

    ColumnLayout {
        id: sumCol
        anchors.fill: parent
        spacing: 0

        // ── 聚合统计（原总览卡 3 项；总数=Total 头并入，避免重复呈现）──
        StatRow {
            Layout.fillWidth: true
            iconName: "badge-check"; accent: ThemeEngine.colors.tertiary
            valueText: String(root._s.total); label: T.tr("totalDiagsLabel")
        }
        StatRow {
            Layout.fillWidth: true
            iconName: "activity"; accent: ThemeEngine.colors.secondary
            valueText: root._timeText; label: T.tr("totalTimeLabel")
        }
        StatRow {
            Layout.fillWidth: true
            iconName: "check"; accent: ThemeEngine.colors.success
            valueText: String(root._count); label: T.tr("completedLabel")
        }

        // 空态：单行提示（归档行为）
        Label {
            Layout.fillWidth: true
            Layout.topMargin: 6
            visible: root._count === 0
            text: T.tr("runFromDiag")
            font.family: ThemeEngine.monoFont
            font.pixelSize: ThemeEngine.fontSize.caption
            color: Qt.alpha(ThemeEngine.colors.onSurfaceVariant, 0.5)
            horizontalAlignment: Text.AlignHCenter
        }

        // ── 7 类结果彩色行（零计数隐藏：用户诉求 2026-08-19）──
        // 5WHY simplify 2026-08-17：各行仅 accent/icon/label/count 键不同，
        // 且与 ThemeEngine.statusColors/statusIconNames 1:1 对应——一张表
        // 驱动，状态映射不再双份维护。
        // 5WHY (复核 2026-08-18 五表漂移): 表上移到 ThemeEngine.statusRows
        // 单一来源，与 StatusBadgeCluster 同源消费。
        Repeater {
            model: ThemeEngine.statusRows
            StatRow {
                Layout.fillWidth: true
                boxed: true
                accent: ThemeEngine.statusColors[modelData.code] || ThemeEngine.colors.skip
                iconName: ThemeEngine.statusIconNames[modelData.code] || "badge-info"
                label: T.tr(modelData.labelKey)
                valueText: String(root._s[modelData.countKey] || 0)
                // 5WHY (2026-08-19): 曾整组 `visible: _count > 0`——零计数
                // 状态行（如 Skipped）也占 34px 空行，7 行全零时列表冗长。
                // 逐行隐藏，只留实际发生的状态。
                visible: (root._s[modelData.countKey] || 0) > 0
            }
        }

        // ── 分层计时（原总览卡迁移；已完成才显示）──
        // 5WHY (复核 2026-08-19 单一门控): 分割线/间距/标题/行组曾各自重复
        // `visible: _count > 0`（4 处）——整块是一个逻辑单元，收敛为单一
        // 容器门控，新增子元素不再逐个复制守卫。
        ColumnLayout {
            Layout.fillWidth: true
            visible: root._count > 0
            spacing: 0
            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 8
                implicitHeight: 1
                color: ThemeEngine.colors.outlineVariant
            }
            Item { Layout.preferredHeight: 4 }
            Label {
                text: T.tr("layerTimings")
                font.family: ThemeEngine.fontUi
                font.pixelSize: ThemeEngine.fontSize.caption
                font.weight: Font.DemiBold
                color: ThemeEngine.colors.onSurfaceVariant
            }
            Repeater {
                model: root._layerMs
                delegate: RowLayout {
                    Layout.fillWidth: true
                    spacing: ThemeEngine.spacing.sm
                    AppIcon {
                        name: ThemeEngine.groupIconName(index)
                        size: 14
                        color: ThemeEngine.groupHue(index)
                    }
                    Label {
                        Layout.fillWidth: true
                        text: T.groupName(index)
                        font.family: ThemeEngine.monoFont
                        font.pixelSize: ThemeEngine.fontSize.caption
                        color: ThemeEngine.colors.onSurface
                        elide: Text.ElideRight
                    }
                    Label {
                        text: ThemeEngine.formatDuration(modelData || 0)
                        font.family: ThemeEngine.monoFont
                        font.pixelSize: ThemeEngine.fontSize.caption
                        color: ThemeEngine.colors.onSurfaceVariant
                    }
                }
            }
        }
    }

    // ── StatRow（图标 + 标签 + 大数值行；5WHY 复核 2026-08-19）──
    // 摘要卡合并后本文件内曾并存两个近同行组件：无盒 SummaryStat（图标 16/
    // 数值 subhead/行高自内容）与盒式 SummaryCard（图标 14/数值 body/32px
    // 底色行）——同一 AppIcon+标签+加粗数值形状，双份漂移（字号/图标尺寸/
    // 行高各写一次）。合并为单一组件，boxed 开关区分镀铬。
    component StatRow: Rectangle {
        property color accent: ThemeEngine.colors.tertiary
        property string label: ""
        property string iconName: "badge-info"
        property string valueText: ""
        property bool boxed: false
        // 5WHY (复核 2026-08-19 回归): 无盒行曾 implicitHeight: 0——旧 SummaryStat
        // 是 RowLayout（隐式高由内容决定），合并成 Rectangle 后 0 高使三行聚合
        // 统计坍缩重叠。无盒行高按内容（16px 图标 + 上下留白）。
        implicitHeight: boxed ? 32 : 24
        radius: 6
        Layout.topMargin: boxed ? 2 : 0
        color: boxed ? Qt.alpha(accent, 0.06) : "transparent"
        border {
            width: boxed ? 1 : 0
            color: boxed ? Qt.alpha(accent, 0.2) : "transparent"
        }

        RowLayout {
            anchors {
                fill: parent
                leftMargin: boxed ? 8 : 0
                rightMargin: boxed ? 8 : 0
            }
            spacing: boxed ? 8 : 10
            AppIcon {
                name: iconName
                size: boxed ? 14 : 16
                color: accent
                Layout.alignment: Qt.AlignVCenter
            }
            Label {
                Layout.fillWidth: true
                text: label
                font.family: ThemeEngine.monoFont
                font.pixelSize: ThemeEngine.fontSize.caption
                font.weight: boxed ? Font.Medium : Font.Normal
                color: ThemeEngine.colors.onSurfaceVariant
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            Label {
                text: valueText
                font.family: ThemeEngine.monoFont
                font.pixelSize: boxed ? ThemeEngine.fontSize.body : ThemeEngine.fontSize.subhead
                font.weight: Font.Bold
                color: accent
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
