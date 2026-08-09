// =============================================================================
// AppIcon.qml - statically pre-colored SVG icon (NO runtime colorization)
// =============================================================================
// 5WHY (2026-08-01): Every runtime colorization mechanism failed on at least
// one platform (ShaderEffect inline GLSL: no Metal support; MultiEffect:
// QtQuick.Effects absent from iOS aqt; Image.color: property never existed;
// Rectangle overlay: unmasked foggy square).  Icons are now pre-generated per
// palette color by scripts/generate-colored-icons.py into qrc:/icons/.
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
    // 5WHY: Directional glyphs (chevron-right etc.) are baked into static SVGs
    // and do NOT flip under LayoutMirroring — layout order mirrors but the
    // glyph stays LTR.  Callers set mirror: T.isRtl on directional icons so
    // the arrow points along the reading direction (e.g. collapse chevron →
    // left in Arabic).  Horizontal flip via Scale — no shader, no new assets.
    property bool mirror: false

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
                ? "qrc:/icons/" + root._hex.substr(1).toLowerCase()
                  + "/" + root.name + ".svg"
                : ""
        sourceSize.width: root.size * Screen.devicePixelRatio
        sourceSize.height: root.size * Screen.devicePixelRatio
        fillMode: Image.PreserveAspectFit
        smooth: true
        // Alpha (Qt.alpha() callers) via opacity - not colorization.
        opacity: root.color.a
        // 5WHY: horizontal flip for RTL directional glyphs (see root.mirror).
        // origin.x = half width mirrors around the glyph center, preserving
        // the Image's layout box (anchors.fill) exactly.
        transform: Scale {
            origin.x: parent.width / 2
            xScale: root.mirror ? -1 : 1
        }
    }
}