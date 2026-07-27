// =============================================================================
// CaptureModePanel.qml — Capture mode selection panel
// =============================================================================
// Design ref: docs/AutomatedEvidenceCapture_Design.md §2.1
//
// Modern redesign (2026-07): Card-based mode toggles with SVG iconography,
// smooth scale-in entry animation, glass-morphism backdrop, Material Design 3
// inspired selection states, and proper button hierarchy.
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as T
import "../widgets"

Rectangle {
    id: root
    anchors.fill: parent
    color: Qt.alpha(T.ThemeEngine.colors.surface, 0.82)
    z: 2000

    // ── Scale-in animation ──────────────────────────────────────────
    scale: 0.92; opacity: 0
    Behavior on scale  { NumberAnimation { duration: 280; easing.type: Easing.OutCubic } }
    Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
    Component.onCompleted: { scale = 1.0; opacity = 1.0 }

    // ── State ───────────────────────────────────────────────────────
    property bool wantsScreenshot: true
    property bool wantsRecording: captureOrchestrator ? captureOrchestrator.supportsBothModes : false
    property string diagUrl: appState.target || "https://httpbin.org"

    readonly property int computedMode: (wantsScreenshot && wantsRecording) ? 2
                                      : wantsScreenshot ? 0
                                      : wantsRecording ? 1
                                      : -1

    signal startRequested(int mode, string url)
    signal cancelled()

    // Backdrop dismiss
    MouseArea {
        anchors.fill: parent
        onClicked: root.cancelled()
    }

    // ── Main card ───────────────────────────────────────────────────
    Rectangle {
        id: card
        anchors.centerIn: parent
        width: Math.min(420, parent.width * 0.92)
        implicitHeight: panelCol.implicitHeight + 48
        height: Math.min(implicitHeight, parent.height * 0.92)
        radius: 24
        color: T.ThemeEngine.colors.card
        border { width: 1; color: Qt.alpha(T.ThemeEngine.colors.borderCard, 0.6) }
        clip: true

        // Subtle top accent line
        Rectangle {
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: 3
            radius: 3
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: T.ThemeEngine.primary }
                GradientStop { position: 1.0; color: T.ThemeEngine.cyan }
            }
        }

        MouseArea { anchors.fill: parent } // absorb clicks

        ColumnLayout {
            id: panelCol
            anchors { fill: parent; margins: 28 }
            spacing: 20

            // ── Header ──────────────────────────────────────────────
            RowLayout {
                spacing: 12
                AppIcon { name: "camera"; size: 26; color: T.ThemeEngine.cyan }
                Label {
                    text: "Capture Mode"
                    font.family: T.ThemeEngine.monoFont
                    font.pixelSize: 20; font.weight: Font.Bold
                    color: T.ThemeEngine.textPrimary
                }
            }

            Label {
                Layout.fillWidth: true
                text: "Choose how you'd like to capture evidence during diagnostics."
                font.family: T.ThemeEngine.monoFont; font.pixelSize: 12
                color: T.ThemeEngine.textSecondary; wrapMode: Text.WordWrap
                lineHeight: 1.4
            }

            // ── Mode selector cards ─────────────────────────────────
            ColumnLayout {
                spacing: 10

                // ── Screenshots card ────────────────────────────────
                Rectangle {
                    id: screenshotCard
                    Layout.fillWidth: true; implicitHeight: 66; radius: 14
                    color: root.wantsScreenshot
                           ? Qt.alpha(T.ThemeEngine.cyan, 0.10)
                           : Qt.alpha(T.ThemeEngine.colors.input, 0.3)
                    border {
                        width: root.wantsScreenshot ? 2 : 1
                        color: root.wantsScreenshot
                               ? T.ThemeEngine.cyan
                               : Qt.alpha(T.ThemeEngine.colors.borderCard, 0.5)
                    }
                    // Smooth color transition
                    Behavior on color  { ColorAnimation { duration: 200 } }
                    Behavior on border.color { ColorAnimation { duration: 200 } }

                    // Subtle glow when selected
                    Rectangle {
                        anchors.fill: parent; radius: 14
                        visible: root.wantsScreenshot
                        color: "transparent"
                        border { width: 3; color: Qt.alpha(T.ThemeEngine.cyan, 0.08) }
                    }

                    RowLayout {
                        anchors { fill: parent; margins: 14 }
                        spacing: 14

                        // Icon container
                        Rectangle {
                            implicitWidth: 40; implicitHeight: 40; radius: 10
                            color: root.wantsScreenshot
                                   ? Qt.alpha(T.ThemeEngine.cyan, 0.18)
                                   : Qt.alpha(T.ThemeEngine.textSecondary, 0.08)
                            Behavior on color { ColorAnimation { duration: 200 } }
                            AppIcon {
                                anchors.centerIn: parent
                                name: "camera"; size: 22
                                color: root.wantsScreenshot ? T.ThemeEngine.cyan : T.ThemeEngine.textSecondary
                            }
                        }

                        ColumnLayout {
                            spacing: 2
                            Label {
                                text: "Screenshots"
                                font.family: T.ThemeEngine.monoFont
                                font.pixelSize: 14; font.weight: Font.DemiBold
                                color: T.ThemeEngine.textPrimary
                            }
                            Label {
                                text: "Capture each diagnostic page as images"
                                font.family: T.ThemeEngine.monoFont
                                font.pixelSize: 11
                                color: T.ThemeEngine.textSecondary
                            }
                        }

                        Item { Layout.fillWidth: true }

                        // Selection indicator
                        Rectangle {
                            implicitWidth: 24; implicitHeight: 24; radius: 12
                            color: root.wantsScreenshot
                                   ? T.ThemeEngine.cyan
                                   : "transparent"
                            border {
                                width: 2
                                color: root.wantsScreenshot
                                       ? T.ThemeEngine.cyan
                                       : Qt.alpha(T.ThemeEngine.textSecondary, 0.3)
                            }
                            Behavior on color        { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                            AppIcon {
                                anchors.centerIn: parent
                                name: "check"; size: 14
                                // 5WHY: #0F172A on cyan background is readable on both light
                                // and dark themes — cyan is mid-luminance, dark text always contrasts.
                                color: root.wantsScreenshot ? "#0F172A" : "transparent"
                                visible: root.wantsScreenshot
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (captureOrchestrator && !captureOrchestrator.supportsBothModes) {
                                root.wantsScreenshot = !root.wantsScreenshot
                                if (root.wantsScreenshot) root.wantsRecording = false
                            } else {
                                root.wantsScreenshot = !root.wantsScreenshot
                            }
                        }
                    }
                }

                // ── Recording card ──────────────────────────────────
                Rectangle {
                    id: recordingCard
                    Layout.fillWidth: true; implicitHeight: 66; radius: 14
                    color: root.wantsRecording
                           ? Qt.alpha(T.ThemeEngine.cyan, 0.10)
                           : Qt.alpha(T.ThemeEngine.colors.input, 0.3)
                    border {
                        width: root.wantsRecording ? 2 : 1
                        color: root.wantsRecording
                               ? T.ThemeEngine.cyan
                               : Qt.alpha(T.ThemeEngine.colors.borderCard, 0.5)
                    }
                    Behavior on color  { ColorAnimation { duration: 200 } }
                    Behavior on border.color { ColorAnimation { duration: 200 } }

                    Rectangle {
                        anchors.fill: parent; radius: 14
                        visible: root.wantsRecording
                        color: "transparent"
                        border { width: 3; color: Qt.alpha(T.ThemeEngine.cyan, 0.08) }
                    }

                    RowLayout {
                        anchors { fill: parent; margins: 14 }
                        spacing: 14

                        Rectangle {
                            implicitWidth: 40; implicitHeight: 40; radius: 10
                            color: root.wantsRecording
                                   ? Qt.alpha(T.ThemeEngine.cyan, 0.18)
                                   : Qt.alpha(T.ThemeEngine.textSecondary, 0.08)
                            Behavior on color { ColorAnimation { duration: 200 } }
                            AppIcon {
                                anchors.centerIn: parent
                                name: "video"; size: 22
                                color: root.wantsRecording ? T.ThemeEngine.cyan : T.ThemeEngine.textSecondary
                            }
                        }

                        ColumnLayout {
                            spacing: 2
                            Label {
                                text: "Screen Recording"
                                font.family: T.ThemeEngine.monoFont
                                font.pixelSize: 14; font.weight: Font.DemiBold
                                color: T.ThemeEngine.textPrimary
                            }
                            Label {
                                text: "Record video of the automated diagnostic flow"
                                font.family: T.ThemeEngine.monoFont
                                font.pixelSize: 11
                                color: T.ThemeEngine.textSecondary
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Rectangle {
                            implicitWidth: 24; implicitHeight: 24; radius: 12
                            color: root.wantsRecording
                                   ? T.ThemeEngine.cyan
                                   : "transparent"
                            border {
                                width: 2
                                color: root.wantsRecording
                                       ? T.ThemeEngine.cyan
                                       : Qt.alpha(T.ThemeEngine.textSecondary, 0.3)
                            }
                            Behavior on color        { ColorAnimation { duration: 200 } }
                            Behavior on border.color { ColorAnimation { duration: 200 } }
                            AppIcon {
                                anchors.centerIn: parent
                                name: "check"; size: 14
                                // 5WHY: Same reasoning as screenshot card — cyan bg + dark check is universal.
                                color: root.wantsRecording ? "#0F172A" : "transparent"
                                visible: root.wantsRecording
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (captureOrchestrator && !captureOrchestrator.supportsBothModes) {
                                root.wantsRecording = !root.wantsRecording
                                if (root.wantsRecording) root.wantsScreenshot = false
                            } else {
                                root.wantsRecording = !root.wantsRecording
                            }
                        }
                    }
                }
            }

            // ── Both-mode badge ─────────────────────────────────────
            Rectangle {
                visible: root.computedMode === 2 && captureOrchestrator && captureOrchestrator.supportsBothModes
                Layout.fillWidth: true; implicitHeight: 32; radius: 8
                color: Qt.alpha(T.ThemeEngine.cyan, 0.08)
                border { width: 1; color: Qt.alpha(T.ThemeEngine.cyan, 0.2) }
                RowLayout {
                    anchors.centerIn: parent
                    spacing: 6
                    AppIcon { name: "badge-info"; size: 14; color: T.ThemeEngine.cyan }
                    Label {
                        text: "Both modes — complete evidence capture"
                        font.family: T.ThemeEngine.monoFont; font.pixelSize: 11
                        color: T.ThemeEngine.cyan
                    }
                }
            }

            // ── Divider ─────────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 1
                color: Qt.alpha(T.ThemeEngine.colors.borderCard, 0.4)
            }

            // ── URL input ───────────────────────────────────────────
            ColumnLayout {
                spacing: 6
                RowLayout {
                    spacing: 6
                    AppIcon { name: "globe"; size: 14; color: T.ThemeEngine.textSecondary }
                    Label {
                        text: "Diagnostic URL"
                        font.family: T.ThemeEngine.monoFont
                        font.pixelSize: 12; font.weight: Font.DemiBold
                        color: T.ThemeEngine.textSecondary
                    }
                }
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 44; radius: 10
                    color: T.ThemeEngine.bgInput
                    border {
                        width: 1
                        color: urlInput.activeFocus
                               ? T.ThemeEngine.cyan
                               : Qt.alpha(T.ThemeEngine.colors.borderCard, 0.5)
                    }
                    Behavior on border.color { ColorAnimation { duration: 200 } }
                    RowLayout {
                        anchors { fill: parent; margins: 12 }
                        spacing: 8
                        AppIcon { name: "target"; size: 16; color: Qt.alpha(T.ThemeEngine.textSecondary, 0.5) }
                        TextInput {
                            id: urlInput
                            Layout.fillWidth: true
                            text: root.diagUrl
                            font.family: T.ThemeEngine.monoFont; font.pixelSize: 13
                            color: T.ThemeEngine.textPrimary
                            clip: true
                            selectByMouse: true
                            onTextChanged: root.diagUrl = text
                        }
                    }
                }
            }

            // ── Action buttons ──────────────────────────────────────
            RowLayout {
                spacing: 12
                // Cancel button
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 48; radius: 14
                    color: "transparent"
                    border { width: 1.5; color: Qt.alpha(T.ThemeEngine.textSecondary, 0.25) }
                    // Hover feedback via scale
                    scale: cancelMa.pressed ? 0.97 : 1.0
                    Behavior on scale { NumberAnimation { duration: 100 } }
                    Label {
                        anchors.centerIn: parent
                        text: "Cancel"
                        font.family: T.ThemeEngine.monoFont
                        font.pixelSize: 14; font.weight: Font.DemiBold
                        color: T.ThemeEngine.textSecondary
                    }
                    MouseArea {
                        id: cancelMa
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: root.cancelled()
                    }
                }
                // Start button
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 48; radius: 14
                    color: root.computedMode >= 0
                           ? T.ThemeEngine.cyan
                           : Qt.alpha(T.ThemeEngine.textSecondary, 0.15)
                    scale: startMa.pressed && root.computedMode >= 0 ? 0.97 : 1.0
                    Behavior on scale  { NumberAnimation { duration: 100 } }
                    Behavior on color  { ColorAnimation { duration: 200 } }
                    // Gradient accent for enabled state
                    gradient: root.computedMode >= 0 ? Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: T.ThemeEngine.primary }
                        GradientStop { position: 1.0; color: T.ThemeEngine.cyan }
                    } : null
                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 8
                        AppIcon {
                            name: "play"; size: 16
                            // 5WHY: Dark icon/text on gradient cyan/primary button
                            // is readable on both light & dark themes.
                            color: root.computedMode >= 0 ? "#0F172A" : T.ThemeEngine.textSecondary
                        }
                        Label {
                            text: "Start Capture"
                            font.family: T.ThemeEngine.monoFont
                            font.pixelSize: 15; font.weight: Font.Bold
                            color: root.computedMode >= 0 ? "#0F172A" : T.ThemeEngine.textSecondary
                        }
                    }
                    MouseArea {
                        id: startMa
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        enabled: root.computedMode >= 0
                        onClicked: root.startRequested(root.computedMode, root.diagUrl)
                    }
                }
            }
        }
    }
}
