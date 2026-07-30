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
// 5WHY (2026-07-29 _nativeColored removal): Commit fc0cc6b redesigned all
// 53 icons as "bold stroke-only line-art SVG (2.5px)" — every SVG now uses
// fill="none" stroke="#FFFFFF".  No icon has native colored fills.  The
// _nativeColored logic assumed badge icons had designer-intended fills but
// this was invalidated by the icon redesign.  It caused badge icons with
// the default "white" color to render transparent (white stroke on light
// backgrounds = invisible) and prevented colorization from being applied
// to any badge icon during initialization.
//
// Fix: ALL icons are now treated uniformly — the caller's color is always
// applied.  There are no special cases.  If a caller sets color=white on a
// badge icon, it renders white (the caller's choice).  If they want status
// colors, they set the color explicitly (which every existing call site
// already does: passGreen, failRed, warnYellow, etc.).
// =============================================================================
import QtQuick

Item {
    id: root
    property string name: ""
    property color color: "white"
    property int size: 20

    width: size; height: size
    visible: name !== ""

    // 5WHY (2026-07-29 _nativeColored removed): Formerly guarded FBO
    // allocation to only activate when the overlay fallback was active
    // AND the icon wasn't a "native-colored" badge.  Since all 53 icons
    // are now monochrome (fill="none" stroke="#FFFFFF" per fc0cc6b),
    // ALL icons should use the FBO when the overlay is active.  No
    // special cases remain.
    layer.enabled: _useOverlay

    // 5WHY (2026-07-29): _nativeColored REMOVED.  All 53 SVG icons were
    // redesigned as monochrome line-art (fill="none" stroke="#FFFFFF")
    // in commit fc0cc6b.  The old assumption that badge icons had native
    // colored fills no longer holds — every icon is structurally identical.
    // Applying the caller's color to ALL icons is now the correct behavior.
    // Callers that want status-specific colors already set them explicitly
    // (passGreen, failRed, etc.); callers using default "white" get a white
    // icon (their choice — matches the SVG's native stroke).

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
    // Visible only when native Image.color is unavailable.
    Rectangle {
        id: colorOverlay
        anchors.fill: parent
        color: root.color
        // 5WHY: Opacity 0.55 was too low — on light backgrounds the
        // tinted white SVG strokes were nearly invisible.  0.80 gives
        // sufficient color saturation while still allowing the SVG
        // line-art details to show through.
        opacity: 0.80
        visible: root._useOverlay
    }

    // Attempt Qt 6.5+ native SVG colorization.  On platforms where
    // Image.color exists, the assignment succeeds and the overlay stays
    // hidden.  On platforms where it doesn't (iOS static builds), the
    // assignment silently fails, the read-back won't match, and
    // _useOverlay is set true to show the Rectangle fallback.
    //
    // 5WHY (2026-07-29 _nativeColored removed): The old code had a special
    // path for badge icons that set iconImg.color = "transparent" and
    // skipped colorization entirely, relying on the SVG's native fills.
    // Since all 53 icons are now monochrome stroke-only (fc0cc6b), every
    // icon should be colorized uniformly by the caller's color.  The only
    // early-exit is for icons with no name (not loaded).
    function _tryNativeColorization() {
        // Hidden icons (name="") don't need colorization — skipping
        // avoids allocating a wasted FBO via layer.enabled on iOS when
        // the icon will never be visible.
        if (root.name === "") {
            root._useOverlay = false
            return
        }
        // Attempt native Image.color assignment
        iconImg.color = root.color
        // If Image.color doesn't exist on this platform, the assignment
        // above is a silent no-op and reading it back yields a different
        // value (undefined or default transparent) → enable overlay.
        // 5WHY: QML's != operator on color type may not compare RGBA values
        // correctly — Qt documentation says "the == and != operators should
        // not be used directly. Use Qt.colorEqual() instead."  On iOS,
        // iconImg.color reads as undefined (property does not exist), and
        // Qt.colorEqual(undefined, root.color) reliably returns false,
        // correctly enabling the overlay.
        root._useOverlay = !Qt.colorEqual(iconImg.color, root.color)
    }


    // 5WHY: On ARM64 Linux with Qt 6.8, var-property Object.assign
    // may not reliably trigger nested color binding re-evaluation.
    // ThemeEngine._colorsVersion is a plain int property — watching it
    // guarantees re-colorization on every theme switch regardless of
    // the QML engine's var-property tracking quirks.
    Connections {
        target: ThemeEngine
        function on_colorsVersionChanged() { _tryNativeColorization() }
    }

    Component.onCompleted: {
        _tryNativeColorization()
    }

    // When the caller's color binding re-evaluates at runtime (e.g. theme
    // switch), re-run the platform-detection + color-application logic.
    onColorChanged: {
        _tryNativeColorization()
    }

    // When the icon name changes (e.g. "spinner" → "badge-check" in
    // progress indicators), re-apply the correct colorization strategy
    // for the new icon type.
    onNameChanged: {
        _tryNativeColorization()
    }
}
