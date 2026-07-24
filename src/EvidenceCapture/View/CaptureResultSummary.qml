// =============================================================================
// CaptureResultSummary.qml — Completion summary after capture finishes
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

    signal dismissed()

    MouseArea { anchors.fill: parent; onClicked: root.dismissed() }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(400, parent.width * 0.9)
        implicitHeight: sumCol.implicitHeight + 48
        radius: 20
        color: T.ThemeEngine.colors.card
        border { width: 1; color: T.ThemeEngine.colors.borderCard }

        MouseArea { anchors.fill: parent } // absorb clicks

        ColumnLayout {
            id: sumCol
            anchors { fill: parent; margins: 24 }
            spacing: 16

            // Success icon
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 64; implicitHeight: 64; radius: 32
                color: Qt.alpha(T.ThemeEngine.passGreen, 0.12)
                Label {
                    anchors.centerIn: parent
                    text: "✅"
                    font.pixelSize: 32
                }
            }

            Label {
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                text: "Capture Complete"
                font.family: T.ThemeEngine.monoFont; font.pixelSize: 18
                font.weight: Font.Bold; color: T.ThemeEngine.textPrimary
            }

            // Results
            ColumnLayout { spacing: 8
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

            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: T.ThemeEngine.colors.borderCard }

            // Session path
            Label {
                Layout.fillWidth: true
                text: "📁 " + root.sessionPath
                font.family: T.ThemeEngine.monoFont; font.pixelSize: 10
                color: Qt.alpha(T.ThemeEngine.textSecondary, 0.7)
                wrapMode: Text.WrapAnywhere
            }

            // Done button
            Rectangle {
                Layout.fillWidth: true; implicitHeight: 44; radius: 12
                color: T.ThemeEngine.cyan
                Label {
                    anchors.centerIn: parent
                    text: "✓ Done"
                    font.family: T.ThemeEngine.monoFont; font.pixelSize: 14
                    font.weight: Font.Bold; color: "#0F172A"
                }
                MouseArea {
                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: root.dismissed()
                }
            }
        }
    }
}
