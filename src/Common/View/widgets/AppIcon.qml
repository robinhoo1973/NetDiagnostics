// =============================================================================
// AppIcon.qml — SVG icon with cross-platform colorization
// =============================================================================
// 5WHY (2026-07-29 iOS startup crash): Image.color is a Qt 6.5+ feature for
// native SVG vector colorization.  On iOS static Qt 6.8.3 builds, this
// property does NOT exist — a static property binding to it causes QML
// component creation to fail, cascading through AppBar → DiagnosticScreen →
// AppContent → main.qml → engine.load() returns 0 root objects → app exits.
//
// Fix strategy: do NOT set Image.color as a static QML binding.  Instead,
// attempt dynamic assignment in Component.onCompleted (JavaScript runtime).
// If the read-back after assignment does not match the target color, native
// colorization is unavailable → enable the Rectangle overlay fallback.
//
// Platforms with Qt 6.5+ Image.color (desktop, Android): native SVG tinting
// – no overlay, no FBO, no alpha-compositing artifacts, fully transparent
// backgrounds.
//
// Platforms without Image.color (iOS static builds): semi-transparent
// colored Rectangle overlay with layer.enabled FBO compositing – slight
// haze on transparent SVG regions is an acceptable trade-off for not
// crashing.
//
// Badge icons (badge-check, badge-warning, etc.) have native colored fills
// designed by the icon artist.  _nativeColored detects badge icons and skips
// ALL colorization, rendering the SVG with its original fills on all platforms.
// =============================================================================
import QtQuick

Item {
    id: root
    property string name: ""
    property color color: "white"
    property int size: 20

    width: size; height: size
    visible: name !== ""

    // 5WHY: layer.enabled forces Qt to composite the entire icon subtree
    // (Image + optional Rectangle overlay) into an alpha-aware Frame Buffer
    // Object.  Without this, the overlay fallback on iOS would produce
    // "foggy square box" artifacts around every icon — the overlay tints
    // transparent SVG regions.  The FBO properly composites alpha so the
    // overlay only affects actual SVG stroke pixels.  No layer.samples
    // (MSAA resolve loses alpha on Mali/Adreno GPUs).
    layer.enabled: true

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

    // True when native Image.color is unavailable on this platform
    // (iOS static Qt builds) — Rectangle overlay is active instead.
    property bool _useOverlay: false

    Image {
        id: iconImg
        anchors.fill: parent
        source: name ? "qrc:/icons/" + name + ".svg" : ""
        sourceSize.width: size * 2
        sourceSize.height: size * 2
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: false
        // Image.color is intentionally NOT set as a static property binding.
        // It does not exist on iOS static Qt 6.8.3 builds, and a static
        // binding to a non-existent property causes QML component creation
        // to fail (the "Cannot assign to non-existent property" fatal error).
        // Dynamic assignment via _tryNativeColorization() attempts to set
        // it at JavaScript runtime; if the property is absent, assignment
        // silently fails and _useOverlay enables the Rectangle fallback below.
    }

    // Fallback: semi-transparent colored Rectangle over the white SVG icon.
    // Visible only when native Image.color is unavailable AND the icon is
    // not a native-colored badge (which needs its designer palette intact).
    Rectangle {
        id: colorOverlay
        anchors.fill: parent
        color: root.color
        opacity: 0.55
        visible: root._useOverlay && !root._nativeColored
    }

    // Attempt Qt 6.5+ native SVG colorization.  On platforms where
    // Image.color exists, the assignment succeeds and the overlay stays
    // hidden.  On platforms where it doesn't (iOS static builds), the
    // assignment silently fails, the read-back won't match, and
    // _useOverlay is set true to show the Rectangle fallback.
    function _tryNativeColorization() {
        if (root._nativeColored) {
            // Badge icon with designer palette — skip ALL colorization
            root._useOverlay = false
            return
        }
        // Attempt native Image.color assignment
        iconImg.color = root.color
        // If Image.color doesn't exist on this platform, the assignment
        // above is a silent no-op and reading it back yields a different
        // value (undefined or default transparent) → enable overlay.
        root._useOverlay = (iconImg.color != root.color)
    }

    Component.onCompleted: {
        _tryNativeColorization()
    }

    // When the caller's color binding re-evaluates at runtime (e.g. theme
    // switch), re-run the platform-detection + color-application logic.
    onColorChanged: {
        _tryNativeColorization()
    }
}
