import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── PulseAnimation.qml — Opacity+scale pulse for ~5 diagnostic tests ──
// NetworkCard, CPU, DnsServers, MySQL, Postgres, Redis, MongoDB.
// A subtle "breathing" glow effect — opacity oscillates + gentle scale pulse.
//
// Usage: PulseAnimation { anchors.fill: parent; running: testRunning }

AnimationBase {
    id: root
    // accentColor 名统一（曾 glowColor——AnimationBase 单一属性名）
    property int pulsePeriod: Tokens.tokens.pulsePeriod

    Rectangle {
        id: glow
        anchors.fill: parent
        radius: 10
        color: root.accentColor
        opacity: 0.0

        SequentialAnimation on opacity {
            running: root.running
            loops: Animation.Infinite
            NumberAnimation {
                from: 0.0; to: 0.12
                duration: root.pulsePeriod / 2
                easing.type: Easing.InOutSine
            }
            NumberAnimation {
                from: 0.12; to: 0.0
                duration: root.pulsePeriod / 2
                easing.type: Easing.InOutSine
            }
        }
    }

    function resetVisuals() { glow.opacity = 0.0 }
}
