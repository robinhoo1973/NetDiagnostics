// =============================================================================
// PageSection.qml — Unified display base class (UI layer core)
//
// Per review/refactor/ui/ui-refactor-architecture-guide.md §3.
// Collapse contract (§3.3): Layout.fillWidth/topMargin/bottomMargin,
// implicitHeight, visible are ALL bound to active — derived classes MUST NOT
// override them (height goes through sectionImplicitHeight only).
// Bar sections MUST set fixedHeight (5WHY UI-1, NEW-6).
// =============================================================================
import QtQuick
import QtQuick.Layouts
import theme

Item {
    id: root
    // ── 1. Identity ──
    property string sectionId: ""
    property string sectionTitle: ""
    // ── 2. Layout contract ──
    property bool active: true
    property real topMargin: 0
    property real bottomMargin: 0
    property real minHeight: 0
    property real fixedHeight: -1
    property real contentSpacing: ThemeEngine.spacing.xs
    // ── 3. Chrome ──
    enum BackgroundStyle { Plain, Card, Bar }
    property int backgroundStyle: PageSection.Card
    property color cardColor: ThemeEngine.colors.card
    property color borderColor: ThemeEngine.colors.borderCard
    property real cardRadius: ThemeEngine.radius.md
    property real paddingH: 12
    property real paddingV: 12
    // ── 4. Events ──
    signal actionRequested(string action, var payload)
    function emitAction(action, payload) { root.actionRequested(action, payload) }
    // ── 5. Collapse contract (DO NOT override) ──
    Layout.fillWidth: true
    Layout.topMargin: active ? topMargin : 0
    Layout.bottomMargin: active ? bottomMargin : 0
    implicitHeight: active ? (fixedHeight >= 0 ? fixedHeight : Math.max(minHeight, sectionImplicitHeight)) : 0
    visible: active
    readonly property real insets: backgroundStyle === PageSection.Card ? 2 * paddingV : 0
    property real sectionImplicitHeight: body.implicitHeight + insets
    // ── 6. Chrome visuals (P2: loaded on demand) ──
    Loader {
        id: cardChromeLoader
        anchors.fill: parent
        active: root.backgroundStyle === PageSection.Card
        sourceComponent: Rectangle {
            anchors.fill: parent   // R1-6: Loader 不自动缩放子项——显式填充才渲染，否则 0×0 不可见
            radius: root.cardRadius
            color: root.cardColor
            border { width: 1; color: root.borderColor }
        }
    }
    Loader {
        id: barChromeLoader
        anchors.fill: parent
        active: root.backgroundStyle === PageSection.Bar
        sourceComponent: Rectangle {
            anchors.fill: parent   // R1-6
            color: ThemeEngine.colors.navBar
            border { width: 1; color: root.borderColor }
        }
    }
    // ── 7. Content slot ──
    ColumnLayout {
        id: body
        anchors { fill: parent; leftMargin: hPad; rightMargin: hPad; topMargin: vPad; bottomMargin: vPad }
        spacing: root.contentSpacing
    }
    readonly property real hPad: backgroundStyle === PageSection.Card ? paddingH : 0
    readonly property real vPad: backgroundStyle === PageSection.Card ? paddingV : 0
    default property alias content: body.data
    // ── 8. Accessibility ──
    Accessible.role: Accessible.Pane
    Accessible.name: root.sectionTitle
    // NEW-22: interactive sections (tappable tiles/switch rows) override
    // Accessible.role (Button/CheckBox) + focusPolicy (Controls TabFocus).
}
