// =============================================================================
// DialogCard.qml — M3 弹窗卡片壳（dialogs/ 共享）
//
// 5WHY (复核 2026-08-19 弹窗壳抽取): PremiumDialog 与蜂窝数据警告弹窗各自
// 手写同一卡片镀铬——居中卡（宽高双向钳制）+ radius.xl + surfaceContainerLow
// + outlineVariant 边框 + spacing.xl 边距 + 内容 ColumnLayout + RTL 镜像。
// RTL 修复曾在 PremiumDialog 补一次、蜂窝弹窗再复制一次（漂移实证）。
// 壳收敛于此：内容以默认属性子项注入（Layout.* 附着属性可用），规格只
// 在一处维护。
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme

Rectangle {
    id: root
    anchors.centerIn: parent

    // 内容最大宽（Premium 420 / 蜂窝 400）
    property real maxWidth: 420

    width: Math.min(parent.width - 48, root.maxWidth)
    // 5WHY (复核 2026-08-19 高度钳制): 蜂窝弹窗重构后内容增高（56px 图标垫
    // + title 标题 + 44/48 按钮）——横屏手机上 parent.height-48 小于内容
    // 隐式高，Math.min 静默裁掉底部按钮（旧蜂窝弹窗无钳制、内容恒全显）。
    // 业界惯例：高度受限时内容滚动——Flickable 兜底，内容不超界时行为不变。
    height: Math.min(parent.height - 48,
                     bodyFlick.contentHeight + 2 * ThemeEngine.spacing.xl)
    radius: ThemeEngine.radius.xl
    color: ThemeEngine.colors.surfaceContainerLow
    border { width: 1; color: ThemeEngine.colors.outlineVariant }

    // 5WHY (review 2026-08-17): 弹窗缺 LayoutMirroring——阿拉伯语下关闭按钮
    // 停留在错误视觉边；镜像作为壳的固有规格（调用方不再各自复制）。
    LayoutMirroring.enabled: T.isRtl
    LayoutMirroring.childrenInherit: true

    default property alias content: bodyCol.data

    Flickable {
        id: bodyFlick
        anchors.fill: parent
        anchors.margins: ThemeEngine.spacing.xl
        contentWidth: width
        contentHeight: bodyCol.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        ColumnLayout {
            id: bodyCol
            width: parent.width
            spacing: ThemeEngine.spacing.md
        }
    }
}
