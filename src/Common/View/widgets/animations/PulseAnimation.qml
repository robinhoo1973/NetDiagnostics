import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── PulseAnimation.qml — Opacity+scale pulse for ~5 diagnostic tests ──
// NetworkCard, CPU, DnsServers, MySQL, Postgres, Redis, MongoDB.
// A subtle "breathing" glow effect — opacity oscillates + gentle scale pulse.
//
// Usage: PulseAnimation { anchors.fill: parent; running: testRunning }

Item {
    id: root
    property bool running: false
    property color glowColor: T.ThemeEngine ? T.ThemeEngine.colors.primary : "#60C8F8"
    property int pulsePeriod: Tokens.tokens.pulsePeriod

    Rectangle {
        id: glow
        anchors.fill: parent
        radius: 10
        color: root.glowColor
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

    onRunningChanged: { if (!running) glow.opacity = 0.0 }
}
