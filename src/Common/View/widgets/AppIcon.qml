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

    Image {
        id: iconImg
        anchors.fill: parent
        source: name ? "qrc:/icons/" + name + ".svg" : ""
        sourceSize.width: size * 2
        sourceSize.height: size * 2
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
    }
    // Universal colorization fallback — semi-transparent color overlay.
    // Works without QtQuick.Effects (MultiEffect/ColorOverlay), making it
    // reliable on iOS static builds where the Effects module is unavailable.
    Rectangle {
        anchors.fill: parent
        color: root.color
        opacity: 0.55
    }
}
