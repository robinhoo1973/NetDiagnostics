// ShadowIcon.qml — AppIcon with 1px drop-shadow offset to bottom-right
// Light-source convention: light from top-left, shadow to bottom-right.
// Matches Text.Raised styleColor rendering.
import QtQuick
import "../theme"

Item {
    property string iconName: ""
    property color  foregroundColor: "white"
    // 5WHY: #80000000 (50% black) was designed for dark backgrounds.  On light
    // backgrounds, a 50%-opacity black shadow creates a harsh dark halo around
    // every icon — visually heavier than the foreground icon itself.
    // Fix: use a theme-aware default: 50% black for dark theme (invisible
    // surface absorbs it), 15% black for light theme (subtle depth without
    // muddying the icon).  Callers can still override via shadowColor property.
    property color  shadowColor: ThemeEngine.isDark ? Qt.rgba(0, 0, 0, 0.50)
                                                     : Qt.rgba(0, 0, 0, 0.15)
    property int    iconSize: 15
    // 5WHY: 1 logical pixel = Screen.devicePixelRatio physical pixels.
    // At 2x DPR -> 2 phys px, 3x -> 3 phys px. More visible on HiDPI.
    property int    shadowOffset: 1

    implicitWidth: iconSize; implicitHeight: iconSize

    // Drop-shadow layer — offset to bottom-right so it peeks out
    // beyond the foreground icon.
    // 5WHY: anchors.fill with margins does NOT extend beyond parent bounds
    // — it constrains right/bottom to parent.right/parent.bottom, making
    // the shadow narrower instead of peeking out.  Explicit width/height
    // set to parent.width/parent.height ensures the shadow extends
    // shadowOffset pixels beyond parent.right + parent.bottom.
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
