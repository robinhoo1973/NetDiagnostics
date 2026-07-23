import QtQuick
import QtQuick.Effects

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
        visible: false
    }
    // MultiEffect colorization at 1.0 replaces hue+saturation but PRESERVES
    // source luminance.  SVG icons MUST use white strokes (#FFFFFF, 100%
    // luminance) so the colorized result is the full-brightness target color.
    // Icons using currentColor (0% luminance) or #C0C0D0 (75% = pastel) were
    // invisible or washed out on dark backgrounds — fixed in SVG sources.
    MultiEffect {
        anchors.fill: parent
        source: iconImg
        colorization: 1.0
        colorizationColor: root.color
    }
}
