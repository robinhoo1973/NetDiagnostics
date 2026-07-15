import QtQuick

Image {
    id: icon
    property string name: ""
    property color color: "white"  // for future theme-aware rendering
    property int size: 20

    width: size; height: size
    source: name ? "qrc:/icons/" + name + ".svg" : ""
    sourceSize.width: size * 2
    sourceSize.height: size * 2
    fillMode: Image.PreserveAspectFit
    smooth: true
    visible: name !== ""
    mipmap: true
}
