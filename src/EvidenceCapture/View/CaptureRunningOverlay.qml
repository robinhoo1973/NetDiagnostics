// =============================================================================
// CaptureRunningOverlay.qml — Progress indicator during capture
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
    color: Qt.alpha(T.ThemeEngine.colors.surface, 0.92)
    z: 2100

    signal cancelled()

    property alias currentStep: stepLabel.text
    property alias stepProgress: stepBar.value
    property alias stepTotal: stepBar.to
    property alias captureCount: countLabel.text
    property alias elapsedText: elapsedLabel.text

    // 5WHY: wireFlickable was missing — ScrollController::setFlickable was
    // never called, so all Scroll steps silently no-opped (m_flickable==nullptr).
    // Captures the Flickable from the current StackView page and passes it to
    // the orchestrator's ScrollController so recording-mode scrolls actually work.
    function wireFlickable(flickable) {
        if (flickable && captureOrchestrator) {
            captureOrchestrator.setScrollFlickable(flickable)
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(400, parent.width * 0.9)
        implicitHeight: runCol.implicitHeight + 40
        // 5WHY: implicitHeight had no upper bound — on landscape devices the
        // dialog could overflow the screen.  Clamp to 92% of parent height.
        height: Math.min(implicitHeight, parent.height * 0.92)
        radius: 20
        color: T.ThemeEngine.colors.card
        border { width: 1; color: T.ThemeEngine.colors.borderCard }
        clip: true

        ColumnLayout {
            id: runCol
            anchors { fill: parent; margins: 24 }
            spacing: 16

            // Header
            RowLayout {
                Rectangle {
                    implicitWidth: 12; implicitHeight: 12; radius: 6
                    color: T.ThemeEngine.failRed
                    // Pulsing red dot animation
                    SequentialAnimation on opacity {
                        running: true; loops: Animation.Infinite
                        NumberAnimation { from: 1.0; to: 0.3; duration: 600 }
                        NumberAnimation { from: 0.3; to: 1.0; duration: 600 }
                    }
                }
                Label {
                    text: "Capturing..."
                    font.family: T.ThemeEngine.monoFont; font.pixelSize: 16
                    font.weight: Font.Bold; color: T.ThemeEngine.textPrimary
                }
            }

            Label {
                Layout.fillWidth: true
                text: "Please do not touch the device. The process is fully automatic."
                font.family: T.ThemeEngine.monoFont; font.pixelSize: 11
                color: T.ThemeEngine.textSecondary; wrapMode: Text.WordWrap
            }

            // 5WHY: Visual separator between header and status
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 1
                color: T.ThemeEngine.colors.borderCard
            }

            // Current step
            ColumnLayout { spacing: 4
                Label {
                    id: stepLabel
                    text: "Initializing..."
                    font.family: T.ThemeEngine.monoFont; font.pixelSize: 14
                    font.weight: Font.DemiBold; color: T.ThemeEngine.cyan
                }
                ProgressBar {
                    id: stepBar
                    Layout.fillWidth: true
                    from: 0; to: 1; value: 0
                    background: Rectangle {
                        implicitHeight: 4; radius: 2
                        color: Qt.alpha(T.ThemeEngine.cyan, 0.15)
                    }
                    contentItem: Item {
                        Rectangle {
                            width: stepBar.visualPosition * parent.width
                            height: parent.height; radius: 2
                            color: T.ThemeEngine.cyan
                        }
                    }
                }
            }

            // Stats
            RowLayout {
                spacing: 24
                ColumnLayout { spacing: 2
                    Label { text: "Screenshots"; font.family: T.ThemeEngine.monoFont; font.pixelSize: 10; color: T.ThemeEngine.textSecondary }
                    Label { id: countLabel; text: "0"; font.family: T.ThemeEngine.monoFont; font.pixelSize: 20; font.weight: Font.Bold; color: T.ThemeEngine.textPrimary }
                }
                ColumnLayout { spacing: 2
                    Label { text: "Elapsed"; font.family: T.ThemeEngine.monoFont; font.pixelSize: 10; color: T.ThemeEngine.textSecondary }
                    Label { id: elapsedLabel; text: "0s"; font.family: T.ThemeEngine.monoFont; font.pixelSize: 20; font.weight: Font.Bold; color: T.ThemeEngine.textPrimary }
                }
            }

            // 5WHY: elapsedText was never populated — the overlay always showed "0s".
            // Poll captureOrchestrator.elapsedSeconds every second for live display.
            Timer {
                id: elapsedTimer
                interval: 1000; repeat: true; running: true
                onTriggered: {
                    if (captureOrchestrator) {
                        elapsedLabel.text = captureOrchestrator.elapsedSeconds + "s"
                    }
                }
            }

            // Cancel button
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 44; radius: 12
                color: "transparent"
                border { width: 1.5; color: Qt.alpha(T.ThemeEngine.failRed, 0.4) }
                Label {
                    anchors.centerIn: parent
                    text: "✕ Cancel Capture"
                    font.family: T.ThemeEngine.monoFont; font.pixelSize: 14
                    color: T.ThemeEngine.failRed
                }
                MouseArea {
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: root.cancelled()
                }
            }
        }
    }
}
