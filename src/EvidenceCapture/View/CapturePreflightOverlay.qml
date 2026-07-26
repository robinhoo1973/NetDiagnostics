// =============================================================================
// CapturePreflightOverlay.qml — Focus/DND guide before capture starts
// =============================================================================
// Design: Separated from the countdown.  This overlay only handles Focus/DND
// setup.  When the user confirms readiness, it emits requestCountdown() and
// the AppContent Loader swaps to CaptureCountdownOverlay.
//
// On platforms where Focus setup is not needed (Android with programmatic DND),
// AppContent loads CaptureCountdownOverlay directly, skipping this overlay.
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

    signal requestCountdown()
    signal cancelled()

    // 5WHY: On iOS (needsFocusModeSetup=true), the user must manually enable
    // Focus/DND mode before capture.  The "I'm Ready" button confirms this
    // and requests the transition to the countdown overlay.
    // On Android (needsFocusModeSetup=false), this overlay is skipped entirely
    // by AppContent — the countdown loads directly.

    MouseArea { anchors.fill: parent } // absorb clicks — don't dismiss

    // ── Card ────────────────────────────────────────────────────────
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
            contentWidth: width
            contentHeight: preCol.implicitHeight + 48
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ColumnLayout {
                id: preCol
                width: cardFlick.width
                spacing: 16
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

                    // ── DND / Focus mode guide ──────────────────────────────
                    Rectangle {
                        Layout.fillWidth: true; implicitHeight: focusCol.implicitHeight + 20; radius: 12
                        color: Qt.alpha(T.ThemeEngine.warnYellow, 0.08)
                        border { width: 1; color: Qt.alpha(T.ThemeEngine.warnYellow, 0.25) }
                        ColumnLayout {
                            id: focusCol
                            anchors { fill: parent; margins: 12 }
                            spacing: 8
                            Label {
                                Layout.fillWidth: true; wrapMode: Text.WordWrap
                                text: "⚠️ Focus / Do Not Disturb must be enabled manually.\n\nThis device requires a one-time permission or manual\nsetup before notifications can be suppressed.\n\n1. Tap below to open Settings\n2. Enable Focus/DND (or grant notification access)\n   (iOS 18+: if app Settings opens, tap back then Focus)\n3. Return here and tap 'I'm Ready'"
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
                                        onClicked: root.requestCountdown()
                                    }
                                }
                            }
                        }
                    }

                    Label { text: "• Screen will stay awake"; font.family: T.ThemeEngine.monoFont; font.pixelSize: 12; color: T.ThemeEngine.textSecondary }
                    Label { text: "• Estimated time: ~45 seconds"; font.family: T.ThemeEngine.monoFont; font.pixelSize: 12; color: T.ThemeEngine.textSecondary }
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
                        onClicked: root.cancelled()
                    }
                }
            }
        }
    }
}
