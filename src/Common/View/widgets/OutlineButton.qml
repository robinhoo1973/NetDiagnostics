// OutlineButton.qml — Shared outlined secondary button (Material Design 3)
// 5WHY: The cancel button pattern (outlined Rectangle + Label + MouseArea
// with press-scale animation) was duplicated identically in 3 capture
// overlay files.  Extract once so a design change (radius, border width,
// animation duration) updates all cancel buttons uniformly.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as T

Rectangle {
    id: root
    property string text: ""
    signal clicked()

    Layout.fillWidth: true
    implicitHeight: 48; radius: 14
    color: "transparent"
    border { width: 1.5; color: Qt.alpha(T.ThemeEngine.colors.textSecondary, 0.25) }
    scale: btnMouse.pressed ? 0.97 : 1.0
    Behavior on scale { NumberAnimation { duration: 100 } }

    Label {
        anchors.centerIn: parent
        text: root.text
        font.family: T.ThemeEngine.monoFont
        font.pixelSize: 14; font.weight: Font.DemiBold
        color: T.ThemeEngine.colors.textSecondary
    }
    MouseArea {
        id: btnMouse
        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
