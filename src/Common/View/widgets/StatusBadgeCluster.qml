// =============================================================================
// StatusBadgeCluster.qml — 7 状态徽标簇（单一来源）
// =============================================================================
// 5WHY (复核 2026-08-18): 同一组 7 行 StatusBadge 在 PageStatusHeaderSection /
// PageGroupPanelSection.BuiltinStats / DashboardRowHeader 三处逐字复制——加
// cancelled 徽标、调顺序、改配色必须三处同步，漏一处即出现用户可见的计数
// 失配（历史已发生两次）。收敛为共享组件；DashboardSummaryComp 的纵向
// 每状态一行布局是不同模式，不在此簇范围。
//
// 窄屏适配（Fix 1）：compact 模式缩小图标/字号，7 徽标行在 320dp 手机
// 上可容纳（徽标 ~28px × 7 + 间距 ≈ 220px < 296px 可用宽度）。
// =============================================================================
import QtQuick
import QtQuick.Layouts
import theme

RowLayout {
    id: root
    property var stats: ({})
    property bool compact: false
    spacing: 4

    // 5WHY (复核 2026-08-18 五表漂移): 7 行字面量徽标改为 Repeater 消费
    // ThemeEngine.statusRows 单一来源（与 DashboardSummaryComp 同表）——
    // 加状态/调顺序只改一处。被替换的 DashboardRowHeader 原代码对每个徽标
    // 都有 null 守卫——共享公共组件恢复同等契约。
    // 5WHY (2026-08-19 用户诉求 "X/Y 在第一行、状态图标在第二行"):
    // 内联 X/Y 标签（showLabel）曾是组面板专用——两行头重构后 X/Y 移入
    // 组头首行（PageGroupPanelSection），属性零消费方，随 Label 一并删除。
    Repeater {
        model: ThemeEngine.statusRows
        StatusBadge {
            required property var modelData
            statusCode: modelData.code
            count: root.stats ? (root.stats[modelData.countKey] || 0) : 0
            compact: root.compact
        }
    }
}
