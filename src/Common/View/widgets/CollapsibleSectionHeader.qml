// =============================================================================
// CollapsibleSectionHeader.qml — reusable collapsible section header
//
// Used by DetailPage for the "Properties" and "Detailed Data" sections.
// Encapsulates the tappable title row (label + ▲/▼ toggle) with the
// MouseArea on a wrapper Item (5WHY: anchors.fill on a layout-managed child
// is undefined behavior per qmllint) and localized Accessible labels.
//
// 5WHY: DetailPage previously duplicated this ~15-line header twice (the two
// sections drifted apart as fixes were applied to only one).  One component
// = one header = no drift; the caller just supplies title + expanded state.
//
// Usage:
//   CollapsibleSectionHeader {
//       Layout.fillWidth: true
//       title: T.tr("detailProperties")
//       expanded: page.propsExpanded
//       onToggleRequested: page.propsExpanded = !page.propsExpanded
//   }
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme

Item {
    id: root

    // ── Public API ────────────────────────────────────────────────────────
    property string title: ""
    property bool expanded: false
    signal toggleRequested()

    implicitHeight: headerRow.implicitHeight

    // ── Header row ────────────────────────────────────────────────────────
    RowLayout {
        id: headerRow
        anchors.fill: parent
        Label {
            text: root.title
            font.family: ThemeEngine.monoFont
            font.pixelSize: 12; font.weight: Font.Bold
            color: ThemeEngine.colors.textPrimary
        }
        Item { Layout.fillWidth: true }
        Label {
            text: root.expanded ? "▲" : "▼"
            font.pixelSize: 10; color: ThemeEngine.colors.textSecondary
        }
    }

    // ── Tap target (overlay, not a layout child) ──────────────────────────
    MouseArea {
        anchors.fill: parent
        onClicked: root.toggleRequested()
        cursorShape: Qt.PointingHandCursor
        Accessible.name: root.title
            + (root.expanded ? T.tr("accExpanded") : T.tr("accCollapsed"))
        Accessible.role: Accessible.Button
    }
}
