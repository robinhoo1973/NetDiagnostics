// =============================================================================
// CaptureResultSummary.qml — Completion summary after capture finishes
// =============================================================================
// Shows success or failure result.  In failure mode, the capture count,
// recording path, and elapsed time are omitted; instead the error code
// and user-facing message are displayed so the user knows why it failed.
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as T

Rectangle {
    id: root
    anchors.fill: parent
    color: Qt.alpha(T.ThemeEngine.colors.surface, 0.85)
    z: 2100

    property string sessionPath: ""
    property int totalScreenshots: 0
    property string recordingFile: ""
    property string elapsedTime: ""
    // 5WHY: Added error-state properties so the overlay can be reused for
    // captureFailed signals instead of silently hiding with no feedback.
    property bool isError: false
    property string errorMessage: ""
    property string errorCode: ""

    signal dismissed()

    MouseArea { anchors.fill: parent; onClicked: root.dismissed() }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(400, parent.width * 0.9)
        implicitHeight: sumCol.implicitHeight + 48
        radius: 20
        color: T.ThemeEngine.colors.card
        border { width: 1; color: root.isError ? Qt.alpha(T.ThemeEngine.failRed, 0.3) : T.ThemeEngine.colors.borderCard }

        MouseArea { anchors.fill: parent } // absorb clicks

        ColumnLayout {
            id: sumCol
            anchors { fill: parent; margins: 24 }
            spacing: 16

            // Status icon — success (green) or error (red)
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 64; implicitHeight: 64; radius: 32
                color: root.isError
                    ? Qt.alpha(T.ThemeEngine.failRed, 0.12)
                    : Qt.alpha(T.ThemeEngine.passGreen, 0.12)
                Label {
                    anchors.centerIn: parent
                    text: root.isError ? "❌" : "✅"
                    font.pixelSize: 32
                }
            }

            // Title
            Label {
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                text: root.isError ? "Capture Failed" : "Capture Complete"
                font.family: T.ThemeEngine.monoFont; font.pixelSize: 18
                font.weight: Font.Bold; color: T.ThemeEngine.textPrimary
            }

            // Error details (visible only when isError)
            ColumnLayout { spacing: 6; visible: root.isError
                Label {
                    Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                    text: root.errorCode
                    font.family: T.ThemeEngine.monoFont; font.pixelSize: 11
                    color: T.ThemeEngine.failRed; font.weight: Font.DemiBold
                }
                Label {
                    Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                    text: root.errorMessage
                    font.family: T.ThemeEngine.monoFont; font.pixelSize: 13
                    color: T.ThemeEngine.textSecondary; wrapMode: Text.WordWrap
                }
            }

            // Results (visible only on success)
            ColumnLayout { spacing: 8; visible: !root.isError
                RowLayout {
                    Label { text: "📸 Screenshots:"; font.family: T.ThemeEngine.monoFont; font.pixelSize: 13; color: T.ThemeEngine.textSecondary }
                    Item { Layout.fillWidth: true }
                    Label { text: root.totalScreenshots; font.family: T.ThemeEngine.monoFont; font.pixelSize: 14; font.weight: Font.Bold; color: T.ThemeEngine.textPrimary }
                }
                RowLayout {
                    visible: root.recordingFile !== ""
                    Label { text: "🎥 Recording:"; font.family: T.ThemeEngine.monoFont; font.pixelSize: 13; color: T.ThemeEngine.textSecondary }
                    Item { Layout.fillWidth: true }
                    Label { text: root.recordingFile !== "" ? "✓ Saved" : "—"; font.family: T.ThemeEngine.monoFont; font.pixelSize: 14; font.weight: Font.Bold; color: T.ThemeEngine.passGreen }
                }
                RowLayout {
                    Label { text: "⏱️ Duration:"; font.family: T.ThemeEngine.monoFont; font.pixelSize: 13; color: T.ThemeEngine.textSecondary }
                    Item { Layout.fillWidth: true }
                    Label { text: root.elapsedTime; font.family: T.ThemeEngine.monoFont; font.pixelSize: 14; font.weight: Font.Bold; color: T.ThemeEngine.textPrimary }
                }
            }

            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: T.ThemeEngine.colors.borderCard; visible: !root.isError }

            // Session path (success only)
            Label {
                Layout.fillWidth: true; visible: !root.isError
                text: "📁 " + root.sessionPath
                font.family: T.ThemeEngine.monoFont; font.pixelSize: 10
                color: Qt.alpha(T.ThemeEngine.textSecondary, 0.7)
                wrapMode: Text.WrapAnywhere
            }

            // Done / Dismiss button
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 44; radius: 12
                color: root.isError ? Qt.alpha(T.ThemeEngine.failRed, 0.2) : T.ThemeEngine.cyan
                Label {
                    anchors.centerIn: parent
                    text: root.isError ? "✕ Dismiss" : "✓ Done"
                    font.family: T.ThemeEngine.monoFont; font.pixelSize: 14
                    font.weight: Font.Bold; color: root.isError ? T.ThemeEngine.failRed : "#0F172A"
                }
                MouseArea {
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: root.dismissed()
                }
            }
        }
    }
}
