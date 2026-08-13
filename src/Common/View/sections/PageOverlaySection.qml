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

    Rectangle {
        id: mask
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: ThemeEngine.colors.scrim   // 半透明遮罩
        visible: root.visible
        MouseArea { anchors.fill: parent; onClicked: root.visible = false }
    }
    // 覆盖基类 default property：内容直接进遮罩（自由定位，不进 body 布局）
    default property alias overlayContent: mask.data
}
