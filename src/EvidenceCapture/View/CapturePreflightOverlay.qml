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

    property int countdown: (captureOrchestrator && captureOrchestrator.needsFocusModeSetup) ? 5 : 3
    property bool running: false

    function start() {
        // 5WHY: When Focus/DND mode needs manual setup (iOS), extend the
        // countdown from 3s to 5s to give the user time to read the DND hint
        // and tap to open Settings.  The 3s countdown was too short for the
        // user to notice and act on the hint before capture began.
        countdown = (captureOrchestrator && captureOrchestrator.needsFocusModeSetup) ? 5 : 3
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

    // 5WHY: The outer card uses anchors.centerIn but previously had no height
    // clamp.  On landscape iPhones (~320px logical height) the dialog content
    // (~430px) overflowed the screen, clipping the title, instructions, and
    // Cancel button.  Clamp height to 92% of parent and wrap content in a
    // Flickable so the user can scroll on narrow screens.
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(380, parent.width * 0.88)
        height: Math.min(preCol.implicitHeight + 48, parent.height * 0.92)
        radius: 20
        color: T.ThemeEngine.colors.card
        border { width: 1; color: T.ThemeEngine.colors.borderCard }
        clip: true

        Flickable {
            id: cardFlick
            anchors.fill: parent
            anchors.margins: 0
            contentWidth: width
            contentHeight: preCol.implicitHeight + 48
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ColumnLayout {
                id: preCol
                width: cardFlick.width
                spacing: 16
                // Replicate the original 28px margins inside the Flickable
                anchors { left: parent.left; leftMargin: 28; right: parent.right; rightMargin: 28; top: parent.top; topMargin: 28 }

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
}
