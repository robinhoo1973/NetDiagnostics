// =============================================================================
// AppIcon.qml — SVG icon with alpha-preserving colorization via MultiEffect
// =============================================================================
// 5WHY (2026-07-30 foggy-square v2): The inline ShaderEffect with texture2D()
// may fail to compile on ARM64 OpenGL ES 3.0+ backends where texture2D is
// deprecated and Qt's QSB fallback fails to transpile.  When the shader
// doesn't compile, layer.effect is a silent no-op — the white SVG renders
// as-is, invisible on light themes.
//
// Fix: Replace inline ShaderEffect with QtQuick.Effects.MultiEffect.
// MultiEffect uses QSB-precompiled shaders, works on all Qt 6 platforms
// (including ARM64), and its colorization pipeline is tested by the Qt CI.
//
// How it works:
//   1. SVG Image renders white strokes on transparent background
//   2. ShaderEffectSource explicitly captures the Image into an FBO texture
//      — robust on all Qt 6 versions (visible:false Image may not generate
//      a texture in Qt 6.2.x when used as a direct MultiEffect source)
//   3. MultiEffect reads the captured texture, applies colorizationColor tint
//   4. Alpha channel is preserved — transparent areas stay transparent
//   5. Result: vivid icon on perfectly transparent background, no fog
//
// Theme switching: colorizationColor is bound to root.color which is
// bound to ThemeEngine.colors.xxx → updates automatically.
//
// DPR adaptation: sourceSize uses Screen.devicePixelRatio (not hardcoded 2x)
// so icons render crisply at 1x, 2x, and 3x pixel densities.
// =============================================================================
import QtQuick
import QtQuick.Window
import QtQuick.Effects

Item {
    id: root
    property string name: ""
    property color color: "white"
    property int size: 20

    implicitWidth: size; implicitHeight: size
    width: size; height: size

    // Defense-in-depth: empty-name icons are always invisible
    visible: name !== ""
    opacity: name !== "" ? 1.0 : 0.0

    // White SVG — hidden; rendered only through ShaderEffectSource → MultiEffect.
    // fill="none" stroke="#FFFFFF" — only strokes are opaque, fill is transparent.
    // sourceSize uses dynamic DPR to avoid blur on 3x screens and wasted VRAM on 1x.
    Image {
        id: iconImg
        visible: false
        anchors.fill: parent
        source: name ? "qrc:/icons/" + name + ".svg" : ""
        sourceSize.width: size * Screen.devicePixelRatio
        sourceSize.height: size * Screen.devicePixelRatio
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: false
    }

    // 5WHY (2026-07-30 texture-capture): MultiEffect reading a visible:false
    // Image directly as `source` may fail on Qt 6.2.x where the Item's render
    // texture is not generated for invisible items.  ShaderEffectSource
    // explicitly captures sourceItem into an FBO — this is guaranteed to work
    // regardless of sourceItem visibility across all Qt 6 versions.
    ShaderEffectSource {
        id: iconSource
        sourceItem: iconImg
        anchors.fill: parent
        visible: false
    }

    // QSB-precompiled colorization effect.  Replaces white pixels with
    // colorizationColor while preserving alpha — transparent SVG regions
    // remain perfectly transparent (no fog, no haze, no tinted square).
    MultiEffect {
        source: iconSource
        colorizationColor: root.color
        colorization: 1.0
        anchors.fill: parent
    }
}
