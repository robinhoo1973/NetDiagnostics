// ShareSubscriptionDialog.qml — Shared share/subscription dialog (M3)
// 5WHY: The share/subscription dialog was duplicated identically in
// DiagnosticScreen, DashboardScreen, and ReportScreen (~38 lines each).
// Extract once so adding a new button, changing the layout, or updating
// the subscription flow affects all screens uniformly.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../theme" as T
import "../widgets"

Rectangle {
    id: root
    anchors.fill: parent
    color: Qt.alpha(T.ThemeEngine.colors.surface, 0.85)
    visible: shareStage !== 0; z: 1100

    required property int shareStage       // 0=none, 1=subscribe, 2=confirm
    required property bool isMobile
    signal dismissed()

    MouseArea { anchors.fill: parent; onClicked: root.dismissed() }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(420, parent.width * 0.92)
        implicitHeight: dlgCol.implicitHeight + 40; radius: 14
        color: T.ThemeEngine.colors.card
        ColumnLayout {
            id: dlgCol
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 }
            spacing: 14

            // Icon badge
            Rectangle {
                Layout.alignment: Qt.AlignHCenter; width: 60; height: 60; radius: 30
                color: Qt.alpha(root.shareStage === 1 ? T.ThemeEngine.warnYellow : T.ThemeEngine.cyan, 0.12)
                AppIcon {
                    anchors.centerIn: parent
                    name: root.shareStage === 1 ? "badge-info" : "report"; size: 28
                    color: root.shareStage === 1 ? T.ThemeEngine.warnYellow : T.ThemeEngine.cyan
                }
            }

            // Title
            Label {
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                text: root.shareStage === 1 ? T.Tr.subscribeTitle : T.Tr.confirmShareTitle
                font.family: T.ThemeEngine.monoFont; font.pixelSize: 17
                font.weight: Font.Bold; color: T.ThemeEngine.textPrimary; wrapMode: Text.WordWrap
            }

            // Body
            Label {
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                text: root.shareStage === 1 ? T.Tr.subscribeBody : T.Tr.confirmShareBody
                font.family: T.ThemeEngine.monoFont; font.pixelSize: 13
                color: T.ThemeEngine.textSecondary; wrapMode: Text.WordWrap
            }

            // Buttons
            RowLayout {
                Layout.fillWidth: true; spacing: 10

                // Cancel / Not Now
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 42; radius: 8
                    color: "transparent"
                    border { width: 1; color: Qt.alpha(T.ThemeEngine.textSecondary, 0.5) }
                    Label {
                        anchors.centerIn: parent
                        text: root.shareStage === 1 ? T.Tr.subscribeNotNow : T.Tr.dialogCancel
                        font.family: T.ThemeEngine.monoFont; font.pixelSize: 13
                        color: T.ThemeEngine.textSecondary
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: root.dismissed()
                    }
                    Accessible.name: root.shareStage === 1 ? T.Tr.subscribeNotNow : T.Tr.dialogCancel
                    Accessible.role: Accessible.Button
                }

                // Subscribe / Share
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 42; radius: 8
                    color: root.shareStage === 1 ? T.ThemeEngine.warnYellow : T.ThemeEngine.cyan
                    Label {
                        anchors.centerIn: parent
                        text: root.shareStage === 1 ? T.Tr.subscribeBtn
                              : (root.isMobile ? T.Tr.shareBtn : T.Tr.emailBtn)
                        font.family: T.ThemeEngine.monoFont; font.pixelSize: 13
                        font.weight: Font.DemiBold; color: T.ThemeEngine.bgDark
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: root.actionRequested()
                    }
                }
            }
        }
    }

    signal actionRequested()
}
