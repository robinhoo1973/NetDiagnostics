import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── JiggleAnimation.qml — iOS-style icon jiggle for ~20 diagnostic tests ──
// Replaces the old RotationAnimation spinner.  Renders a shimmering border
// ring that rotates ±2.5° + pulses in scale.  Each instance has a random
// phase offset so multiple blocks on screen don't jiggle in sync.
//
// Usage: JiggleAnimation { anchors.fill: parent; running: testRunning }

Item {
    id: root
    property bool running: false
    property real phaseOffset: Math.random() * 200  // ms, random per block
    property color accentColor: T.ThemeEngine ? T.ThemeEngine.colors.primary : "#60C8F8"

    // ── Shimmer ring (visual jiggle) ────────────────────────────────────
    Rectangle {
        id: ring
        anchors.centerIn: parent
        width: Math.min(parent.width, parent.height) * 0.82
        height: width
        radius: width / 2
        color: "transparent"
        border { width: 2; color: root.accentColor }
        opacity: 0.0

        ParallelAnimation {
            running: root.running
            loops: Animation.Infinite

            // Rotate ±2.5° with random phase offset
            SequentialAnimation on rotation {
                PauseAnimation { duration: root.phaseOffset }
                NumberAnimation {
                    from: -2.5; to: 2.5
                    duration: Tokens.tokens.jigglePeriod
                    easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    from: 2.5; to: -2.5
                    duration: Tokens.tokens.jigglePeriod
                    easing.type: Easing.InOutQuad
                }
            }

            // Scale pulse 0.96 ↔ 1.0
            SequentialAnimation on scale {
                PauseAnimation { duration: root.phaseOffset }
                NumberAnimation {
                    from: 1.0; to: 0.96
                    duration: 150; easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    from: 0.96; to: 1.0
                    duration: 150; easing.type: Easing.InOutQuad
                }
            }

            // Opacity shimmer 0.0 ↔ 0.25
            SequentialAnimation on opacity {
                NumberAnimation {
                    from: 0.0; to: 0.22
                    duration: Tokens.tokens.jigglePeriod
                    easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    from: 0.22; to: 0.0
                    duration: Tokens.tokens.jigglePeriod
                    easing.type: Easing.InOutQuad
                }
            }
        }
    }

    onRunningChanged: {
        if (!running) {
            ring.rotation = 0
            ring.scale = 1.0
            ring.opacity = 0.0
        }
    }
}
