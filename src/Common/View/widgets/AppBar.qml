// AppBar.qml — 共享应用栏（M3 紧凑 48px，归档原样移植）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme

Rectangle {
    id: root

    property string iconName: ""
    property string title: ""

    implicitHeight: 48
    // 5WHY（双重 chrome）：PageHeaderSection 的 Bar 背景已提供 navBar 底色与
    // 底边框，AppBar 再画一遍会叠成 2px 边线。改为透明壳，保留高度契约。
    color: "transparent"
    border { width: 0; color: "transparent" }

    default property alias content: titleRow.data

    RowLayout {
        id: titleRow
        anchors { fill: parent; leftMargin: ThemeEngine.spacing.lg; rightMargin: ThemeEngine.spacing.lg }
        AppIcon {
            name: root.iconName; size: 20
            color: ThemeEngine.colors.cyan
        }
        Item { width: ThemeEngine.spacing.md }
        Label {
            text: root.title
            font.family: ThemeEngine.fontUi; font.pixelSize: ThemeEngine.fontSize.title
            font.weight: Font.DemiBold
            color: ThemeEngine.colors.textPrimary
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
    }
}
