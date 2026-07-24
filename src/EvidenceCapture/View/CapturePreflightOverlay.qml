// =============================================================================
// CapturePreflightOverlay.qml — Countdown + warning before capture starts
// =============================================================================
// Design ref: docs/AutomatedEvidenceCapture_Design.md §2.1
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

    property int countdown: 3
    property bool running: false

    function start() {
        countdown = 3
        running = true
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

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(380, parent.width * 0.88)
        implicitHeight: preCol.implicitHeight + 48
        radius: 20
        color: T.ThemeEngine.colors.card
        border { width: 1; color: T.ThemeEngine.colors.borderCard }

        ColumnLayout {
            id: preCol
            anchors { fill: parent; margins: 28 }
            spacing: 16

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 80; implicitHeight: 80; radius: 40
                color: Qt.alpha(T.ThemeEngine.warnYellow, 0.10)
                Label {
                    anchors.centerIn: parent
                    text: "⚠️"
                    font.pixelSize: 40
                }
            }

            Label {
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                text: "Prepare to Capture"
                font.family: T.ThemeEngine.monoFont; font.pixelSize: 18
                font.weight: Font.Bold; color: T.ThemeEngine.textPrimary
            }

            ColumnLayout {
                spacing: 6
                Layout.fillWidth: true
                Label { text: "• Please do not touch the device"; font.family: T.ThemeEngine.monoFont; font.pixelSize: 12; color: T.ThemeEngine.textSecondary }
                Label { text: "• Enable Do Not Disturb mode"; font.family: T.ThemeEngine.monoFont; font.pixelSize: 12; color: T.ThemeEngine.textSecondary }
                Label { text: "• Screen will stay awake"; font.family: T.ThemeEngine.monoFont; font.pixelSize: 12; color: T.ThemeEngine.textSecondary }
                Label { text: "• Estimated time: ~45 seconds"; font.family: T.ThemeEngine.monoFont; font.pixelSize: 12; color: T.ThemeEngine.textSecondary }
            }

            // Big countdown number
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: root.countdown
                font.family: T.ThemeEngine.monoFont; font.pixelSize: 64
                font.weight: Font.Bold; color: T.ThemeEngine.cyan
                Behavior on text { NumberAnimation { duration: 200 } }
            }

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
