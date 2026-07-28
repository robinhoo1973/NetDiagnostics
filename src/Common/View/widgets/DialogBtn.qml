// DialogBtn.qml — Shared dialog action button (outlined or filled)
// 5WHY: This component was inline in ReportScreen.qml as `component DialogBtn`.
// Extract so it can be reused by ShareSubscriptionDialog and any future dialog.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as T

Rectangle {
    id: dbtn
    property string label: ""
    property color accent: T.ThemeEngine.colors.cyan
    property bool filled: false
    signal clicked()

    Layout.fillWidth: true; implicitHeight: 42; radius: 8
    color: dbtn.filled ? dbtn.accent : "transparent"
    border { width: 1; color: dbtn.filled ? "transparent" : Qt.alpha(dbtn.accent, 0.5) }

    Label {
        anchors.centerIn: parent
        text: dbtn.label
        // 5WHY: On narrow iOS screens (iPhone SE, 375pt), translated
        // labels like 'Premium freischalten' overflow the button width
        // without elide.  ElideRight gracefully truncates with ellipsis.
        elide: Text.ElideRight
        color: dbtn.filled ? T.ThemeEngine.colors.surface : dbtn.accent
        font.family: T.ThemeEngine.monoFont; font.pixelSize: 13; font.weight: Font.DemiBold
    }
    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: dbtn.clicked() }
}
