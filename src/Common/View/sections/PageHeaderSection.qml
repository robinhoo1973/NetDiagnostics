// PageHeaderSection.qml — AppBar 壳（§2.1）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import widgets
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Bar
    fixedHeight: appbar.implicitHeight   // 5WHY UI-1：Bar 区块必须显式高度契约

    property string iconName: ""
    property string title: ""

    AppBar {
        id: appbar
        Layout.fillWidth: true
        iconName: root.iconName
        title: root.title
    }
    default property alias extra: appbar.content
}
