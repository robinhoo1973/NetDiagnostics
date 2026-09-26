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

    // 5WHY (2026-09-27 命中区/键盘): 曾 implicitHeight 由 12px 字行高决定
    // （≈17px，不达 WCAG 2.5.5 的 24px 底线）且 MouseArea 无键盘激活——
    // 同页 IconActionButton 是 44px+键盘+读屏契约，两套标准。统一最低可
    // 触契约：高度钳 44（视觉行保持紧凑、交互层外扩）+ 空格/回车激活。
    implicitHeight: Math.max(headerRow.implicitHeight, 44)

    // ── Header row ────────────────────────────────────────────────────────
    RowLayout {
        id: headerRow
        anchors.fill: parent
        Label {
            text: root.title
            font.family: ThemeEngine.monoFont
            font.pixelSize: 12; font.weight: Font.Bold
            color: ThemeEngine.colors.onSurface
        }
        Item { Layout.fillWidth: true }
        Label {
            text: root.expanded ? "▲" : "▼"
            font.pixelSize: 10; color: ThemeEngine.colors.onSurfaceVariant
        }
    }

    // ── Tap target (overlay, not a layout child) ──────────────────────────
    MouseArea {
        id: tapArea
        anchors.fill: parent
        onClicked: root.toggleRequested()
        cursorShape: Qt.PointingHandCursor
        Accessible.name: root.title
            + (root.expanded ? T.tr("accExpanded") : T.tr("accCollapsed"))
        Accessible.role: Accessible.Button
        focus: true
        Keys.onSpacePressed: root.toggleRequested()
        Keys.onReturnPressed: root.toggleRequested()
        Keys.onEnterPressed: root.toggleRequested()
    }
}
