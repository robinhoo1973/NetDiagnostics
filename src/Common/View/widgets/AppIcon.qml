// =============================================================================
// AppIcon.qml — SVG icon with Qt 6.5+ native SVG colorization
// =============================================================================
// 5WHY: The previous Rectangle-overlay approach (55% opacity colored rect)
// produced visible "foggy square box" artifacts around every icon —
// transparent SVG regions were tinted by the overlay, creating a visible
// colored haze on all platforms, especially Mali/Adreno GPUs.
//
// Qt 6.5 added native SVG colorization via Image.color.  This tints the
// SVG strokes directly in the vector renderer — no overlay, no FBO, no
// alpha compositing artifacts.  The background is rendered as fully
// transparent because the SVG viewBox has no background fill.
//
// Badge icons (badge-check, badge-warning, etc.) have native colored fills
// designed by the icon artist.  Image.color would replace these fills with
// the caller's single color, destroying the designer-intended palette.
// _nativeColored detects badge icons and skips Image.color, rendering the
// SVG with its original fills.
// =============================================================================
import QtQuick

Item {
    id: root
    property string name: ""
    property color color: "white"
    property int size: 20

    width: size; height: size
    visible: name !== ""

    // 5WHY: Badge icons have native colored circle fills (green check,
    // red X, yellow warning, etc.).  Image.color replaces ALL colors with
    // a single tint, which would destroy the designer's palette.  Detect
    // badge icons by name prefix and skip colorization for them.
    // badge-circle has no native fill (white stroke only), so it should
    // still be colorized when the caller sets an explicit color.
    // 5WHY: badge icons have designer-intended native colored fills
    // (green check, red X, yellow warning, purple info, etc.).
    // Image.color replaces ALL colors with a single tint, which would
    // destroy the native palette.  Skip colorization for ALL badge
    // icons so their native fills render — regardless of the caller's
    // color property (which may carry a theme color from the parent).
    // badge-circle has no native fill (white stroke only), so it
    // should still be colorized when the caller sets an explicit color.
    readonly property bool _nativeColored: name.indexOf("badge-") === 0
        && name !== "badge-circle"

    Image {
        id: iconImg
        anchors.fill: parent
        source: name ? "qrc:/icons/" + name + ".svg" : ""
        sourceSize.width: size * 2
        sourceSize.height: size * 2
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: false
        // Qt 6.5+ native SVG colorization: tints vector strokes directly
        // in the renderer.  Produces fully transparent backgrounds because
        // the SVG viewBox has no fill.  Skipped for badge icons that have
        // their own designer-intended colored fills.
        color: root._nativeColored ? "transparent" : root.color
    }
}
