// =============================================================================
// Gauge.qml — Horizontal bar gauge for L5 detail page metrics
//
// Displays a percentage or ratio (e.g. packet loss, score) as an animated
// horizontal bar.  Uses pure Rectangle + NumberAnimation — no Canvas, no
// ShapePath, no ShaderEffect — safe for iOS static Qt (5WHY #1: circular
// arcs require Canvas or ShapePath, both of which have known rendering
// issues on iOS static builds.  A horizontal bar delivers the same "fill
// level" mental model with only basic primitives.)
//
// Usage:
//   Gauge { value: 2.3; maxValue: 100; unit: "%"; gaugeColor: ThemeEngine.colors.success }
// =============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme

Item {
    id: root

    // ── Public API ────────────────────────────────────────────────────────
    property real value: 0
    property real maxValue: 100
    property string unit: "%"
    property color gaugeColor: ThemeEngine.colors.success
    // 5WHY (R2): metrics that are semantically invalid at negative values
    // (e.g. daysLeft for an expired certificate) show this localized label
    // instead of a confusing negative number + 0% fill.
    property string emptyLabel: ""

    implicitWidth: 240
    // 5WHY: implicitHeight must report the TRUE content height (value row 24
    // + spacing 6 + bar 8 + spacing 6 + pct label ~14 = ~58px) with a small
    // breathing room.  ResultChart now sizes its container to this value, so
    // an under-reported 56px would clip the gauge; the old fixed 80px chart
    // area instead left ~22px of dead space on G3+ detail pages.
    implicitHeight: 64

    // 5WHY: _ratio is clamped to [0, 1] to prevent negative bar widths
    // (when value < 0) and overflows (when value > maxValue).  Without
    // clamping, a Math.max(..., 0) on bar width would look correct
    // visually but break the animation's to-value, causing a jarring snap.
    readonly property real _ratio: Math.max(0, Math.min(1, maxValue > 0 ? value / maxValue : 0))

    // 5WHY: _animWidth is the NumberAnimation target — drives bar width
    // via binding.  When root._ratio changes, the animation runs from
    // the current _animWidth to the target fraction of barTrack.width.
    // This gives smooth fill/unfill transitions for live-updating data
    // (e.g. packet loss climbing from 0% to 2.3% during a test).
    property real _animWidth: 0

    // Central animation trigger — re-animates on ratio change or track resize
    function _retarget() {
        if (barTrack.width > 0) {
            anim.from = _animWidth
            anim.to = _ratio * barTrack.width
            anim.start()
        }
    }
    on_ratioChanged: _retarget()

    // Defer initial animation until layout completes and barTrack has width
    Component.onCompleted: {
        Qt.callLater(function() {
            if (barTrack.width > 0) {
                _animWidth = _ratio * barTrack.width
            }
        })
    }

    // Re-animate on container resize so bar width stays proportional
    Connections {
        target: barTrack
        function onWidthChanged() {
            if (barTrack.width > 0 && !anim.running) {
                _retarget()
            }
        }
    }

    NumberAnimation {
        id: anim
        target: root
        property: "_animWidth"
        duration: 500
        easing.type: Easing.OutCubic
    }

    // ── Visual ────────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        // ── Track label row: value/maxValue + unit ────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            Label {
                text: root.value < 0 && root.emptyLabel !== "" ? root.emptyLabel : root.value.toFixed(1)
                font.family: ThemeEngine.monoFont
                font.pixelSize: 18
                font.weight: Font.Bold
                color: root.gaugeColor
                horizontalAlignment: T.textAlignEnd
            }

            Label {
                visible: !(root.value < 0 && root.emptyLabel !== "")
                text: "/ " + root.maxValue + " " + root.unit
                font.family: ThemeEngine.monoFont
                font.pixelSize: 12
                font.weight: Font.Medium
                color: ThemeEngine.colors.onSurfaceVariant
                verticalAlignment: Text.AlignBottom
                bottomPadding: 2
            }

            Item { Layout.fillWidth: true }
        }

        // ── Bar track ─────────────────────────────────────────────────────
        // 5WHY: The track must be a separate Rectangle from the fill bar
        // because the fill bar's width is animated.  If the track and fill
        // were the same Rectangle with a gradient, the gradient would stretch
        // with the width animation, creating a "squashed gradient" effect.
        // Separate track (full-width, muted bg) + fill bar (animated width)
        // gives a clean "progress fill" appearance.
        Rectangle {
            id: barTrack
            Layout.fillWidth: true
            Layout.preferredHeight: 8
            radius: 4
            color: ThemeEngine.colors.surfaceContainerHighest

            // ── Fill bar ──────────────────────────────────────────────────
            Rectangle {
                id: barFill
                anchors {
                    left: parent.left
                    top: parent.top
                    bottom: parent.bottom
                }
                // 5WHY: width is bound to _animWidth with a clamp — even
                // before layout completes (barTrack.width === 0), the bar
                // won't render negative or impossibly wide.  Once layout
                // settles, on_ratioChanged triggers the animation to the
                // correct proportional width.
                width: Math.max(0, Math.min(barTrack.width, root._animWidth))
                radius: barTrack.radius
                color: root.gaugeColor

                // 5WHY: ColorAnimation on the fill bar provides a smooth
                // transition when gaugeColor changes (e.g. packet loss moves
                // from success → warning → fail as it climbs).
                // Without this, the bar snaps to the new color immediately,
                // which is jarring for live-updating metrics.
                Behavior on color {
                    ColorAnimation { duration: 300 }
                }
            }
        }

        // ── Percentage label (bottom-right, muted) ─────────────────────────
        Label {
            Layout.alignment: Qt.AlignRight
            text: Math.round(root._ratio * 100) + "%"
            font.family: ThemeEngine.monoFont
            font.pixelSize: 10
            color: ThemeEngine.colors.textMuted
        }
    }

    // ── Accessibility ─────────────────────────────────────────────────────
    Accessible.name: (root.value < 0 && root.emptyLabel !== "" ? root.emptyLabel : root.value.toFixed(1))
        + " " + root.maxValue + " " + root.unit
        + " (" + Math.round(root._ratio * 100) + "%)"
    Accessible.role: Accessible.Indicator
}
