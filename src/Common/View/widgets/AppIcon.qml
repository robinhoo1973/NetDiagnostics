// =============================================================================
// AppIcon.qml — SVG icon with cross-platform colorization
// =============================================================================
// ARCHITECTURE: Visible Image + Rectangle overlay with layer.enabled FBO.
// This is the Phase 2 architecture (pre-ShaderEffect, post-MultiEffect) that
// was the last known-working approach on all platforms.
//
// 5WHY (2026-08-01): After 6 failed ShaderEffect-based fixes, we restored the
// proven Phase 2 design.  The ShaderEffect pipeline (inline GLSL fragmentShader)
// is NOT supported on Qt 6 Metal backend — Qt 6 docs: "ShaderEffect No Longer
// Supports Inline GLSL Shader Strings."  MultiEffect (Phase 1) requires
// QtQuick.Effects which is not in iOS aqt.  Rectangle overlay is the only
// approach using solely QtQuick core types available on all platforms.
//
// Trade-off (iOS only): the semi-transparent overlay tints transparent SVG
// regions, producing a slight colored haze.  On desktop, _useOverlay=false
// (Image.color native tinting) — no overlay, no haze, ideal rendering.
// =============================================================================
import QtQuick

Item {
    id: root
    property string name: ""
    property color color: "white"
    property int size: 20

    width: size; height: size
    implicitWidth: size; implicitHeight: size
    visible: name !== ""

    // layer.enabled: true creates an FBO for the entire Item subtree.
    // Required for the Rectangle overlay to composite correctly over
    // the SVG Image on all platforms including iOS Metal.
    layer.enabled: _useOverlay

    // True when native Image.color is unavailable (iOS static builds).
    // When true, the Rectangle overlay provides colorization instead.
    property bool _useOverlay: false

    Image {
        id: iconImg
        anchors.fill: parent
        source: name ? "qrc:/icons/" + name + ".svg" : ""
        sourceSize.width: size * Screen.devicePixelRatio
        sourceSize.height: size * Screen.devicePixelRatio
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: false
    }

    // Colorization overlay — active when Image.color is unavailable.
    Rectangle {
        id: colorOverlay
        anchors.fill: parent
        color: root.color
        opacity: 0.80  // 5WHY: 0.55 too low — strokes nearly invisible on light bg
        visible: root._useOverlay
    }

    // Detect whether native Image.color is available on this platform.
    // iOS static Qt 6.8.3: Image.color does NOT exist → overlay required.
    // Desktop: Image.color exists → native tinting (no overlay, no haze).
    // Belt-and-suspenders: Qt.platform.os check guards against Qt versions
    // where the JS probe might accidentally create a dynamic color property.
    function _tryNativeColorization() {
        if (root.name === "") {
            root._useOverlay = false
            return
        }
        if (Qt.platform.os === "ios") {
            root._useOverlay = true
            return
        }
        iconImg.color = root.color
        root._useOverlay = !Qt.colorEqual(iconImg.color, root.color)
    }

    Component.onCompleted: _tryNativeColorization()
    onColorChanged: _tryNativeColorization()
    onNameChanged: _tryNativeColorization()
}
