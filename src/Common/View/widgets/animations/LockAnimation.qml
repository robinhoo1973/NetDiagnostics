import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── LockAnimation.qml — Stamp/lock drop-and-settle for ~2 diagnostic tests ──
// SSL Certificate, Security Headers.
// (DNS Integrity → CheckAnimation、Internet Connectivity → MeterAnimation)
// A checkmark/shield drops from above and settles with a bounce.
//
// Usage: LockAnimation { anchors.fill: parent; running: testRunning }

AnimationBase {
    id: root
    // Lock 成功语义：accent 覆盖为 success 令牌
    property color accentColor: T.ThemeEngine.colors.success
    property int dropDuration: Tokens.tokens.lockDropDuration

    // ── Lock indicator ──────────────────────────────────────────────────
    Rectangle {
        id: stamp
        width: 16; height: 16; radius: 3
        color: root.accentColor
        opacity: 0
        anchors.horizontalCenter: parent.horizontalCenter

        SequentialAnimation {
            running: root.running
            loops: Animation.Infinite

            // Start above
            PropertyAction { target: stamp; property: "y"; value: -10 }
            PropertyAction { target: stamp; property: "opacity"; value: 0 }

            // Drop down
            ParallelAnimation {
                NumberAnimation {
                    target: stamp; property: "y"
                    from: -10; to: parent.height / 2 - 8
                    duration: root.dropDuration
                    easing.type: Easing.OutBounce
                }
                NumberAnimation {
                    target: stamp; property: "opacity"
                    from: 0; to: 0.8
                    duration: root.dropDuration / 2
                }
            }

            // Hold
            PauseAnimation { duration: 600 }

            // Fade out
            NumberAnimation {
                target: stamp; property: "opacity"
                from: 0.8; to: 0; duration: 300
            }
        }
    }

    // ── Circular ripple ──────────────────────────────────────────────────
    Rectangle {
        id: ripple
        width: 30; height: 30; radius: 15
        color: "transparent"
        border { width: 2; color: root.accentColor }
        opacity: 0
        anchors.centerIn: parent

        SequentialAnimation {
            running: root.running
            loops: Animation.Infinite

            PauseAnimation { duration: root.dropDuration + 100 }
            PropertyAction { target: ripple; property: "opacity"; value: 0.5 }
            PropertyAction { target: ripple; property: "scale"; value: 0.5 }

            ParallelAnimation {
                NumberAnimation {
                    target: ripple; property: "opacity"
                    from: 0.5; to: 0; duration: 400
                }
                NumberAnimation {
                    target: ripple; property: "scale"
                    from: 0.5; to: 1.5; duration: 400
                }
            }
        }
    }

    function resetVisuals() { stamp.opacity = 0; ripple.opacity = 0; ripple.scale = 0.5 }
}
