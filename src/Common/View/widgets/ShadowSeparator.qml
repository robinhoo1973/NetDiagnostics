// ShadowSeparator.qml — Thin vertical separator with 1px drop-shadow
// Used in CaptureRunningOverlay to separate time | screenshot | task groups.
// Light-source convention: light from top-left, shadow to bottom-right.
import QtQuick

Rectangle {
    property color lineColor: "white"    // required — overridden at call site
    property color shadowColor: "white"  // required — overridden at call site

    implicitWidth: 1; implicitHeight: 16
    color: lineColor

    // Drop-shadow — offset to bottom-right.
    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 1; anchors.topMargin: 1
        width: parent.width
        height: parent.height
        color: parent.shadowColor
    }
}
