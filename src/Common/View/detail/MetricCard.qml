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
import "../theme"

Item {
    id: root

    // ── Public API ────────────────────────────────────────────────────────
    property string label: ""
    property real value: 0
    property string unit: "ms"
    property color accentColor: ThemeEngine.colors.primary

    implicitWidth: 160
    implicitHeight: 72

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
        color: ThemeEngine.colors.card
        border {
            width: 1
            color: ThemeEngine.colors.borderCard
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
                color: ThemeEngine.colors.textSecondary
                elide: Text.ElideRight
                maximumLineCount: 1
            }

            // ── Value + Unit (bottom) ─────────────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                // Large animated number
                Label {
                    text: Math.round(root._animValue)
                    font.family: ThemeEngine.monoFont
                    font.pixelSize: 22
                    font.weight: Font.Bold
                    color: root.accentColor
                    // 5WHY: Right-aligned for numeric data — aligns decimal
                    // positions across multiple MetricCards in a grid, and
                    // mirrors correctly under RTL via T.textAlignEnd.
                    horizontalAlignment: T.textAlignEnd
                }

                // Unit suffix (smaller, secondary color)
                Label {
                    text: root.unit
                    font.family: ThemeEngine.monoFont
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    color: ThemeEngine.colors.textSecondary
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
    Accessible.name: root.label + ": " + Math.round(root.value) + " " + root.unit
    Accessible.role: Accessible.StaticText
}
