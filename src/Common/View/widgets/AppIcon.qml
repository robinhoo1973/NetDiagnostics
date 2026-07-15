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
        visible: false
    }
    // Shader-based colorization: replaces the source color with root.color
    // while preserving alpha, so SVGs using currentColor render correctly
    // in any theme (dark/light/system).
    ShaderEffect {
        anchors.fill: parent
        property var source: iconImg
        property color tint: root.color
        fragmentShader: "
            varying highp vec2 qt_TexCoord0;
            uniform sampler2D source;
            uniform highp vec4 tint;
            void main() {
                highp vec4 tex = texture2D(source, qt_TexCoord0);
                // Preserve alpha from source; replace RGB with tint color modulated by source luminance.
                float lum = dot(tex.rgb, vec3(0.299, 0.587, 0.114));
                gl_FragColor = vec4(tint.rgb * lum * 2.0, tex.a * tint.a);
            }
        "
    }
}
