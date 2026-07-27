// AppBar.qml — Shared app bar widget (Material Design 3 compact, 48px)
// Replaces the duplicated AppBar pattern in all 5 screen files.
//
// Callers choose how to position: anchors (anchored) or Layout (in ColumnLayout).
//
// Usage (anchored):            Usage (in Layout):
//   AppBar {                     AppBar {
//     anchors { left: ...          Layout.fillWidth: true
//       right: ... top: ... }      iconName: "gear"
//     iconName: "gear"             title: Tr.settings
//     title: Tr.settings         }
//   }
//
// 5WHY: The AppBar pattern (Rectangle + RowLayout + AppIcon + Label + spacer)
// was duplicated identically across all 5 screens.  Extracting once ensures
// icon color, spacing, font, and border are a single design decision.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as T

Rectangle {
    id: root

    property string iconName: ""
    property string title: ""

    implicitHeight: 48
    color: T.ThemeEngine.colors.navBar
    border { width: 1; color: T.ThemeEngine.colors.borderCard }

    // Caller-provided children appear after title, before fillWidth spacer.
    // DiagnosticScreen uses this for the capture indicator badge.
    default property alias content: titleRow.data

    RowLayout {
        id: titleRow
        anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
        AppIcon {
            name: root.iconName; size: 20
            color: T.ThemeEngine.cyan
        }
        Item { width: 10 }
        Label {
            text: root.title
            font.family: T.ThemeEngine.monoFont; font.pixelSize: 15
            font.weight: Font.DemiBold
            color: T.ThemeEngine.textPrimary
        }
        // Caller's children inserted here by default property alias
        Item { Layout.fillWidth: true }
    }
}
