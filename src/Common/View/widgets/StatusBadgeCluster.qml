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
import QtQuick.Controls
import QtQuick.Layouts
import theme

RowLayout {
    id: root
    property var stats: ({})
    property bool compact: false
    // X/Y 计数标签（组面板显示；状态头首行已有计数故关闭）
    property bool showLabel: false
    spacing: 4

    // 5WHY (复核 2026-08-18 五表漂移): 7 行字面量徽标改为 Repeater 消费
    // ThemeEngine.statusRows 单一来源（与 DashboardSummaryComp 同表）——
    // 加状态/调顺序只改一处。被替换的 DashboardRowHeader 原代码对每个徽标
    // 都有 null 守卫——共享公共组件恢复同等契约。
    Label {
        visible: root.showLabel
        // 5WHY (复核 2026-08-18 Reuse C4): X/Y 组合经 ThemeEngine.xyLabel 单一来源。
        text: ThemeEngine.xyLabel(root.stats ? root.stats.completed : 0,
                                  root.stats ? root.stats.total : 0)
        font.family: ThemeEngine.monoFont
        font.pixelSize: ThemeEngine.fontSize.body
        color: ThemeEngine.colors.onSurfaceVariant
    }
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
