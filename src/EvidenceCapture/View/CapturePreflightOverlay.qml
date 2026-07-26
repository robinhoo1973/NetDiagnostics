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

    // 5WHY: On iOS (needsFocusModeSetup=true), the user must manually enable
    // Focus/DND mode before capture can begin.  The "I'm Ready" button sets
    // this flag; until then, the countdown is hidden and the timer is paused.
    property bool focusConfirmed: false
    // Countdown is ready when either focus setup isn't needed (Android/Desktop)
    // OR the user has confirmed they've enabled it (iOS).
    readonly property bool readyForCountdown: {
        if (!captureOrchestrator || !captureOrchestrator.needsFocusModeSetup) return true
        return focusConfirmed
    }
    onFocusConfirmedChanged: {
        if (focusConfirmed && running) {
            // User tapped "I'm Ready" — begin countdown + notify C++
            if (captureOrchestrator) captureOrchestrator.notifyCountdownStarted()
            countdown = 5
            countdownTimer.restart()
        }
    }

    property int countdown: 5
    property bool running: false

    function start() {
        // 5WHY: Guard against double-call — if the Loader re-incubates the
        // overlay (configuration change, memory pressure), start() may fire
        // twice.  Resetting focusConfirmed on a running instance would hide
        // the countdown on iOS, trapping the user.  Return early if already
        // running so the existing countdown and Focus-confirmation state
        // are preserved.
        if (running) return
        countdown = 5
        focusConfirmed = false
        running = true
        if (readyForCountdown) {
            // 5WHY: Notify C++ that the countdown has actually started so it
            // can register its safety-net timer.  On iOS this fires after
            // focus confirmation; on Android it fires immediately.
            if (captureOrchestrator) captureOrchestrator.notifyCountdownStarted()
            countdownTimer.restart()
        }
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
                    // 5WHY: iOS Focus/DND mode setup requires manual user action.
                    // Show a prominent guide + "I've enabled Focus mode" button
                    // that the user must tap BEFORE the countdown begins.  Without
                    // this, the 3-5s countdown starts while the user is still
                    // reading the hint — they have no time to act.
                    Rectangle {
                        visible: captureOrchestrator ? captureOrchestrator.needsFocusModeSetup : false
                        Layout.fillWidth: true; implicitHeight: focusCol.implicitHeight + 20; radius: 12
                        color: Qt.alpha(T.ThemeEngine.warnYellow, 0.08)
                        border { width: 1; color: Qt.alpha(T.ThemeEngine.warnYellow, 0.25) }
                        ColumnLayout {
                            id: focusCol
                            anchors { fill: parent; margins: 12 }
                            spacing: 8
                            Label {
                                Layout.fillWidth: true; wrapMode: Text.WordWrap
                                text: "⚠️ Focus / Do Not Disturb must be enabled manually on this device.\n\niOS has no automatic Focus mode API for apps.\n\n1. Tap below to open Settings\n2. Navigate to Focus (iOS 18+: tap back then Focus)\n3. Enable any Focus mode (e.g. Do Not Disturb)\n4. Return here and tap 'I'm Ready'"
                                font.family: T.ThemeEngine.monoFont; font.pixelSize: 11
                                color: T.ThemeEngine.textSecondary
                            }
                            RowLayout { spacing: 8
                                Rectangle {
                                    Layout.fillWidth: true; implicitHeight: 36; radius: 8
                                    color: Qt.alpha(T.ThemeEngine.cyan, 0.12)
                                    border { width: 1; color: T.ThemeEngine.cyan }
                                    Label {
                                        anchors.centerIn: parent
                                        text: "Open Settings"
                                        font.family: T.ThemeEngine.monoFont; font.pixelSize: 12; font.weight: Font.Bold
                                        color: T.ThemeEngine.cyan
                                    }
                                    MouseArea {
                                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                        onClicked: { if (captureOrchestrator) captureOrchestrator.openFocusSettings() }
                                    }
                                }
                                Rectangle {
                                    Layout.fillWidth: true; implicitHeight: 36; radius: 8
                                    color: T.ThemeEngine.cyan
                                    Label {
                                        anchors.centerIn: parent
                                        text: "✓ I'm Ready"
                                        font.family: T.ThemeEngine.monoFont; font.pixelSize: 12; font.weight: Font.Bold
                                        color: "#0F172A"
                                    }
                                    MouseArea {
                                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                        onClicked: root.focusConfirmed = true
                                    }
                                }
                            }
                        }
                    }
                    Label { text: "• Screen will stay awake"; font.family: T.ThemeEngine.monoFont; font.pixelSize: 12; color: T.ThemeEngine.textSecondary }
                    Label { text: "• Estimated time: ~45 seconds"; font.family: T.ThemeEngine.monoFont; font.pixelSize: 12; color: T.ThemeEngine.textSecondary }
                }

                // Big countdown number (hidden until focus is confirmed on iOS)
                // 5WHY: Behavior on text with NumberAnimation doesn't work —
                // NumberAnimation operates on numeric properties but text is a
                // string.  QML silently ignores this.  Use opacity transition
                // instead to create a brief fade between countdown values.
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    visible: root.readyForCountdown
                    text: root.countdown
                    font.family: T.ThemeEngine.monoFont; font.pixelSize: 64
                    font.weight: Font.Bold; color: T.ThemeEngine.cyan
                    opacity: 1.0
                    Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.InQuad } }
                }

                // 5WHY: Cancel button was visible only when readyForCountdown=true.
                // On iOS (needsFocusModeSetup=true), readyForCountdown requires
                // focusConfirmed=true — users who changed their mind and wanted
                // to cancel were trapped with no dismiss option.  Always show
                // the Cancel button so the user can back out at any time.
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
