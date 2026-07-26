// =============================================================================
// CaptureCountdownOverlay.qml — Standalone countdown before capture starts
// =============================================================================
// Design: Separated from CapturePreflightOverlay so Android (no DND guide)
// can use this directly, and iOS can transition here after Focus confirmation.
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as T

Rectangle {
    id: root
    anchors.fill: parent
    color: Qt.alpha(T.ThemeEngine.colors.surface, 0.88)
    z: 2100

    signal countdownFinished()
    signal cancelled()

    // ── Countdown state ─────────────────────────────────────────────
    property int countdown: 5
    property bool running: false

    function start() {
        if (running) return
        countdown = 5
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

    MouseArea { anchors.fill: parent } // absorb clicks — don't dismiss

    // ── Card ────────────────────────────────────────────────────────
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(320, parent.width * 0.80)
        height: Math.min(ctCol.implicitHeight + 48, parent.height * 0.60)
        radius: 20
        color: T.ThemeEngine.colors.card
        border { width: 1; color: T.ThemeEngine.colors.borderCard }
        clip: true

        ColumnLayout {
            id: ctCol
            anchors { fill: parent; margins: 32 }
            spacing: 20

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: "Starting capture in"
                font.family: T.ThemeEngine.monoFont; font.pixelSize: 16
                font.weight: Font.Bold; color: T.ThemeEngine.textPrimary
            }

            // Big countdown number
            // 5WHY: Behavior on opacity with constant opacity=1.0 is dead code —
            // the property never changes, so the Behavior never triggers.
            // The countdown text update (root.countdown → Label.text) is a string
            // binding that re-evaluates instantly each second — sufficient UX.
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: root.countdown
                font.family: T.ThemeEngine.monoFont; font.pixelSize: 64
                font.weight: Font.Bold; color: T.ThemeEngine.cyan
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: "Please do not touch the device"
                font.family: T.ThemeEngine.monoFont; font.pixelSize: 12
                color: T.ThemeEngine.textSecondary
            }

            // Cancel button
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 44; radius: 12
                color: "transparent"
                border { width: 1.5; color: Qt.alpha(T.ThemeEngine.textSecondary, 0.3) }
                Label {
                    anchors.centerIn: parent
                    text: "Cancel Capture"
                    font.family: T.ThemeEngine.monoFont; font.pixelSize: 14
                    color: T.ThemeEngine.textSecondary
                }
                MouseArea {
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: { root.stop(); root.cancelled() }
                }
            }
        }
    }
}
