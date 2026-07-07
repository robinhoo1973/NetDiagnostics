import QtQuick

// AppIcon — SVG icon with ShaderEffect tinting.
// Works across all Qt 6 builds (static + dynamic) because ShaderEffect
// is part of QtQuick base, not QtQuick.Controls.
//
// Usage:
//   AppIcon { name: "settings"; color: ThemeEngine.colors.textPrimary }  // tinted
//   AppIcon { name: "badge-check" }  // no tint — SVG native colors show through
Item {
    id: icon
    property string name: ""
    property color color: "transparent"
    property int size: 20

    width: size; height: size

    Image {
        id: src
        anchors.fill: parent
        source: name ? "qrc:/icons/" + name + ".svg" : ""
        sourceSize.width: icon.size * 2
        sourceSize.height: icon.size * 2
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        visible: false
    }

    // Tinted layer — only active when color is non-transparent
    ShaderEffect {
        anchors.fill: parent
        visible: icon.color !== "transparent" && icon.color.a > 0
        property variant texSrc: src
        property color tint: icon.color
        property real alpha: icon.color.a

        fragmentShader: "varying highp vec2 qt_TexCoord0;
            uniform sampler2D texSrc;
            uniform lowp vec4 tint;
            uniform lowp float alpha;
            uniform lowp float qt_Opacity;
            void main() {
                lowp vec4 tex = texture2D(texSrc, qt_TexCoord0);
                gl_FragColor = tint * tex.a * qt_Opacity * alpha;
            }"
    }

    // Fallthrough — renders native SVG colors when color is transparent
    Image {
        anchors.fill: parent
        visible: icon.color === "transparent" || icon.color.a === 0
        source: name ? "qrc:/icons/" + name + ".svg" : ""
        sourceSize.width: icon.size * 2
        sourceSize.height: icon.size * 2
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
    }
}