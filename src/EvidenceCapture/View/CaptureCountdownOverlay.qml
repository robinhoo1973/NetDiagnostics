// =============================================================================
// CaptureCountdownOverlay.qml — Standalone countdown before capture starts
// =============================================================================
// Modern redesign: Animated pulse ring around the countdown number,
// smooth scale transitions on each tick, progress bar indicator,
// and polished typography.
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as T
import "../widgets"

Rectangle {
    id: root
    anchors.fill: parent
    color: Qt.alpha(T.ThemeEngine.colors.surface, 0.86)
    z: 2100

    // ── Entry animation ─────────────────────────────────────────────
    scale: 0.92; opacity: 0
    Behavior on scale  { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
    Component.onCompleted: { scale = 1.0; opacity = 1.0 }

    signal countdownFinished()
    signal cancelled()

    // ── Countdown state ─────────────────────────────────────────────
    property int countdown: 5
    property bool running: false
    readonly property int totalSeconds: 5

    function start() {
        if (running) return
        countdown = totalSeconds
        running = true
        if (captureOrchestrator) captureOrchestrator.notifyCountdownStarted()
        countdownTimer.restart()
    }
    function stop() {
        running = false
        countdownTimer.stop()
    }

    Timer {
        id: countdownTimer
        interval: 1000
        repeat: true
        onTriggered: {
            root.countdown--
            if (root.countdown <= 0) {
                stop()
                root.countdownFinished()
            }
        }
    }

    MouseArea { anchors.fill: parent } // absorb clicks

    // ── Card ────────────────────────────────────────────────────────
    Rectangle {
        id: card
        anchors.centerIn: parent
        width: Math.min(420, parent.width * 0.9)
        height: Math.min(ctCol.implicitHeight + 48, parent.height * 0.65)
        radius: 28
        color: T.ThemeEngine.colors.card
        border { width: 1; color: Qt.alpha(T.ThemeEngine.colors.borderCard, 0.6) }
        clip: true

        // Top accent — 2px refined
        Rectangle {
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: 2; radius: 2
            color: T.ThemeEngine.cyan
        }

        ColumnLayout {
            id: ctCol
            anchors { fill: parent; margins: 24 }
            spacing: 20

            // ── Timer icon ──────────────────────────────────────────
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 48; implicitHeight: 48; radius: 24
                color: Qt.alpha(T.ThemeEngine.cyan, 0.10)
                AppIcon {
                    anchors.centerIn: parent
                    name: "timer"; size: 24
                    color: T.ThemeEngine.cyan
                }
            }

            // ── Title ───────────────────────────────────────────────
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: T.Tr.captureStartingIn
                font.pixelSize: 14
                font.weight: Font.DemiBold; color: T.ThemeEngine.textPrimary
            }

            // ── Animated countdown number ───────────────────────────
            Item {
                Layout.alignment: Qt.AlignHCenter
                width: 120; height: 120

                // Pulsing ring behind the number
                Rectangle {
                    anchors.centerIn: parent
                    width: 100; height: 100; radius: 50
                    color: "transparent"
                    border { width: 3; color: Qt.alpha(T.ThemeEngine.cyan, 0.15) }
                    // Subtle pulse animation
                    scale: pulseAnim.running ? 1.08 : 1.0
                    opacity: pulseAnim.running ? 0.5 : 0.3
                    Behavior on scale   { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
                    Behavior on opacity { NumberAnimation { duration: 400 } }
                }

                // Decorative accent ring (static, not a progress arc — see progress bar below)
                Rectangle {
                    anchors.centerIn: parent
                    width: 106; height: 106; radius: 53
                    color: "transparent"
                    border {
                        width: 3
                        color: T.ThemeEngine.cyan
                    }
                    // Static accent ring framing the countdown number
                    opacity: 0.6
                }

                // Countdown number
                Label {
                    id: countLabel
                    anchors.centerIn: parent
                    text: root.countdown
                    font.pixelSize: 56
                    font.weight: Font.Bold; color: T.ThemeEngine.cyan
                    // Pulse on each tick
                    scale: 1.0
                    SequentialAnimation {
                        id: pulseAnim
                        running: root.running && root.countdown > 0
                        loops: Animation.Infinite
                        // 5WHY: Use explicit target id (countLabel), not implicit
                        // parent.  If someone wraps this Label in another Item
                        // for visual grouping, implicit parent would silently
                        // break, stopping the pulse with no warning.
                        // 5WHY: 250ms expand + 750ms contract = 1000ms total,
                        // precisely matching the 1-second countdown interval.
                        NumberAnimation { target: countLabel; property: "scale"; from: 1.0; to: 1.18; duration: 250; easing.type: Easing.OutCubic }
                        NumberAnimation { target: countLabel; property: "scale"; from: 1.18; to: 1.0; duration: 750; easing.type: Easing.InOutCubic }
                    }
                }
            }

            // ── Progress bar ────────────────────────────────────────
            // 5WHY: implicitHeight is a general QQuickItem hint that Qt Quick
            // Layouts treat as a last-resort fallback.  On static/cross-compiled
            // ARM Qt builds, the Layout may skip the implicitHeight fallback path
            // for Rectangle, resulting in zero height → invisible progress bar.
            // Layout.preferredHeight is the Layout-specific attached property
            // that Layouts are guaranteed to respect regardless of build config.
            // 5WHY (round-2): The outer track at 12% opacity was invisible on
            // embedded GPUs that clamp low-alpha fragments.  Bump to 20% so the
            // track is visible even on Mali/Adreno embedded renderers.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 4
                radius: 2
                color: Qt.alpha(T.ThemeEngine.textSecondary, 0.20)
                Rectangle {
                    height: parent.height; radius: 2
                    width: parent.width * (root.countdown / root.totalSeconds)
                    color: T.ThemeEngine.cyan
                    Behavior on width { NumberAnimation { duration: 900; easing.type: Easing.OutCubic } }
                }
            }

            // ── Subtitle ────────────────────────────────────────────
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: T.Tr.captureDoNotTouch
                font.pixelSize: 12
                color: T.ThemeEngine.textSecondary
            }

            // ── Cancel button ───────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 48; radius: 14
                color: "transparent"
                border { width: 1.5; color: Qt.alpha(T.ThemeEngine.textSecondary, 0.25) }
                scale: cancelMa.pressed ? 0.97 : 1.0
                Behavior on scale { NumberAnimation { duration: 100 } }
                Label {
                    anchors.centerIn: parent
                    text: T.Tr.captureCancelBtn
                    
                    font.pixelSize: 14; font.weight: Font.DemiBold
                    color: T.ThemeEngine.textSecondary
                }
                MouseArea {
                    id: cancelMa
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: { root.stop(); root.cancelled() }
                }
            }
        }
    }
}
