// =============================================================================
// AppIcon.qml - statically pre-colored SVG icon (NO runtime colorization)
// =============================================================================
// 5WHY (2026-08-01): Every runtime colorization mechanism failed on at least
// one platform (ShaderEffect inline GLSL: no Metal support; MultiEffect:
// QtQuick.Effects absent from iOS aqt; Image.color: property never existed;
// Rectangle overlay: unmasked foggy square).  Icons are now pre-generated per
// palette color by scripts/generate-colored-icons.py into qrc:/icons-gen/.
// This component only SELECTS the nearest pre-generated color variant and
// expresses alpha via Image.opacity.  Zero shaders, zero effects, zero
// overlays - identical rendering on every platform.
// =============================================================================
import QtQuick
import "IconColors.js" as IconColors

Item {
    id: root
    property string name: ""
    property color color: "white"
    property int size: 20

    width: size; height: size
    implicitWidth: size; implicitHeight: size
    // 5WHY: visible must NOT depend on color.a — Layouts skip invisible
    // items, so "transparent" callers (CaptureModePanel check icons) would
    // cause layout shift.  Alpha-hiding is handled by Image.opacity below,
    // which preserves layout geometry.
    visible: name !== ""

    // Nearest pre-generated palette color (RGB distance).  Callers pass
    // palette colors so this is normally an exact match; Qt.lighter()/
    // arbitrary colors snap to the closest generated variant.
    readonly property string _hex: {
        var best = "#FFFFFF"
        var bd = 1e9
        for (var i = 0; i < IconColors.hexes.length; i++) {
            var h = IconColors.hexes[i]
            var r = parseInt(h.substr(1, 2), 16) / 255.0
            var g = parseInt(h.substr(3, 2), 16) / 255.0
            var b = parseInt(h.substr(5, 2), 16) / 255.0
            var d = (r - color.r) * (r - color.r)
                  + (g - color.g) * (g - color.g)
                  + (b - color.b) * (b - color.b)
            if (d < bd) { bd = d; best = h }
        }
        return best
    }

    Image {
        anchors.fill: parent
        source: root.name
                ? "qrc:/icons-gen/" + root._hex.substr(1).toLowerCase()
                  + "/" + root.name + ".svg"
                : ""
        sourceSize.width: root.size * Screen.devicePixelRatio
        sourceSize.height: root.size * Screen.devicePixelRatio
        fillMode: Image.PreserveAspectFit
        smooth: true
        // Alpha (Qt.alpha() callers) via opacity - not colorization.
        opacity: root.color.a
    }
}