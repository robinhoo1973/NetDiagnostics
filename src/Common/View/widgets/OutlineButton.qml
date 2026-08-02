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
    border {
        width: activeFocus ? 2 : 1.5
        color: activeFocus ? T.ThemeEngine.colors.borderFocused
              : Qt.alpha(T.ThemeEngine.colors.textSecondary, 0.25)
    }
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

    // 5WHY: OutlineButton was the only button component without keyboard
    // or screen-reader support. Used as Cancel/Dismiss in capture overlays.
    activeFocusOnTab: true
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            root.clicked(); event.accepted = true
        }
    }
    Accessible.name: root.text
    Accessible.role: Accessible.Button
}
