// Toast.qml — 浮动提示条（NEW-7：由 PageToastSection 承载）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme

Rectangle {
    id: root
    property string text: ""

    visible: text !== ""
    implicitWidth: Math.min(360, toastRow.implicitWidth + 32)
    implicitHeight: 40
    radius: ThemeEngine.radius.lg
    color: Qt.alpha(ThemeEngine.colors.surfaceContainerLow, 0.96)
    border { width: 1; color: ThemeEngine.colors.outlineVariant }
    opacity: text !== "" ? 1.0 : 0.0
    Behavior on opacity { NumberAnimation { duration: 160 } }

    RowLayout {
        id: toastRow
        anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
        spacing: 8
        AppIcon { name: "badge-info"; size: 16; color: ThemeEngine.colors.tertiary }
        Label {
            Layout.fillWidth: true
            text: root.text
            elide: Text.ElideRight
            color: ThemeEngine.colors.onSurface
            font.family: ThemeEngine.fontUi; font.pixelSize: ThemeEngine.fontSize.body
        }
    }
}
