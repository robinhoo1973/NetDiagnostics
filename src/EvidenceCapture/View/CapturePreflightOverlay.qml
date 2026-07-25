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
                // 5WHY: iOS has no programmatic Focus mode API.  Make the
                // DND hint a clickable link that opens the system settings
                // page where the user can manually enable it.
                // 5WHY (round-28): The hint was always shown regardless of
                // whether DND was already active.  On Android where
                // platformEnableFocusMode() can succeed, showing the hint when
                // DND is ON is misleading.  Use needsFocusModeSetup from the
                // orchestrator — iOS always true, Android conditional.
                Label {
                    visible: captureOrchestrator ? captureOrchestrator.needsFocusModeSetup : false
                    text: "• Enable Do Not Disturb mode  (tap to open Settings)"
                    font.family: T.ThemeEngine.monoFont; font.pixelSize: 12
                    color: T.ThemeEngine.cyan
                    font.underline: true
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (captureOrchestrator) {
                                captureOrchestrator.openFocusSettings()
                            }
                        }
                    }
                }
                Label { text: "• Screen will stay awake"; font.family: T.ThemeEngine.monoFont; font.pixelSize: 12; color: T.ThemeEngine.textSecondary }
                Label { text: "• Estimated time: ~45 seconds"; font.family: T.ThemeEngine.monoFont; font.pixelSize: 12; color: T.ThemeEngine.textSecondary }
            }

            // Big countdown number
            // 5WHY: Behavior on text with NumberAnimation doesn't work —
            // NumberAnimation operates on numeric properties but text is a
            // string.  QML silently ignores this.  Use opacity transition
            // instead to create a brief fade between countdown values.
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: root.countdown
                font.family: T.ThemeEngine.monoFont; font.pixelSize: 64
                font.weight: Font.Bold; color: T.ThemeEngine.cyan
                opacity: 1.0
                Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.InQuad } }
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
