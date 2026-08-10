import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── BounceAnimation.qml — Ping-pong bounce for ~6 diagnostic tests ──
// Ping, DhcpStatus, GeoIP, Mqtt.  A small dot bounces horizontally between
// two endpoint circles.  Represents round-trip communication.
//
// Usage: BounceAnimation { anchors.fill: parent; running: testRunning }

Item {
    id: root
    property bool running: false
    property color accentColor: T.ThemeEngine ? T.ThemeEngine.colors.primary : "#60C8F8"

    // ── Left endpoint ────────────────────────────────────────────────────
    Rectangle {
        id: leftEnd
        anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 8 }
        width: 8; height: 8; radius: 4
        color: root.accentColor
        opacity: root.running ? 0.8 : 0.3
    }

    // ── Right endpoint ───────────────────────────────────────────────────
    Rectangle {
        id: rightEnd
        anchors { right: parent.right; verticalCenter: parent.verticalCenter; rightMargin: 8 }
        width: 8; height: 8; radius: 4
        color: root.accentColor
        opacity: root.running ? 0.8 : 0.3
    }

    // ── Bouncing dot ─────────────────────────────────────────────────────
    Rectangle {
        id: ball
        width: 10; height: 10; radius: 5
        y: parent.height / 2 - 5
        color: root.accentColor

        SequentialAnimation on x {
            id: bounce
            running: root.running
            loops: Animation.Infinite
            NumberAnimation {
                from: leftEnd.x + 10; to: rightEnd.x - 10
                duration: Tokens.tokens.bouncePeriod
                easing.type: Easing.InOutQuad
            }
            PauseAnimation { duration: 80 }  // brief pause at right
            NumberAnimation {
                from: rightEnd.x - 10; to: leftEnd.x + 10
                duration: Tokens.tokens.bouncePeriod
                easing.type: Easing.InOutQuad
            }
            PauseAnimation { duration: 80 }  // brief pause at left
        }
    }

    onRunningChanged: { if (!running) ball.x = leftEnd.x + 10 }
}
