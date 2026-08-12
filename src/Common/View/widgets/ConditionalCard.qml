// =============================================================================
// ConditionalCard.qml — card that fully collapses when inactive and OWNS the
// gap to the next card.
//
// Encapsulates the Qt layout edge-case workaround where invisible children
// with non-zero implicitHeight can leak phantom spacing into the parent
// ColumnLayout.  The pattern (visible + implicitHeight + Layout margins all
// conditional on the same flag) was duplicated 5+ times across DetailPage,
// Dashboard, and DiagGroupPanel.
//
// Gap ownership (5WHY): each card carries its own trailing bottomMargin —
// consumers stack cards with spacing:0 and NEVER compute spacing against the
// previous/next element.  Adding a new section touches only that section.
//
//   +------- ConditionalCard -------+
//   |        Card Content Display        |
//   +--------- End of Content ------+
//   |          Gap Between Cards         |  ← bottomMargin (owned by THIS card)
//   +-- End of Conditional Card ---+
//
// Usage:
//   ConditionalCard {
//       active: _hasSection
//       bottomMargin: ThemeEngine.spacing.lg   // gap to the next card
//       minHeight: 80                          // optional height floor
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
    // topMargin is reserved for rare "gap below a non-card element" cases
    // (e.g. the hero's breathing room below the toolbar).  Inter-card spacing
    // always uses bottomMargin so the gap belongs to THIS card.
    property real topMargin: 0           // Layout.topMargin when active (rare)
    property real bottomMargin: 0        // Layout.bottomMargin when active — gap to next card
    property real minHeight: 0           // optional height floor (e.g. hero 80px)
    property real contentSpacing: 4     // internal ColumnLayout spacing
    property color cardColor: ThemeEngine.colors.card
    property color borderColor: ThemeEngine.colors.borderCard
    property real cardRadius: ThemeEngine.radius.md  // 8

    // ── Conditional layout (the workaround) ───────────────────────────────
    Layout.fillWidth: true
    Layout.topMargin: active ? topMargin : 0
    Layout.bottomMargin: active ? bottomMargin : 0
    implicitHeight: active ? Math.max(minHeight, bodyLayout.implicitHeight + 24) : 0
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
