// =============================================================================
// IconActionButton.qml — 44px 命中区 + 键盘可达的图标操作按钮
//
// 5WHY (simplify 2026-08-17): 同一脚手架（Item + focusPolicy + Enter/Space +
// Accessible + AppIcon + MouseArea）在 PageDetailHeaderSection 写了两遍，
// PremiumDialog 的关闭按钮第三遍却漏了键盘支持——行为已经漂移。抽为共享
// 组件后，命中区/键盘/a11y 策略只在一处维护。
// =============================================================================
import QtQuick
import theme
import widgets

Item {
    id: root
    property string iconName: "circle"
    property int iconSize: 18
    property color iconColor: ThemeEngine.colors.onSurfaceVariant
    property bool iconMirror: false
    property real iconRotation: 0
    signal activated()

    implicitWidth: 44
    implicitHeight: 44
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.Button

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
            || event.key === Qt.Key_Space) {
            root.activated()
            event.accepted = true
        }
    }
    AppIcon {
        anchors.centerIn: parent
        name: root.iconName
        size: root.iconSize
        color: root.iconColor
        mirror: root.iconMirror
        rotation: root.iconRotation
    }
    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.activated()
    }
}
