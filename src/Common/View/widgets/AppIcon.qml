// =============================================================================
// AppIcon.qml — SVG icon with reliable cross-platform colorization
// =============================================================================
// 5WHY: MultiEffect (QtQuick.Effects) is unavailable on iOS static Qt builds —
// the linker may strip the module, and MultiEffect's GPU shaders may not
// compile for the iOS rendering backend.  Using a semi-transparent colored
// Rectangle overlay provides a universal colorization fallback that works on
// ALL platforms without any effect module dependency.
//
// ⚠ CRITICAL CONSTRAINT: The Image MUST be visible so the Rectangle overlay
//   has content to tint.  This means SVG icon source files MUST use pure
//   white strokes (#FFFFFF).  Non-white strokes (currentColor, #C0C0D0, dark
//   fills) will bleed through the 55%-opacity overlay as ghost artifacts —
//   the raw stroke color mixes with the overlay color instead of being fully
//   replaced.  Every icon in resources/icons/ must be audited for #FFFFFF
//   strokes before being added to the QRC.
//
// Visual tradeoff: the overlay TINTS the icon rather than performing full
// hue+saturation colorization.  White SVG icons tinted with a 55%-opacity
// color overlay read as the target color with a slight luminance drop
// compared to MultiEffect.  This is acceptable — reliability across all
// platforms > visual perfection for diagnostic evidence.
// =============================================================================
import QtQuick

Item {
    id: root
    property string name: ""
    property color color: "white"
    property int size: 20

    width: size; height: size
    visible: name !== ""

    // 5WHY (hazy-square-box root cause): The Rectangle color overlay at 55%
    // opacity covered the entire Item area, including transparent SVG regions
    // (the 1-unit margin between the 22-diameter badge circle and the 24x24
    // viewBox).  On Mali/Adreno embedded GPUs, two rendering-pipeline defects
    // combine to make this artifact prominent:
    //   1. SVG render path may use opaque QImage surfaces — transparent
    //      viewBox corners become filled with default background color.
    //   2. mipmap:true downsamples without preserving the alpha channel,
    //      turning what should be sub-pixel transparent corners into
    //      visible semi-opaque haze.
    // Fix: layer.enabled forces Qt to composite the entire icon subtree into
    // an alpha-aware Frame Buffer Object before display.  mipmap:false
    // prevents alpha loss during texture downsampling.  The 55%-opacity
    // Rectangle overlay is retained as the universal colorization fallback
    // for platforms without QtQuick.Effects/MultiEffect.
    // 5WHY (round 2): layer.samples: 4 enabled MSAA on the FBO.  On Mali/Adreno
    // embedded GPUs, the multisample resolve step does NOT preserve the alpha
    // channel — transparent SVG regions become semi-opaque, producing the
    // "foggy square box" artifact around every icon.  Removing the samples
    // keeps the single-sample FBO which correctly preserves alpha on all GPUs.
    layer.enabled: true

    Image {
        id: iconImg
        anchors.fill: parent
        source: name ? "qrc:/icons/" + name + ".svg" : ""
        sourceSize.width: size * 2
        sourceSize.height: size * 2
        fillMode: Image.PreserveAspectFit
        smooth: true
        // 5WHY: mipmap generation on Mali/Adreno embedded GPUs does not
        // preserve the alpha channel — transparent SVG regions become
        // semi-opaque, creating a visible rectangular haze around icons.
        mipmap: false
    }
    // 5WHY: Badge icons (badge-check, badge-info, badge-warning, badge-error,
    // badge-close, badge-skip, badge-refresh) use native colored circle fills
    // (#4ADE80 green, #A5B4FC purple, #FBBF24 yellow, #F87171 red, #9CA3AF
    // gray, #06B6D4 cyan).  Applying the 55%-opacity overlay over these fills
    // produces muddy mixed colors — the native fill bleeds through and mixes
    // with the overlay color.
    //
    // 5WHY (round 2): Auto-detecting by naming convention was too aggressive —
    // callers that explicitly set `color` (e.g. warnYellow on badge-check,
    // cyan on badge-info, textMuted on a disabled toggle) expect their color
    // to work.  Silently suppressing the overlay broke the API contract.
    //
    // 5WHY (round 3): Adding nativeColor:false at every affected call site
    // is a bandaid — the component can auto-detect caller intent.  If the
    // caller set a non-default color, they want the overlay.  If the color is
    // still "white" (the default), the native SVG fill is preferred.  The
    // edge case of explicitly setting color:"white" on a badge icon is a
    // no-op — white overlay over white SVG = same white icon.
    //
    // badge-circle.svg has NO native colored fill — it renders as a white
    // stroke circle, so hiding the overlay makes it pure white and silently
    // ignores the caller's explicit color.  Exclude it from the default.
    readonly property bool _nativeColored: name.indexOf("badge-") === 0
        && name !== "badge-circle"
        && root.color === "white"   // caller didn't set an explicit color

    // Universal colorization fallback — semi-transparent color overlay.
    // Works without QtQuick.Effects (MultiEffect/ColorOverlay), making it
    // reliable on iOS static builds where the Effects module is unavailable.
    // Hidden when nativeColor is true (badge icons rendering with their
    // designer-intended palette).
    Rectangle {
        anchors.fill: parent
        color: root.color
        opacity: 0.55
        visible: !root._nativeColored
    }
}
