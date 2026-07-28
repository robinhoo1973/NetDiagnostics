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

    // 5WHY: Badge icons (badge-check, badge-info, badge-warning, etc.)
    // have designer-intended native colored fills.  Image.color replaces
    // ALL source colors with a single tint, which would destroy the native
    // palette.  When the caller has NOT set an explicit color (default
    // "white"), skip colorization so the native fills render.  When the
    // caller HAS set an explicit non-white color, respect it — the caller
    // intentionally wants a monochrome badge in that color.
    // badge-circle has no native fill (white stroke only), so it should
    // always be colorized according to the caller's color property.
    readonly property bool _nativeColored: name.indexOf("badge-") === 0
        && name !== "badge-circle"
        && Qt.colorEqual(root.color, "white")

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
