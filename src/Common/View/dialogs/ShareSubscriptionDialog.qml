// ShareSubscriptionDialog.qml — Shared share/subscription dialog (M3)
// 5WHY: The share/subscription dialog was duplicated identically in
// DiagnosticScreen, DashboardScreen, and ReportScreen (~38 lines each).
// Extract once so adding a new button, changing the layout, or updating
// the subscription flow affects all screens uniformly.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as T
import "../widgets"

Rectangle {
    id: root
    anchors.fill: parent
    color: Qt.alpha(T.ThemeEngine.colors.surface, 0.85)
    visible: shareStage !== 0; z: 1100

    required property int shareStage       // 0=none, 1=subscribe, 2=confirm
    required property bool isMobile
    property bool showProBadge: false      // ReportScreen shows a PRO badge
    property bool showIconBorder: false    // ReportScreen shows border on icon
    signal dismissed()

    MouseArea { anchors.fill: parent; onClicked: root.dismissed() }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(420, parent.width * 0.92)
        implicitHeight: dlgCol.implicitHeight + 40; radius: 14
        color: T.ThemeEngine.colors.card
        // 5WHY: Without this MouseArea, clicks on empty card space (between
        // title text and buttons, or near-misses) propagate to the backdrop
        // MouseArea and dismiss the dialog.  Absorb clicks so only explicit
        // button presses or backdrop taps trigger actions.
        MouseArea { anchors.fill: parent }
        ColumnLayout {
            id: dlgCol
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 }
            spacing: 14

            // Icon badge
            Rectangle {
                Layout.alignment: Qt.AlignHCenter; width: 60; height: 60; radius: 30
                color: Qt.alpha(root.shareStage === 1 ? T.ThemeEngine.colors.warnYellow : T.ThemeEngine.colors.cyan, 0.12)
                border {
                    // 5WHY: border.visible does not exist in Qt Quick Rectangle —
                    // Border has only width/color properties.  Setting an
                    // undeclared property causes "Cannot assign to non-existent
                    // property" fatal error at QML load time.  Use width: 0 to
                    // hide the border instead.
                    width: root.showIconBorder ? 1.5 : 0
                    color: Qt.alpha(root.shareStage === 1 ? T.ThemeEngine.colors.warnYellow : T.ThemeEngine.colors.cyan, 0.35)
                }
                AppIcon {
                    anchors.centerIn: parent
                    name: root.shareStage === 1 ? "badge-info" : "report"; size: 28
                    color: root.shareStage === 1 ? T.ThemeEngine.colors.warnYellow : T.ThemeEngine.colors.cyan
                }
            }

            // Title
            Label {
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                text: root.shareStage === 1 ? T.Tr.subscribeTitle : T.Tr.confirmShareTitle
                font.family: T.ThemeEngine.monoFont; font.pixelSize: 17
                font.weight: Font.Bold; color: T.ThemeEngine.colors.textPrimary; wrapMode: Text.WordWrap
            }

            // Body
            Label {
                Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                text: root.shareStage === 1 ? T.Tr.subscribeBody : T.Tr.confirmShareBody
                font.family: T.ThemeEngine.monoFont; font.pixelSize: 13
                color: T.ThemeEngine.colors.textSecondary; wrapMode: Text.WordWrap
            }

            // PRO badge (subscribe stage, optional)
            Rectangle {
                visible: root.showProBadge && root.shareStage === 1
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: proRow.implicitWidth + 20; implicitHeight: 26; radius: 13
                color: Qt.alpha(T.ThemeEngine.colors.warnYellow, 0.15)
                RowLayout {
                    id: proRow
                    anchors.centerIn: parent; spacing: 5
                    AppIcon { name: "badge-check"; size: 12; color: T.ThemeEngine.colors.warnYellow }
                    Label { text: T.Tr.premiumBadge; color: T.ThemeEngine.colors.warnYellow
                        font.family: T.ThemeEngine.monoFont; font.pixelSize: 11; font.weight: Font.Bold }
                }
            }

            // Buttons
            RowLayout {
                Layout.fillWidth: true; Layout.topMargin: 4; spacing: 10
                DialogBtn {
                    label: root.shareStage === 1 ? T.Tr.subscribeNotNow : T.Tr.dialogCancel
                    accent: T.ThemeEngine.colors.textSecondary; filled: false
                    onClicked: root.dismissed()
                }
                DialogBtn {
                    label: root.shareStage === 1 ? T.Tr.subscribeBtn
                           : (root.isMobile ? T.Tr.shareBtn : T.Tr.emailBtn)
                    accent: root.shareStage === 1 ? T.ThemeEngine.colors.warnYellow : T.ThemeEngine.colors.cyan
                    filled: true
                    onClicked: root.actionRequested()
                }
            }
        }
    }

    signal actionRequested()
}
