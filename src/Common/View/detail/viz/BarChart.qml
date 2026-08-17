// =============================================================================
// BarChart.qml — Simple horizontal bar chart for L5 detail page data series
//
// Renders an array of {label, value, color} items as horizontal bars with
// labels.  Each bar animates its width from 0 on appear with a stagger delay.
// Uses pure Rectangle primitives — no Canvas, no ShapePath, no ShaderEffect
// — safe for iOS static Qt builds (5WHY #1: only basic rendering primitives
// survive all platform configurations).
//
// Usage:
//   BarChart {
//       values: [
//           { label: "DNS", value: 12, color: ThemeEngine.colors.success },
//           { label: "TCP", value: 34, color: ThemeEngine.colors.primary },
//           { label: "TLS", value: 89, color: ThemeEngine.colors.secondary }
//       ]
//       maxValue: 100
//   }
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme

Item {
    id: root

    // ── Public API ────────────────────────────────────────────────────────
    property var values: []
    // 5WHY: maxValue auto-calculated when 0 — walks the values array to find
    // the max.  This avoids stale hardcoded max values when data changes
    // (e.g., HTTP timing phases vary wildly between cold and warm connections).
    // When explicitly set (non-zero), the caller's value is used as-is.
    property real maxValue: 0

    implicitWidth: 280
    // 5WHY: implicitHeight tracks content height so parent ColumnLayouts can
    // size correctly.  Fixed heights would clip or waste space when the
    // number of bars changes.
    implicitHeight: Math.max(40, barCol.implicitHeight + 16)

    // 5WHY: shared stagger timer replaces N individual Timer QObjects (one
    // per bar) — each bar delegate had its own Timer that fired once and sat
    // idle forever.  Single timer + counter = 1 QObject instead of N.
    property int _revealIndex: -1
    Timer {
        id: staggerTimer
        interval: 80; repeat: true
        running: root.values && root.values.length > 0 && root._revealIndex < root.values.length - 1
        onTriggered: { root._revealIndex++ }
        onRunningChanged: { if (!running && root._revealIndex < 0) root._revealIndex = -1 }
    }
    Component.onCompleted: { if (root.values && root.values.length > 0) root._revealIndex = 0 }

    // ── Derived max ──────────────────────────────────────────────────────
    readonly property real _effectiveMax: {
        if (root.maxValue > 0) return root.maxValue
        if (!root.values || root.values.length === 0) return 100
        var m = 0
        for (var i = 0; i < root.values.length; i++) {
            var v = root.values[i]
            if (v && typeof v.value === 'number' && v.value > m) {
                m = v.value
            }
        }
        return m > 0 ? m : 100
    }

    // ── Visual ────────────────────────────────────────────────────────────
    // 5WHY: Flickable wrapping the Column — when there are many bars
    // (e.g., RTT per packet with 50+ packets), the chart must scroll rather
    // than forcing the parent to grow beyond the viewport.  Without this,
    // the detail page would overflow the screen on long traceroutes.
    Flickable {
        anchors.fill: parent
        contentWidth: barCol.width
        contentHeight: barCol.height
        clip: true
        flickableDirection: Flickable.VerticalFlick
        interactive: contentHeight > height

        Column {
            id: barCol
            width: parent.width
            spacing: 6

            // ── Empty state ─────────────────────────────────────────────
            Label {
                visible: root.values.length === 0
                anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
                // 5WHY: was hardcoded English — the 15-language app must
                // localize even the empty state.
                text: T.tr("noChartData")
                font.family: ThemeEngine.monoFont
                font.pixelSize: 11
                color: ThemeEngine.colors.textMuted
                topPadding: 8
            }

            // ── Bar items ───────────────────────────────────────────────
            Repeater {
                id: barRepeater
                model: root.values

                // 5WHY: Each bar is a standalone Item so width animations
                // don't cascade through a parent ColumnLayout's size
                // calculations.  Using a Column (not ColumnLayout) avoids
                // the QML layout engine re-measuring every sibling bar on
                // each animation frame — critical for smooth 60fps with
                // 20+ bars.
                Item {
                    id: barItem
                    width: barCol.width
                    implicitHeight: Math.max(22, barLabel.implicitHeight + 6)

                    // ── Data extraction ────────────────────────────────
                    readonly property var _item: modelData || {}
                    readonly property string _label: _item.label || ""
                    readonly property real _value: _item.value || 0
                    readonly property color _color: _item.color || ThemeEngine.colors.primary
                    readonly property real _barFraction: root._effectiveMax > 0
                        ? Math.min(1, Math.max(0, _value / root._effectiveMax)) : 0

                    // 5WHY: _barTargetWidth is the fully-revealed bar width
                    // (label area reserved at left, bar fills remaining space).
                    // Computed from available width minus label column width.
                    readonly property real _labelWidth: 80
                    readonly property real _barTargetWidth: _barFraction * (barItem.width - _labelWidth - 12)

                    // Animation driver for bar width
                    property real _animBarWidth: 0

                    // Reactive: when the shared stagger counter reaches our
                    // index, start the bar width animation for this bar.
                    // Replaces per-bar Timer QObjects (N → 1 shared Timer).
                    onRevealedChanged: {
                        if (revealed) {
                            barWidthAnim.from = 0
                            barWidthAnim.to = barItem._barTargetWidth
                            barWidthAnim.start()
                        }
                    }
                    property bool revealed: index <= root._revealIndex   // 可写（readonly 嵌套声明违反 B.2）

                    NumberAnimation {
                        id: barWidthAnim
                        target: barItem
                        property: "_animBarWidth"
                        duration: 400
                        easing.type: Easing.OutCubic
                    }

                    // ── Label (left-aligned, fixed width) ──────────────
                    Label {
                        id: barLabel
                        anchors {
                            left: parent.left
                            leftMargin: 4
                            verticalCenter: parent.verticalCenter
                        }
                        width: barItem._labelWidth
                        text: barItem._label
                        font.family: ThemeEngine.monoFont
                        font.pixelSize: 11
                        font.weight: Font.Medium
                        color: ThemeEngine.colors.onSurfaceVariant
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }

                    // ── Bar background track ───────────────────────────
                    Rectangle {
                        anchors {
                            left: parent.left
                            leftMargin: barItem._labelWidth + 8
                            right: parent.right
                            rightMargin: 4
                            verticalCenter: parent.verticalCenter
                        }
                        height: 12
                        radius: 3
                        color: ThemeEngine.colors.surfaceContainerHighest
                    }

                    // ── Bar fill (animated width) ──────────────────────
                    Rectangle {
                        anchors {
                            left: parent.left
                            leftMargin: barItem._labelWidth + 8
                            verticalCenter: parent.verticalCenter
                        }
                        // 5WHY: width is bound to _animBarWidth and clamped
                        // to prevent negative/overflow values during layout
                        // and animation edge cases (e.g., model changes
                        // mid-animation).
                        width: Math.max(0, barItem._animBarWidth)
                        height: 12
                        radius: 3
                        color: barItem._color

                        Behavior on color {
                            ColorAnimation { duration: 300 }
                        }
                    }

                    // ── Value label (right of bar) ─────────────────────
                    Label {
                        anchors {
                            left: parent.left
                            leftMargin: barItem._labelWidth + 8 + barItem._animBarWidth + 6
                            verticalCenter: parent.verticalCenter
                        }
                        text: barItem._value.toFixed(1)
                        font.family: ThemeEngine.monoFont
                        font.pixelSize: 11
                        font.weight: Font.Bold
                        color: barItem._color
                        // 5WHY: visible only when bar has some width —
                        // prevents the value label from overlapping the
                        // label text when the bar hasn't animated yet.
                        visible: barItem._animBarWidth > 20
                    }
                }
            }
        }
    }

    // ── Accessibility ─────────────────────────────────────────────────────
    Accessible.name: "Bar chart with " + (root.values ? root.values.length : 0) + " items"
    Accessible.role: Accessible.Chart
}
