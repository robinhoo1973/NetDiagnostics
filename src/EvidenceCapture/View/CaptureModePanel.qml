// =============================================================================
// CaptureModePanel.qml — Capture mode selection panel
// =============================================================================
// Design ref: docs/AutomatedEvidenceCapture_Design.md §2.1
//
// Shown after user double-clicks the Settings app icon.
// User selects: Screenshot Only / Recording Only / Both + diagnostic URL.
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

    property int selectedMode: 2  // default: Both
    property string diagUrl: appState.target || "https://httpbin.org"

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
        radius: 20
        color: T.ThemeEngine.colors.card
        border { width: 1; color: T.ThemeEngine.colors.borderFocused }

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
                text: "Select capture mode and enter a diagnostic URL."
                font.family: T.ThemeEngine.monoFont; font.pixelSize: 12
                color: T.ThemeEngine.textSecondary; wrapMode: Text.WordWrap
            }

            // Mode selector
            ColumnLayout {
                spacing: 8
                Repeater {
                    model: [
                        { icon: "📸", label: "Screenshot Only",     desc: "Capture screenshots of each page", mode: 0 },
                        { icon: "🎥", label: "Recording Only",       desc: "Record video of automated flow",  mode: 1 },
                        { icon: "📸+🎥", label: "Both (Recommended)", desc: "Record video + save screenshots", mode: 2 }
                    ]
                    delegate: Rectangle {
                        Layout.fillWidth: true; implicitHeight: 52; radius: 12
                        color: root.selectedMode === modelData.mode
                               ? Qt.alpha(T.ThemeEngine.cyan, 0.12)
                               : "transparent"
                        border {
                            width: root.selectedMode === modelData.mode ? 1.5 : 1
                            color: root.selectedMode === modelData.mode
                                   ? T.ThemeEngine.cyan
                                   : T.ThemeEngine.colors.borderCard
                        }
                        RowLayout {
                            anchors { fill: parent; margins: 12 }
                            spacing: 12
                            Label {
                                text: modelData.icon
                                font.pixelSize: 22
                            }
                            ColumnLayout {
                                spacing: 2
                                Label {
                                    text: modelData.label
                                    font.family: T.ThemeEngine.monoFont
                                    font.pixelSize: 13; font.weight: Font.DemiBold
                                    color: T.ThemeEngine.textPrimary
                                }
                                Label {
                                    text: modelData.desc
                                    font.family: T.ThemeEngine.monoFont
                                    font.pixelSize: 10
                                    color: T.ThemeEngine.textSecondary
                                }
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.selectedMode = modelData.mode
                        }
                    }
                }
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
                    color: T.ThemeEngine.cyan
                    Label {
                        anchors.centerIn: parent
                        text: "▶ Start Capture"
                        font.family: T.ThemeEngine.monoFont; font.pixelSize: 14
                        font.weight: Font.Bold; color: "#0F172A"
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: root.startRequested(root.selectedMode, root.diagUrl)
                    }
                }
            }
        }
    }
}
