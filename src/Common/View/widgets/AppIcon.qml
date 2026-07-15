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
    // MultiEffect colorization — applies root.color to the SVG source.
    // SVGs with currentColor render black in Qt SVG, so colorization=1.0
    // fully replaces the source color with the tint while preserving alpha.
    MultiEffect {
        anchors.fill: parent
        source: iconImg
        colorization: 1.0
        colorizationColor: root.color
    }
}
