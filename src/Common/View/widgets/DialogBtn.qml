// DialogBtn.qml — Shared dialog action button (outlined or filled)
// 5WHY: This component was inline in ReportScreen.qml as `component DialogBtn`.
// Extract so it can be reused by ShareSubscriptionDialog and any future dialog.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as Th

Rectangle {
    id: dbtn
    property string label: ""
    property color accent: Th.ThemeEngine.colors.cyan
    property bool filled: false
    signal clicked()

    Layout.fillWidth: true; implicitHeight: 44; radius: 8
    color: dbtn.filled ? dbtn.accent : "transparent"
    border { width: 1; color: dbtn.filled ? "transparent" : Qt.alpha(dbtn.accent, 0.5) }

    // 5WHY: ElideRight only engages when the Label has a constrained width.
    // anchors.centerIn lets the Label grow to its full implicitWidth — elide
    // never triggers.  Use left+right anchors instead to constrain to the
    // parent Rectangle, with horizontalAlignment to keep text centered.
    Label {
        anchors { left: parent.left; right: parent.right; leftMargin: Th.ThemeEngine.spacing.sm; rightMargin: Th.ThemeEngine.spacing.sm; verticalCenter: parent.verticalCenter }
        text: dbtn.label
        // 5WHY: ElideRight only engages when the Label has a constrained width.
        // anchors.centerIn lets the Label grow to its full implicitWidth — elide
        // never triggers.  Use left+right anchors instead to constrain to the
        // parent Rectangle, with horizontalAlignment to keep text centered.
        // Elide direction follows the script (start edge in RTL).
        elide: T.isRtl ? Text.ElideLeft : Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        color: dbtn.filled ? Th.ThemeEngine.colors.surface : dbtn.accent
        font.family: Th.ThemeEngine.fontUi; font.pixelSize: 13; font.weight: Font.DemiBold
    }
    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: dbtn.clicked() }
}
