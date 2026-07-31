// =============================================================================
// AppIcon.qml — SVG icon with cross-platform colorization
// =============================================================================
// 5WHY (2026-07-31 Qt6-Metal-inline-GLSL): After 60 fix attempts across 5
// sessions, the root cause is architectural: Qt 6 ShaderEffect NO LONGER
// SUPPORTS inline GLSL fragmentShader strings on Metal/Vulkan/D3D11 backends.
// iOS statically forces the Metal backend — inline GLSL silently fails to
// compile, producing no shader output.  Desktop "works" only because the
// OpenGL backend still accepts legacy inline GLSL as a compatibility path.
//
// Fix: eliminate the ShaderEffect pipeline entirely.  Use the simple,
// proven approach: visible SVG Image + semi-transparent Rectangle overlay.
// The overlay produces a slight haze on transparent SVG regions (the
// "foggy square" artifact) which is an acceptable visual trade-off vs.
// completely invisible icons.  opacity 0.55 balances color visibility
// against background bleed.
//
// This is the same architecture that shipped and worked on iOS before the
// ShaderEffect migration (commit d6042b94).
// =============================================================================
import QtQuick
import QtQuick.Window

Item {
    id: root
    property string name: ""
    property color color: "white"
    property int size: 20

    implicitWidth: size; implicitHeight: size
    width: size; height: size
    visible: name !== ""

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

    // Colorization overlay — visible when a non-white color is set.
    // Works on all Qt 6 platforms without shader dependencies.
    Rectangle {
        anchors.fill: parent
        color: root.color
        opacity: 0.55
        visible: root.color !== "white" && root.color.a > 0
    }
}
