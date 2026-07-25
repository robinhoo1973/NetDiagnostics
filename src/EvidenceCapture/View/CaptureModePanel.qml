// =============================================================================
// CaptureModePanel.qml — Capture mode selection panel
// =============================================================================
// Design ref: docs/AutomatedEvidenceCapture_Design.md §2.1
//
// Shown after user double-clicks the Settings app icon.
// User selects: Screenshots + Recording (independent checkboxes) + diagnostic URL.
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as T

Rectangle {
    id: root
    anchors.fill: parent
    color: Qt.alpha(T.ThemeEngine.colors.surface, 0.85)
    z: 2000

    // 5WHY: Was a single selectedMode int with 3 radio-button options
    // (ScreenshotOnly, RecordingOnly, Both).  "Both" was the union of the
    // other two — semantically redundant and confusing.  Use independent
    // checkboxes: the user can freely combine Screenshots + Recording.
    property bool wantsScreenshot: true
    property bool wantsRecording: true
    property string diagUrl: appState.target || "https://httpbin.org"

    // Compute the capture mode int from checkbox state.
    // 0=ScreenshotOnly, 1=RecordingOnly, 2=Both
    readonly property int computedMode: (wantsScreenshot && wantsRecording) ? 2
                                      : wantsScreenshot ? 0
                                      : wantsRecording ? 1
                                      : -1  // nothing selected — Start button disabled

    signal startRequested(int mode, string url)
    signal cancelled()

    // Backdrop dismiss
    MouseArea {
        anchors.fill: parent
        onClicked: root.cancelled()
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(400, parent.width * 0.9)
        implicitHeight: panelCol.implicitHeight + 48
        // 5WHY: No max-height constraint — on landscape the dialog can overflow.
        height: Math.min(implicitHeight, parent.height * 0.92)
        radius: 20
        color: T.ThemeEngine.colors.card
        border { width: 1; color: T.ThemeEngine.colors.borderFocused }
        clip: true

        // Absorb clicks inside the card
        MouseArea { anchors.fill: parent }

        ColumnLayout {
            id: panelCol
            anchors { fill: parent; margins: 24 }
            spacing: 16

            // Title
            Label {
                text: "🎬 Capture Mode"
                font.family: T.ThemeEngine.monoFont
                font.pixelSize: 18; font.weight: Font.Bold
                color: T.ThemeEngine.textPrimary
            }

            Label {
                Layout.fillWidth: true
                text: "Select capture options and enter a diagnostic URL."
                font.family: T.ThemeEngine.monoFont; font.pixelSize: 12
                color: T.ThemeEngine.textSecondary; wrapMode: Text.WordWrap
            }

            // Mode selector — independent checkboxes
            ColumnLayout {
                spacing: 8
                // Screenshots checkbox
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 52; radius: 12
                    color: root.wantsScreenshot
                           ? Qt.alpha(T.ThemeEngine.cyan, 0.12)
                           : "transparent"
                    border {
                        width: root.wantsScreenshot ? 1.5 : 1
                        color: root.wantsScreenshot
                               ? T.ThemeEngine.cyan
                               : T.ThemeEngine.colors.borderCard
                    }
                    RowLayout {
                        anchors { fill: parent; margins: 12 }
                        spacing: 12
                        // Checkbox indicator
                        Rectangle {
                            implicitWidth: 22; implicitHeight: 22; radius: 4
                            color: root.wantsScreenshot
                                   ? T.ThemeEngine.cyan
                                   : Qt.alpha(T.ThemeEngine.textSecondary, 0.15)
                            border { width: 1.5; color: root.wantsScreenshot ? T.ThemeEngine.cyan : T.ThemeEngine.colors.borderCard }
                            Label {
                                anchors.centerIn: parent
                                text: "✓"
                                visible: root.wantsScreenshot
                                font.pixelSize: 14; font.weight: Font.Bold
                                color: "#0F172A"
                            }
                        }
                        Label {
                            text: "📸"
                            font.pixelSize: 22
                        }
                        ColumnLayout {
                            spacing: 2
                            Label {
                                text: "Screenshots"
                                font.family: T.ThemeEngine.monoFont
                                font.pixelSize: 13; font.weight: Font.DemiBold
                                color: T.ThemeEngine.textPrimary
                            }
                            Label {
                                text: "Capture screenshots of each diagnostic page"
                                font.family: T.ThemeEngine.monoFont
                                font.pixelSize: 10
                                color: T.ThemeEngine.textSecondary
                            }
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.wantsScreenshot = !root.wantsScreenshot
                    }
                }

                // Recording checkbox
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 52; radius: 12
                    color: root.wantsRecording
                           ? Qt.alpha(T.ThemeEngine.cyan, 0.12)
                           : "transparent"
                    border {
                        width: root.wantsRecording ? 1.5 : 1
                        color: root.wantsRecording
                               ? T.ThemeEngine.cyan
                               : T.ThemeEngine.colors.borderCard
                    }
                    RowLayout {
                        anchors { fill: parent; margins: 12 }
                        spacing: 12
                        // Checkbox indicator
                        Rectangle {
                            implicitWidth: 22; implicitHeight: 22; radius: 4
                            color: root.wantsRecording
                                   ? T.ThemeEngine.cyan
                                   : Qt.alpha(T.ThemeEngine.textSecondary, 0.15)
                            border { width: 1.5; color: root.wantsRecording ? T.ThemeEngine.cyan : T.ThemeEngine.colors.borderCard }
                            Label {
                                anchors.centerIn: parent
                                text: "✓"
                                visible: root.wantsRecording
                                font.pixelSize: 14; font.weight: Font.Bold
                                color: "#0F172A"
                            }
                        }
                        Label {
                            text: "🎥"
                            font.pixelSize: 22
                        }
                        ColumnLayout {
                            spacing: 2
                            Label {
                                text: "Screen Recording"
                                font.family: T.ThemeEngine.monoFont
                                font.pixelSize: 13; font.weight: Font.DemiBold
                                color: T.ThemeEngine.textPrimary
                            }
                            Label {
                                text: "Record video of the automated diagnostic flow"
                                font.family: T.ThemeEngine.monoFont
                                font.pixelSize: 10
                                color: T.ThemeEngine.textSecondary
                            }
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.wantsRecording = !root.wantsRecording
                    }
                }
            }

            // Both-selected indicator
            Label {
                visible: root.computedMode === 2
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                text: "📸+🎥  Both modes enabled — recommended for complete evidence"
                font.family: T.ThemeEngine.monoFont; font.pixelSize: 10
                color: T.ThemeEngine.cyan
            }

            // URL input
            ColumnLayout {
                spacing: 4
                Label {
                    text: "Diagnostic URL:"
                    font.family: T.ThemeEngine.monoFont; font.pixelSize: 12
                    color: T.ThemeEngine.textSecondary
                }
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 42; radius: 8
                    color: T.ThemeEngine.bgInput
                    border { width: 1; color: T.ThemeEngine.colors.borderCard }
                    TextInput {
                        id: urlInput
                        anchors { fill: parent; margins: 10 }
                        text: root.diagUrl
                        font.family: T.ThemeEngine.monoFont; font.pixelSize: 13
                        color: T.ThemeEngine.textPrimary
                        clip: true
                        onTextChanged: root.diagUrl = text
                    }
                }
            }

            // Action buttons
            RowLayout {
                spacing: 12
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 44; radius: 12
                    color: "transparent"
                    border { width: 1.5; color: Qt.alpha(T.ThemeEngine.textSecondary, 0.3) }
                    Label {
                        anchors.centerIn: parent
                        text: "Cancel"
                        font.family: T.ThemeEngine.monoFont; font.pixelSize: 14
                        color: T.ThemeEngine.textSecondary
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: root.cancelled()
                    }
                }
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 44; radius: 12
                    // 5WHY: Disable the Start button visually when nothing is
                    // selected (computedMode == -1).  Previously the user could
                    // tap Start with no mode selected, producing a meaningless
                    // session with zero screenshots and no recording.
                    color: root.computedMode >= 0
                           ? T.ThemeEngine.cyan
                           : Qt.alpha(T.ThemeEngine.textSecondary, 0.2)
                    Label {
                        anchors.centerIn: parent
                        text: "▶ Start Capture"
                        font.family: T.ThemeEngine.monoFont; font.pixelSize: 14
                        font.weight: Font.Bold
                        color: root.computedMode >= 0 ? "#0F172A" : T.ThemeEngine.textSecondary
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        enabled: root.computedMode >= 0
                        onClicked: root.startRequested(root.computedMode, root.diagUrl)
                    }
                }
            }
        }
    }
}
