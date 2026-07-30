// =============================================================================
// AppIcon.qml — SVG icon with alpha-preserving colorization via ShaderEffect
// =============================================================================
// 5WHY (2026-07-30 foggy-square): The original Rectangle overlay at 0.80 opacity
// composited via FBO tinted BOTH the icon strokes AND the transparent
// background pixels.  Every icon appeared as a colored square with a foggy
// background — unmistakable on light themes where the colored haze dominates.
//
// 5WHY (2026-07-30 iOS-CI): MultiEffect (QtQuick.Effects) was briefly adopted
// as a replacement, but Qt6QuickEffects is NOT bundled in the iOS aqt package
// (qt.qt6.683.ios).  On iOS, Qt Quick Effects must be installed as a separate
// aqt module — adding a platform-specific dependency that broke iOS CI.
// The inline ShaderEffect uses ONLY QtQuick, which is available on every Qt 6
// platform (iOS, Android, macOS, Windows, Linux) without additional packages.
//
// Root cause: platform-blind optimization.  The inline GLSL ES 2.0 shader
// below is transpiled by Qt's QSB pipeline to the native shading language of
// each platform (Metal on iOS, Vulkan/OpenGL on desktop) — texture2D() works
// correctly on all targets, including ARM64 OpenGL ES 3.0+.
//
// Fix (final): Inline ShaderEffect with explicit ShaderEffectSource capture.
//   1. SVG Image renders white strokes on transparent background
//   2. ShaderEffectSource explicitly captures the Image into an FBO texture
//      — robust across all Qt 6 versions (visible:false Image may not generate
//      a GPU texture in Qt 6.2.x when used directly as a shader source)
//   3. Inline ShaderEffect reads the captured texture, multiplies tint.rgb by
//      tex.a — transparent SVG regions (alpha=0) stay fully transparent,
//      icon strokes (alpha=1) render at full tint saturation.
//   4. No foggy square, no platform-specific Qt module dependencies.
//
// Theme switching: `tint: root.color` is a live QML binding — when
// ThemeEngine.applyTheme() changes colors.xxx, binding re-evaluates
// and the ShaderEffect re-renders automatically.
//
// DPR adaptation: sourceSize uses Screen.devicePixelRatio (not hardcoded 2x)
// so icons render crisply at 1x, 2x, and 3x pixel densities.
// =============================================================================
import QtQuick
import QtQuick.Window

Item {
    id: root
    property string name: ""
    property color color: "white"
    property int size: 20

    // 5WHY: implicitWidth/implicitHeight were missing — placing AppIcon in a
    // Qt Quick Layout without explicit size caused the icon to collapse to
    // 0×0 (Item's default implicit size).  Now Layouts can use AppIcon
    // without explicit width/height bindings.
    implicitWidth: size; implicitHeight: size
    width: size; height: size

    // Defense-in-depth: empty-name icons are always invisible.  The opacity
    // guard is stacked with visible — even if a caller overrides visible,
    // an empty-name icon renders at opacity 0.
    visible: name !== ""
    opacity: name !== "" ? 1.0 : 0.0

    // White SVG — hidden; rendered only through ShaderEffectSource → ShaderEffect.
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

    // 5WHY (2026-07-30 texture-capture): ShaderEffect reading a visible:false
    // Image directly as `source` may fail on Qt 6.2.x where the Item's render
    // texture is not generated for invisible items.  ShaderEffectSource
    // explicitly captures sourceItem into an FBO — guaranteed to work
    // regardless of sourceItem visibility across all Qt 6 versions.
    ShaderEffectSource {
        id: iconSource
        sourceItem: iconImg
        anchors.fill: parent
        visible: false
    }

    // Inline ShaderEffect: colorizes only the non-transparent pixels of the
    // captured SVG texture.  tint.rgb * tex.a → transparent regions (alpha=0)
    // output (0,0,0,0), icon strokes (alpha=1) output the tint color at full
    // opacity.  No fog, no haze, no tinted square.
    //
    // Uses ONLY QtQuick (no QtQuick.Effects) → works on all Qt 6 platforms
    // without additional aqt/packaging steps.  Qt's QSB pipeline transpiles
    // the GLSL ES 2.0 source below to platform-native shaders at build time.
    ShaderEffect {
        anchors.fill: parent
        property color tint: root.color
        property variant source: iconSource
        fragmentShader: "
            varying highp vec2 qt_TexCoord0;
            uniform sampler2D source;
            uniform highp vec4 tint;
            void main() {
                highp vec4 tex = texture2D(source, qt_TexCoord0);
                gl_FragColor = vec4(tint.rgb * tex.a, tex.a);
            }
        "
    }
}
