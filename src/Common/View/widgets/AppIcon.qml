// =============================================================================
// AppIcon.qml — 预着色 SVG 图标选择器（归档同款方案，零着色器）
// =============================================================================
import QtQuick
import "IconColors.js" as IconColors

Item {
    id: root
    property string name: ""
    property color color: "white"
    property int size: 20
    property bool mirror: false

    width: size; height: size
    implicitWidth: size; implicitHeight: size
    visible: name !== ""

    property int _hexVersion: 0
    readonly property string _hex: {
        var _v = _hexVersion
        if (!IconColors || !IconColors.hexes || IconColors.hexes.length === 0)
            return "#FFFFFF"
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

    Component.onCompleted: _hexVersion = 1

    transform: Scale {
        origin.x: width / 2
        xScale: mirror ? -1 : 1
    }

    Image {
        anchors.fill: parent
        source: root.name !== "" ? ("qrc:/icons/" + _hex.substr(1).toLowerCase() + "/" + root.name + ".svg") : ""
        // 5WHY (review 2026-08-17): 缺 sourceSize 时 QtSvg 按 SVG 固有 24×24
        // 栅格化再双线性放大到 32-44px——新母版 0.45-0.9 单位的发丝描边在
        // 1.83× 放大后糊成灰带。按显示分辨率栅格化，各尺寸都清晰。
        sourceSize: Qt.size(root.size, root.size)
        fillMode: Image.PreserveAspectFit
        opacity: root.color.a
        cache: true
        asynchronous: true
    }
}
