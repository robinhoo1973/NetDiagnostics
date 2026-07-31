// =============================================================================
// AppIcon.qml — SVG icon colorization via ShaderEffect + ShaderEffectSource
// =============================================================================
// INVARIANTS (do not break):
//  1. SVGs MUST carry both fill="none" AND fill-opacity="0" — iOS QtSvg
//     reliably parses "none" keyword, desktop QtSvg needs numeric opacity
//     (QTBUG-4145).  Losing either attribute → invisible or foggy icons.
//  2. ShaderEffectSource captures iconImg into FBO with hideSource:true +
//     visible:false — hideSource hides the Image from the scene; visible:false
//     hides the ShaderEffectSource's own white-texture rendering.  ShaderEffect
//     reads the FBO via the `source` property, unaffected by visibility.
//  3. Use ONLY QtQuick (no QtQuick.Effects) — Qt6QuickEffects not in iOS aqt.
//  4. tint.rgb * tex.a in the fragment shader colorizes only opaque pixels
//     (SVG strokes at alpha=1) while keeping transparent regions clear.
// =============================================================================
import QtQuick
import QtQuick.Window

Item {
    id: root
    property string name: ""
    property color color: "white"
    property int size: 20

    implicitWidth: size; implicitHeight: size
    width: size; height: size
    visible: name !== ""

    // White SVG — rendered as texture source for the ShaderEffectSource FBO.
    // Visible (default) to guarantee GPU texture allocation on Metal.
    Image {
        id: iconImg
        anchors.fill: parent
        source: name ? "qrc:/icons/" + name + ".svg" : ""
        sourceSize.width: size * Screen.devicePixelRatio
        sourceSize.height: size * Screen.devicePixelRatio
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: false
    }

    // hideSource:true  — hides iconImg from the scene, keeps FBO capture
    // visible:false   — hides this item's own white-texture rendering
    // live:false      — SVGs are static; scheduleUpdate() on name/size change
    ShaderEffectSource {
        id: iconSource
        sourceItem: iconImg
        anchors.fill: parent
        hideSource: true
        visible: false
        live: false
    }

    // Inline ShaderEffect — colorizes opaque SVG strokes, keeps bg transparent.
    // tint.rgb * tex.a → stroke pixels (alpha=1) = full tint; bg (alpha=0) = clear.
    ShaderEffect {
        anchors.fill: parent
        property color tint: root.color
        property var source: iconSource
        fragmentShader: "
            varying highp vec2 qt_TexCoord0;
            uniform sampler2D source;
            uniform highp vec4 tint;
            void main() {
                highp vec4 tex = texture2D(source, qt_TexCoord0);
                gl_FragColor = vec4(tint.rgb * tex.a, tint.a * tex.a);
            }
        "
    }

    // Re-capture FBO when icon name or size changes (SVG content is static).
    Component.onCompleted: {
        iconImg.statusChanged.connect(function() {
            if (iconImg.status === Image.Ready) iconSource.scheduleUpdate()
        })
    }
    onNameChanged: iconSource.scheduleUpdate()
    onSizeChanged: iconSource.scheduleUpdate()
}
