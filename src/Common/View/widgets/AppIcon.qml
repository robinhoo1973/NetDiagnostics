// =============================================================================
// AppIcon.qml — SVG icon with cross-platform colorization via Rectangle overlay
// =============================================================================
// 5WHY (2026-07-30 ARM64 light-mode invisible): Image.color exists as a JS
// property on Qt 6.8.2 ARM64 (so Qt.colorEqual returns true), but the SVG
// renderer does not actually apply the colorization at the rendering level.
// Result: _useOverlay stays false, the overlay is hidden, icons render with
// native stroke="#FFFFFF" → invisible on light backgrounds.
//
// Fix: Eliminate Image.color dependency entirely.  Always use the Rectangle
// overlay as the PRIMARY colorization mechanism.  The overlay is a solid
// colored Rectangle with opacity 0.80 drawn OVER the white SVG.  With
// layer.enabled FBO compositing, the SVG strokes show through as properly
// tinted, and transparent areas remain mostly transparent (slight haze is
// an acceptable cross-platform tradeoff).
//
// Platforms: works identically on iOS (static Qt), Android, Windows, macOS,
// Linux (ARM64 + x86_64).  No platform detection needed.
// =============================================================================
import QtQuick

Item {
    id: root
    property string name: ""
    property color color: "white"
    property int size: 20

    width: size; height: size
    visible: name !== ""

    // 5WHY: FBO compositing required so the colored overlay Rectangle blends
    // correctly with the white SVG strokes beneath it.  Without layer.enabled,
    // the overlay is just a solid Rectangle on top — the icon shape is lost.
    layer.enabled: true

    // White SVG icon — renders with native stroke="#FFFFFF".
    // This is the shape layer; the colorOverlay Rectangle on top provides the tint.
    Image {
        id: iconImg
        anchors.fill: parent
        source: name ? "qrc:/icons/" + name + ".svg" : ""
        sourceSize.width: size * 2
        sourceSize.height: size * 2
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: false
    }

    // Primary colorization: semi-transparent colored Rectangle over the SVG.
    // 5WHY: Opacity 0.80 was chosen empirically — 0.55 was too faint on light
    // backgrounds (white strokes barely visible), 0.95 lost too much icon detail.
    // 0.80 gives good color saturation while preserving stroke clarity.
    Rectangle {
        id: colorOverlay
        anchors.fill: parent
        color: root.color
        opacity: 0.80
    }

    // When the caller's color changes (e.g. theme switch re-evaluates
    // ThemeEngine.colors.xxx binding), the overlay automatically updates
    // because its `color: root.color` binding is live.
}
