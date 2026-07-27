// ShadowIcon.qml — AppIcon with 1px drop-shadow offset to bottom-right
// Light-source convention: light from top-left, shadow to bottom-right.
// Matches Text.Raised styleColor rendering.
import QtQuick

Item {
    property string iconName: ""
    property color  foregroundColor: "white"
    property color  shadowColor: "#80000000"
    property int    iconSize: 15
    // 5WHY: 1 logical pixel = Screen.devicePixelRatio physical pixels.
    // At 2x DPR -> 2 phys px, 3x -> 3 phys px. More visible on HiDPI.
    property int    shadowOffset: 1

    implicitWidth: iconSize; implicitHeight: iconSize

    // Drop-shadow layer — offset to bottom-right so it peeks out
    // beyond the foreground icon.
    AppIcon {
        anchors.left: parent.left
        anchors.leftMargin: parent.shadowOffset
        anchors.top: parent.top
        anchors.topMargin: parent.shadowOffset
        width: parent.width
        height: parent.height
        name: parent.iconName; size: parent.iconSize
        color: parent.shadowColor
    }
    // Foreground icon
    AppIcon {
        anchors.fill: parent
        name: parent.iconName; size: parent.iconSize
        color: parent.foregroundColor
    }
}
