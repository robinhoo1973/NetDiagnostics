// PageOverlaySection.qml — 浮层容器（§2 清单：detailOverlay 等）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import core
import theme

PageSection {
    id: root
    anchors.fill: parent
    backgroundStyle: PageSection.Plain
    // visible 为 Item FINAL 属性：由使用者直接绑定，此处不做 active 联动
    active: true

    // 5WHY (review 2026-08-17): 旧代码 onClicked: root.visible = false 用命令式
    // 赋值永久删除调用方对 visible 的绑定——第二次打开浮层永远不可见，且
    // overlayVisible 仍为 true 卡死导航。改为信号：状态变更归调用方（绑定不破）。
    signal closeRequested()

    Rectangle {
        id: mask
        // 8-18 注记：mask 经 PageSection 默认属性落入 body(ColumnLayout)，
        // 由 Layout.fillWidth/Height 铺满——勿改 anchors（布局管理项用 anchors
        // 属 UB）。浮层内容经 default property 重定向进 mask.data。
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: ThemeEngine.colors.scrim   // 半透明遮罩
        visible: root.visible
        // 5WHY (2026-08-22 UX-5): 遮罩曾无 Accessible 标记——读屏把纯装饰
        // 点击层当匿名控件播报。装饰性遮罩忽略之；关闭语义由显式关闭控
        // 件（PageDetailSheet 返回钮/对话框按钮）承担。
        MouseArea {
            anchors.fill: parent
            onClicked: root.closeRequested()
            Accessible.ignored: true
        }
    }
    // 覆盖基类 default property：内容直接进遮罩（自由定位，不进 body 布局）
    default property alias overlayContent: mask.data
}
