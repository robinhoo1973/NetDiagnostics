// =============================================================================
// MetricCard.qml — Compact L5 detail card showing a large metric number with label
//
// Displays a key metric (e.g. "Round-trip time: 23ms") in a rounded card with
// a counting-up animation.  Uses pure PropertyAnimation (NumberAnimation on a
// dummy _animValue property) — no Canvas, no ShaderEffect, safe on iOS static
// Qt builds (5WHY #1: inline rendering primitives only).
//
// Usage:
//   MetricCard { label: "Round-trip time"; value: 23; unit: "ms" }
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import widgets
import "KeyMetric.js" as KM

Item {
    id: root

    // ── Public API ────────────────────────────────────────────────────────
    property string label: ""
    property real value: 0
    property string unit: "ms"
    // 5WHY: value precision was hardcoded via Math.round() — packet loss of
    // 0.5% rendered as 0 and avg latencies lost decimals.  Callers now pass
    // the metric's native precision (0/1) from KeyMetric.js.
    property int precision: 0
    // 5WHY: durations >= 60s are m:ss ("1:23").  The old DetailPage fed the
    // formatted string through parseFloat() which silently truncated it to
    // "1".  MetricCard now animates total SECONDS and formats m:ss itself —
    // no string parsing anywhere.
    property string format: "num"   // "num" | "minsec"
    // 5WHY: ratio metrics (e.g. "2/3 ports") need a literal suffix after the
    // unit — "/3".  Kept as raw text (not translated); only the unit is.
    property string trailing: ""
    property color accentColor: ThemeEngine.colors.primary

    implicitWidth: 160
    implicitHeight: 72

    // ── Display text — derived from the ANIMATED value ────────────────────
    // 5WHY: dedup — min:sec + precision formatting now lives in the shared
    // KeyMetric.js module (same logic DiagBlock uses for its static tile text).
    readonly property string _displayText: KM.formatNumber(root._animValue, root.precision, root.format)
    // Accessibility reads the FINAL value (not the animating one).
    readonly property string _accessText: KM.formatNumber(root.value, root.precision, root.format)

    // 5WHY: QML NumberAnimation cannot target a property that is also being
    // assigned by a binding (binding conflicts with animation driver).
    // Solution: _animValue is a private dummy property driven by animation;
    // the display Label binds its text to Math.round(_animValue).  When
    // root.value changes, the animation runs from the previous _animValue
    // to the new root.value, and the label follows via binding.
    property real _animValue: 0

    // ── Trigger count-up animation on value change ─────────────────────────
    onValueChanged: {
        anim.from = _animValue
        anim.to = value
        anim.start()
    }

    NumberAnimation {
        id: anim
        target: root
        property: "_animValue"
        duration: 400
        easing.type: Easing.OutCubic
    }

    // ── Visual ────────────────────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        radius: ThemeEngine.radius.md  // 8
        color: ThemeEngine.colors.surfaceContainerLow
        border {
            width: 1
            color: ThemeEngine.colors.outlineVariant
        }

        ColumnLayout {
            anchors {
                fill: parent
                leftMargin: ThemeEngine.spacing.md   // 12
                rightMargin: ThemeEngine.spacing.md
                topMargin: 10
                bottomMargin: 10
            }
            spacing: 4

            // ── Label (top) ───────────────────────────────────────────────
            AppLabel {
                Layout.fillWidth: true
                text: root.label
                font.family: ThemeEngine.fontUi
                font.pixelSize: 10
                // 5WHY: Use font.weight instead of bold property — Font.Bold
                // is cross-platform consistent; `bold: true` maps to different
                // weights per platform font engine.
                font.weight: Font.Medium
                color: ThemeEngine.colors.onSurfaceVariant
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            // ── Value + Unit (bottom) ─────────────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                // Large animated number
                Label {
                    text: root._displayText
                    font.family: ThemeEngine.monoFont
                    font.pixelSize: 22
                    font.weight: Font.Bold
                    color: root.accentColor
                    // 5WHY: Right-aligned for numeric data — aligns decimal
                    // positions across multiple MetricCards in a grid, and
                    // mirrors correctly under RTL via T.textAlignEnd.
                    horizontalAlignment: T.textAlignEnd
                }

                // Unit suffix (smaller, secondary color) + ratio trailing
                Label {
                    text: root.unit + root.trailing
                    font.family: ThemeEngine.monoFont
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    color: ThemeEngine.colors.onSurfaceVariant
                    // 5WHY: baseline alignment — the unit label must sit on
                    // the same baseline as the value label so "23 ms" reads
                    // as one visual phrase.  Without vertical alignment the
                    // smaller unit text floats up and looks disconnected.
                    verticalAlignment: Text.AlignBottom
                    // Bottom padding to optically align baseline with 22px value
                    bottomPadding: 2
                }

                Item { Layout.fillWidth: true }
            }
        }
    }

    // ── Accessibility ─────────────────────────────────────────────────────
    Accessible.name: root.label + ": " + root._accessText
        + (root.unit ? " " + root.unit : "") + root.trailing
    Accessible.role: Accessible.StaticText
}
