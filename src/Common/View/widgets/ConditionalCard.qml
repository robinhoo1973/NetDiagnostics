// =============================================================================
// ConditionalCard.qml — card that fully collapses when inactive
//
// Encapsulates the Qt layout edge-case workaround where invisible children
// with non-zero implicitHeight can leak phantom spacing into the parent
// ColumnLayout.  The pattern (visible + implicitHeight + Layout.topMargin
// all conditional on the same flag) was duplicated 5+ times across
// DetailPage, Dashboard, and DiagGroupPanel.
//
// Usage:
//   ConditionalCard {
//       active: _hasSection
//       topMargin: ThemeEngine.spacing.lg
//       // Override card styling:
//       color: Qt.alpha(ThemeEngine.colors.failRed, 0.06)
//       borderColor: Qt.alpha(ThemeEngine.colors.failRed, 0.5)
//       Label { text: "section content" }
//   }
// =============================================================================
import QtQuick
import QtQuick.Layouts
import "../theme"

Rectangle {
    id: root

    // ── Public API ────────────────────────────────────────────────────────
    property bool active: true
    property real topMargin: 0           // Layout.topMargin when active
    property real contentSpacing: 4     // internal ColumnLayout spacing
    property color cardColor: ThemeEngine.colors.card
    property color borderColor: ThemeEngine.colors.borderCard
    property real cardRadius: ThemeEngine.radius.md  // 8

    // ── Conditional layout (the workaround) ───────────────────────────────
    Layout.fillWidth: true
    Layout.topMargin: active ? topMargin : 0
    implicitHeight: active ? bodyLayout.implicitHeight + 24 : 0
    visible: active

    radius: cardRadius
    color: cardColor
    border {
        width: 1
        color: borderColor
    }

    // ── Content slot — children land here via default property ────────────
    default property alias content: bodyLayout.data

    ColumnLayout {
        id: bodyLayout
        anchors { fill: parent; margins: 12 }
        spacing: root.contentSpacing
    }
}
