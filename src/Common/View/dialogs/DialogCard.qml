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
import QtQuick.Layouts
import theme

Rectangle {
    id: root
    anchors.centerIn: parent

    // 内容最大宽（Premium 420 / 蜂窝 400）
    property real maxWidth: 420

    width: Math.min(parent.width - 48, root.maxWidth)
    height: Math.min(parent.height - 48, bodyCol.implicitHeight + 2 * ThemeEngine.spacing.xl)
    radius: ThemeEngine.radius.xl
    color: ThemeEngine.colors.surfaceContainerLow
    border { width: 1; color: ThemeEngine.colors.outlineVariant }

    // 5WHY (review 2026-08-17): 弹窗缺 LayoutMirroring——阿拉伯语下关闭按钮
    // 停留在错误视觉边；镜像作为壳的固有规格（调用方不再各自复制）。
    LayoutMirroring.enabled: T.isRtl
    LayoutMirroring.childrenInherit: true

    default property alias content: bodyCol.data

    ColumnLayout {
        id: bodyCol
        anchors.fill: parent
        anchors.margins: ThemeEngine.spacing.xl
        spacing: ThemeEngine.spacing.md
    }
}
