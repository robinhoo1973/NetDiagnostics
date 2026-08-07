// AppBar.qml — Shared app bar widget (Material Design 3 compact, 48px)
// Replaces the duplicated AppBar pattern in all 5 screen files.
//
// Callers choose how to position: anchors (anchored) or Layout (in ColumnLayout).
//
// Usage (anchored):            Usage (in Layout):
//   AppBar {                     AppBar {
//     anchors { left: ...          Layout.fillWidth: true
//       right: ... top: ... }      iconName: "gear"
//     iconName: "gear"             title: T.tr("settings")
//     title: T.tr("settings")         }
//   }
//
// 5WHY: The AppBar pattern (Rectangle + RowLayout + AppIcon + Label + spacer)
// was duplicated identically across all 5 screens.  Extracting once ensures
// icon color, spacing, font, and border are a single design decision.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../theme" as Th

Rectangle {
    id: root

    property string iconName: ""
    property string title: ""

    implicitHeight: 48
    color: Th.ThemeEngine.colors.navBar
    border { width: 1; color: Th.ThemeEngine.colors.borderCard }

    // Caller-provided children appear after title, before fillWidth spacer.
    // DiagnosticScreen uses this for the capture indicator badge.
    default property alias content: titleRow.data

    RowLayout {
        id: titleRow
        anchors { fill: parent; leftMargin: Th.ThemeEngine.spacing.lg; rightMargin: Th.ThemeEngine.spacing.lg }
        AppIcon {
            name: root.iconName; size: 20
            color: Th.ThemeEngine.colors.cyan
        }
        Item { width: Th.ThemeEngine.spacing.md }
        Label {
            text: root.title
            // 5WHY: AppBar title is UI chrome — proportional fontUi, 16px to
            // match the section-header hierarchy (was mono 15px).
            font.family: Th.ThemeEngine.fontUi; font.pixelSize: 16
            font.weight: Font.DemiBold
            color: Th.ThemeEngine.colors.textPrimary
        }
        // Caller's children inserted here by default property alias.
        //
        // IMPORTANT: Layout.fillWidth spacer must be provided by the caller
        // as their LAST child.  QML's default property appends user-provided
        // items AFTER component-internal children.  If the spacer were here,
        // any caller child (e.g. capture indicator badge) would be pushed
        // to the right edge instead of sitting adjacent to the title.
    }
}
