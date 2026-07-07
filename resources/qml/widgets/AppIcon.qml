import QtQuick
import QtQuick.Controls

// IconImage.color applies multiplicative tint to SVG white strokes.
// White (default) = identity — SVG native colors pass through unchanged.
// Any other color tints white SVG areas to that color.
IconImage {
    id: icon
    property string name: ""
    property int size: 20

    width: size; height: size
    color: "white"   // IconImage built-in — actually tints
    source: name ? "qrc:/icons/" + name + ".svg" : ""
    smooth: true
    visible: name !== ""
}