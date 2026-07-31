// =============================================================================
// AppIcon.qml — SVG icon colorization via ShaderEffect + ShaderEffectSource
// =============================================================================
// INVARIANTS (do not break):
//  1. SVGs MUST carry both fill="none" AND fill-opacity="0" — iOS QtSvg
//     reliably parses "none" keyword, desktop QtSvg needs numeric opacity
//     (QTBUG-4145).  Losing either attribute → invisible or foggy icons.
//  2. ShaderEffectSource captures iconImg into FBO with hideSource:true +
//     visible:false.  ShaderEffect reads FBO via `source` property.
//  3. Use ONLY QtQuick (no QtQuick.Effects) — Qt6QuickEffects not in iOS aqt.
//  4. tint.rgb * tex.a → stroke color; tint.a * tex.a → preserves caller alpha.
//
// 5WHY (2026-07-31 live-race): live:false was a premature optimization that
// broke iOS icon rendering.  ShaderEffectSource with live:false captures FBO
// exactly ONCE at creation time.  On iOS, SVG decoding is asynchronous — the
// Image may not have rendered its first frame when the initial capture occurs,
// producing a blank FBO forever.  The statusChanged→scheduleUpdate() fallback
// has a race: if the SVG decodes before Component.onCompleted connects the
// signal, scheduleUpdate() is never called.  live:true (default) re-captures
// every frame — ~50 small-icon FBO passes cost <1ms total, negligible vs the
// cost of invisible icons.
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

    ShaderEffectSource {
        id: iconSource
        sourceItem: iconImg
        anchors.fill: parent
        hideSource: true
        visible: false
    }

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
}
