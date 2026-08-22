// =============================================================================
// DialogCard.qml — M3 弹窗卡片壳（dialogs/ 共享）
//
// 5WHY (复核 2026-08-19 弹窗壳抽取): PremiumDialog 与蜂窝数据警告弹窗各自
// 手写同一卡片镀铬——居中卡（宽高双向钳制）+ radius.xl + surfaceContainerLow
// + outlineVariant 边框 + spacing.xl 边距 + 内容 ColumnLayout + RTL 镜像。
// RTL 修复曾在 PremiumDialog 补一次、蜂窝弹窗再复制一次（漂移实证）。
// 壳收敛于此：内容以默认属性子项注入（Layout.* 附着属性可用），规格只
// 在一处维护。
// 5WHY (复核 2026-08-20 主操作埋没): 高钳制时整卡滚屏把底部主按钮滚出
// 首屏（AsNeeded 滚动条触屏不可见、无滚动提示）——用户够不到"继续/购买"
// 即流程卡死。业界对话框惯例：内容滚动、操作行钉底恒可见。footer 槽
// 供操作行注入（property-object 语法），内容/操作分离。
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
    // 操作行槽（钉底恒可见；Layout.* 附着属性可用）
    property list<Item> footer

    // 5WHY (复核 2026-08-20 下界): 父容器 <48px 时钳制为负——补下界。
    width: Math.max(0, Math.min(parent.width - 48, root.maxWidth))
    // 5WHY (复核 2026-08-19 高度钳制): 蜂窝弹窗重构后内容增高（56px 图标垫
    // + title 标题 + 44/48 按钮）——横屏手机上 parent.height-48 小于内容
    // 隐式高，Math.min 静默裁掉底部按钮（旧蜂窝弹窗无钳制、内容恒全显）。
    // 业界惯例：高度受限时内容滚动——Flickable 兜底，内容不超界时行为不变。
    height: Math.max(0, Math.min(parent.height - 48,
                     cardCol.implicitHeight + 2 * ThemeEngine.spacing.xl))
    radius: ThemeEngine.radius.xl
    color: ThemeEngine.colors.surfaceContainerLow
    border { width: 1; color: ThemeEngine.colors.outlineVariant }

    // 5WHY (review 2026-08-17): 弹窗缺 LayoutMirroring——阿拉伯语下关闭按钮
    // 停留在错误视觉边；镜像作为壳的固有规格（调用方不再各自复制）。
    LayoutMirroring.enabled: T.isRtl
    LayoutMirroring.childrenInherit: true

    default property alias content: bodyCol.data

    ColumnLayout {
        id: cardCol
        anchors.fill: parent
        anchors.margins: ThemeEngine.spacing.xl
        spacing: ThemeEngine.spacing.md

        Flickable {
            id: bodyFlick
            Layout.fillWidth: true
            Layout.fillHeight: true
            // 5WHY (2026-08-23 用户 "弹窗只剩按钮无文本"): 卡片高度取
            // cardCol.implicitHeight，而 Flickable 在 Qt 6.8 不把
            // contentHeight 回传给 implicitHeight——fillHeight 项的隐式高
            // 为 0，卡片坍缩成 footer+边距（实测 106px），主体内容被
            // clip 裁掉，只剩继续/取消按钮。显式声明隐式高 = 内容高：
            // 卡片恢复全高；屏幕钳制时 Flickable 仍可滚动主体。
            implicitHeight: bodyCol.implicitHeight
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
        ColumnLayout {
            id: footerCol
            Layout.fillWidth: true
            spacing: ThemeEngine.spacing.sm
            data: root.footer
            visible: footerCol.data.length > 0
        }
    }
}
