import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
// 5WHY: source-tree-relative paths (../../Common/View/...) resolve on disk
// but NOT from QRC aliases (qrc:/qml/screens/).  Use single-level "../" which
// maps to the qml/ QRC prefix: ../theme → qrc:/qml/theme, ../widgets → qrc:/qml/widgets.
// Matches DiagnosticScreen.qml and all other screens in qml/screens/.
import "../theme" as Th
import "../widgets" as W
import "../detail" as D

// ── DetailPage.qml — Living Diagnostics L5 full-screen detail page ─────
// Replaces the old overlay-based detail view.  Pushed onto AppContent's
// StackView when a DiagBlock is tapped.  6 template variants for different
// diagnostic types, sharing a unified page framework (D15: unified frame).
// D16: charts default-collapsed in "Detailed Data" expandable section.
//
// Navigation: ← back button + right-swipe gesture (D10). StackView slide
// transition is inherited from AppContent's push/pop transitions (D14).

Page {
    id: page

    // ── Public properties ────────────────────────────────────────────────
    property var detail: ({})  // from appState.getDetailResult(diagId)
    // 5WHY: detail.diagId || -1 treats 0 (G1NetworkAdapters, first enum
    // value) as falsy — JS coerces 0 to -1, showing wrong icon/title.
    property int diagId: detail.diagId !== undefined ? detail.diagId : -1
    property int status: detail.status !== undefined ? detail.status : -1
    property var data: detail.data || {}

    // Single source: avoids 3x Object.keys() calls in _template/_keyMetric*
    readonly property bool _hasData: data && Object.keys(data).length > 0

    // Template classification from C++ DiagTemplateType (0=Ping,1=Path,...5=System).
    // 5WHY: old duck-typing inferred template from data key existence (e.g.
    // daysLeft→handshake, dnsMs→request).  A DHCP diagnostic with a daysLeft
    // field would misclassify as SSL.  Now the diagnostic declares its template
    // type via diagTemplateType(DiagId) — single source of truth, zero ambiguity.
    readonly property string _template: {
        if (!_hasData) return "system"
        var tt = data.templateType
        if (tt === 0) return "ping"
        if (tt === 1) return "path"
        if (tt === 2) return "handshake"
        if (tt === 3) return "request"
        if (tt === 4) return "query"
        return "system"
    }

    readonly property color _statusColor: status >= 0
        ? (Th.ThemeEngine.statusColors[status] || Th.ThemeEngine.colors.skipGray)
        : Th.ThemeEngine.colors.textSecondary

    // 5WHY (review B16): expand/collapse flags were declared mid-body inside
    // bodyColumn's child list, mixed with layout items — moved to the page
    // root next to the other state for readability and single ownership.
    property bool propsExpanded: true
    property bool chartsExpanded: false

    background: Rectangle { color: Th.ThemeEngine.colors.surface }

    // ── Header bar ───────────────────────────────────────────────────────
    header: ToolBar {
        id: topBar
        background: Rectangle { color: Th.ThemeEngine.colors.card }
        // Bottom separator
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 1; color: Th.ThemeEngine.colors.borderCard
        }

        RowLayout {
            anchors { fill: parent; leftMargin: 8; rightMargin: 12 }
            spacing: 4

            // Back button
            ToolButton {
                id: backBtn
                // 5WHY (review B18): native icon.source + icon.color performs
                // RUNTIME colorization — unreliable on iOS static Qt (see
                // AppIcon 5WHY), leaving a white chevron invisible on light
                // theme.  Use the project's pre-generated AppIcon; mirror
                // flips it along the reading direction in RTL.
                contentItem: W.AppIcon {
                    name: "chevron-right"; size: 18
                    color: Th.ThemeEngine.colors.textSecondary
                    mirror: T.isRtl
                }
                implicitWidth: 36; implicitHeight: 36
                onClicked: { page.StackView.view.pop() }
                Accessible.name: T.tr("accBack")
            }

            W.AppIcon {
                name: page.diagId >= 0 ? appState.diagIconName(page.diagId) : "circle"
                size: 22; color: page._statusColor
            }

            Label {
                text: T.diagName(page.diagId) || page.detail.displayName || ""
                font.family: Th.ThemeEngine.monoFont
                font.pixelSize: 16; font.weight: Font.DemiBold
                color: Th.ThemeEngine.colors.textPrimary
                Layout.fillWidth: true; elide: Text.ElideRight
            }

            // Status badge
            Rectangle {
                radius: 4; implicitWidth: statusText.implicitWidth + 12
                implicitHeight: 22
                color: Qt.alpha(page._statusColor, 0.12)
                Label {
                    id: statusText; anchors.centerIn: parent
                    text: Th.ThemeEngine.statusIconNames[page.status] ? Th.ThemeEngine.statusIconNames[page.status].replace("badge-","") : ""
                    font.family: Th.ThemeEngine.monoFont; font.pixelSize: 11; font.weight: Font.Bold
                    color: page._statusColor
                }
            }
        }
    }

    // ── Scrollable body ──────────────────────────────────────────────────
    Flickable {
        anchors.fill: parent
        contentHeight: bodyColumn.implicitHeight + 32
        clip: true
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: bodyColumn
            anchors { left: parent.left; right: parent.right; top: parent.top }
            anchors.margins: 16; spacing: 12

            // ── Hero area ───────────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                radius: Th.ThemeEngine.radius.lg
                color: Th.ThemeEngine.colors.card
                border { width: 1; color: Th.ThemeEngine.colors.borderCard }

                W.AppIcon {
                    anchors.centerIn: parent
                    name: page.diagId >= 0 ? appState.diagIconName(page.diagId) : "circle"
                    size: 72; color: page._statusColor
                }

                // Completion badge overlay
                W.AppIcon {
                    anchors { right: parent.right; bottom: parent.bottom; margins: 12 }
                    name: Th.ThemeEngine.statusIconNames[page.status] || "badge-skip"
                    size: 28; color: page._statusColor
                }
            }

            // ── Key metric card ──────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                radius: Th.ThemeEngine.radius.md
                color: Th.ThemeEngine.colors.card
                border { width: 1; color: Th.ThemeEngine.colors.borderCard }

                RowLayout {
                    anchors { centerIn: parent; margins: 12 }
                    spacing: 8
                    Label {
                        text: _keyMetricValue; font.family: Th.ThemeEngine.monoFont
                        font.pixelSize: 28; font.weight: Font.Bold
                        color: page._statusColor
                    }
                    Label {
                        text: _keyMetricLabel; font.family: Th.ThemeEngine.monoFont
                        font.pixelSize: 13; color: Th.ThemeEngine.colors.textSecondary
                        Layout.alignment: Qt.AlignBottom; Layout.bottomMargin: 6
                    }
                }
            }

            // ── Summary ──────────────────────────────────────────────────
            Label {
                Layout.fillWidth: true
                text: page.detail.summary || ""
                font.family: Th.ThemeEngine.monoFont; font.pixelSize: 12
                color: Th.ThemeEngine.colors.textSecondary
                wrapMode: Text.WordWrap; visible: text !== ""
            }

            // ── Properties section (collapsible) ─────────────────────────
            Rectangle {
                Layout.fillWidth: true; implicitHeight: propsCol.implicitHeight + 16
                radius: Th.ThemeEngine.radius.md
                color: Th.ThemeEngine.colors.card
                border { width: 1; color: Th.ThemeEngine.colors.borderCard }

                ColumnLayout {
                    id: propsCol
                    anchors { fill: parent; margins: 12 }
                    spacing: 4

                    // Section header (tap to toggle)
                    RowLayout {
                        Label {
                            text: "Properties"; font.family: Th.ThemeEngine.monoFont
                            font.pixelSize: 12; font.weight: Font.Bold; color: Th.ThemeEngine.colors.textPrimary
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: page.propsExpanded ? "▲" : "▼"
                            font.pixelSize: 10; color: Th.ThemeEngine.colors.textSecondary
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: page.propsExpanded = !page.propsExpanded
                            cursorShape: Qt.PointingHandCursor
                        }
                    }

                    // Property rows
                    ColumnLayout {
                        visible: page.propsExpanded; spacing: 2
                        Repeater {
                            model: page.detail.properties || []
                            delegate: RowLayout {
                                spacing: 8
                                Label {
                                    text: modelData.label + ":"; font.family: Th.ThemeEngine.monoFont
                                    font.pixelSize: 11; color: Th.ThemeEngine.colors.textMuted
                                    Layout.preferredWidth: Math.min(implicitWidth, 140)
                                    elide: Text.ElideRight
                                }
                                Label {
                                    text: modelData.value || ""; font.family: Th.ThemeEngine.monoFont
                                    font.pixelSize: 11; color: Th.ThemeEngine.colors.textPrimary
                                    Layout.fillWidth: true; wrapMode: Text.WrapAnywhere
                                }
                            }
                        }
                    }
                }
            }

            // ── Detailed Data section (charts, default collapsed) ────────
            Rectangle {
                Layout.fillWidth: true; implicitHeight: chartsCol.implicitHeight + 16
                radius: Th.ThemeEngine.radius.md
                color: Th.ThemeEngine.colors.card
                border { width: 1; color: Th.ThemeEngine.colors.borderCard }
                // 5WHY (review B4): gate on _chartSource (a real chart exists
                // for this template) — not just _hasChartData, which is true
                // even when no visualization is wired for the template.
                visible: _hasChartData && _chartSource !== ""

                ColumnLayout {
                    id: chartsCol
                    anchors { fill: parent; margins: 12 }
                    spacing: 8

                    RowLayout {
                        Label {
                            text: "Detailed Data"; font.family: Th.ThemeEngine.monoFont
                            font.pixelSize: 12; font.weight: Font.Bold; color: Th.ThemeEngine.colors.textPrimary
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: page.chartsExpanded ? "▲" : "▼"; font.pixelSize: 10
                            color: Th.ThemeEngine.colors.textSecondary
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: page.chartsExpanded = !page.chartsExpanded
                            cursorShape: Qt.PointingHandCursor
                        }
                    }

                    // Chart area — content varies by template (review B4:
                    // BarChart series / Gauge metric wired from structured data.
                    // MetricCard/BarChart/Gauge were orphaned — now connected).
                    Loader {
                        id: chartLoader
                        Layout.fillWidth: true
                        Layout.preferredHeight: 140
                        active: page.chartsExpanded && page._chartSource !== ""
                        source: page._chartSource
                        visible: active
                        onLoaded: { if (item) page._bindChart(item) }
                    }
                }
            }

            // ── Terminal output ──────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true; implicitHeight: termBlock.implicitHeight + 16
                radius: Th.ThemeEngine.radius.md
                color: "#0F172A"  // terminal bg — always dark for readability
                border { width: 1; color: Th.ThemeEngine.colors.borderCard }

                ColumnLayout {
                    anchors { fill: parent; margins: 12 }
                    spacing: 6
                    Label {
                        text: "Terminal Output"; font.family: Th.ThemeEngine.monoFont
                        font.pixelSize: 11; font.weight: Font.Bold
                        color: Th.ThemeEngine.colors.passGreen
                    }
                    // L5: TerminalBlock with typewriter animation
                    Loader {
                        id: termLoader
                        Layout.fillWidth: true
                        Layout.preferredHeight: 200
                        active: (page.detail.details || page.detail.rawOutput || "") !== ""
                        source: "qrc:/qml/detail/TerminalBlock.qml"
                        onLoaded: {
                            if (item) {
                                item.text = Qt.binding(function() {
                                    return page.detail.details || page.detail.rawOutput || ""
                                })
                                // 5WHY (review B17): typewriter types at 40ms/char
                                // — multi-KB terminal output (long traceroutes,
                                // verbose HTTP) would take minutes to animate.
                                // Cap the animation to concise output; large
                                // payloads render instantly.
                                item.typewriter = (page.detail.details || page.detail.rawOutput || "").length < 2000
                            }
                        }
                    }
                }
            }
        }  // bodyColumn
    }  // Flickable

    // ── Language change handler ──────────────────────────────────────────
    Connections {
        target: T
        function onLangChanged() {
            // Force re-evaluation of translated bindings
            // All text is bound via T.diagName() etc — QML re-evaluates
            // bindings when their dependencies change.
        }
    }

    // ── Key metric helpers ───────────────────────────────────────────────
    readonly property string _keyMetricValue: {
        if (!_hasData) {
            var d = detail.durationMs || 0
            return d > 0 ? Th.ThemeEngine.formatDuration(d).replace(/[a-z]/gi,'').trim() : "--"
        }
        if (data.rttAvgMs !== undefined)  return Number(data.rttAvgMs).toFixed(0)
        if (data.hopCount !== undefined)  return String(data.hopCount)
        if (data.totalMs !== undefined)   return Number(data.totalMs).toFixed(0)
        if (data.daysLeft !== undefined)  return String(data.daysLeft)
        if (data.lossPercent !== undefined) return Number(data.lossPercent).toFixed(1)
        return "--"
    }
    readonly property string _keyMetricLabel: {
        if (!_hasData) return "ms"
        if (data.rttAvgMs !== undefined)  return "ms avg"
        if (data.hopCount !== undefined)  return "hops"
        if (data.totalMs !== undefined)   return "ms total"
        if (data.daysLeft !== undefined)  return "days"
        if (data.lossPercent !== undefined) return "% loss"
        return ""
    }
    readonly property bool _hasChartData: {
        if (!data) return false
        return data.individualRtts !== undefined
            || data.hops !== undefined
            || data.dnsMs !== undefined
            || data.downloadResults !== undefined
    }

    // ── L5 chart wiring (review B4) ────────────────────────────────────
    // Visualizations were described in the living-doc but never connected:
    // MetricCard/BarChart/Gauge sat qrc-registered and orphaned, and the
    // chart Loader had an empty source.  Now each template maps to a real
    // viz component and its data is bound on load.
    readonly property string _chartSource: {
        if (!_hasChartData) return ""
        var tt = data.templateType
        if (tt === 0 && data.individualRtts !== undefined) return "qrc:/qml/detail/viz/BarChart.qml"
        if (tt === 1 && data.hops !== undefined)           return "qrc:/qml/detail/viz/BarChart.qml"
        if (tt === 2 && data.daysLeft !== undefined)       return "qrc:/qml/detail/viz/Gauge.qml"
        if (tt === 3 && data.dnsMs !== undefined)          return "qrc:/qml/detail/viz/BarChart.qml"
        return ""
    }
    // BarChart { values: [{label,value,color}] } — RTT per packet / hop
    // delays / HTTP-timing waterfall.
    readonly property var _chartSeries: {
        var out = []
        var tt = data.templateType
        var P = Th.ThemeEngine.colors.primary
        if (tt === 0 && data.individualRtts) {
            for (var i = 0; i < data.individualRtts.length; i++)
                out.push({ label: "p" + (i + 1), value: Number(data.individualRtts[i]), color: P })
        } else if (tt === 1 && data.hops) {
            for (var j = 0; j < data.hops.length; j++) {
                var h = data.hops[j]
                out.push({ label: String(h.ttl !== undefined ? h.ttl : j + 1),
                           value: Number(h.rttMs !== undefined ? h.rttMs : 0), color: P })
            }
        } else if (tt === 3) {
            var phases = [
                { label: "DNS",   value: data.dnsMs,       color: "#0EA5E9" },
                { label: "TCP",   value: data.connectMs,   color: "#6366F1" },
                { label: "TLS",   value: data.sslMs,       color: "#10B981" },
                { label: "TTFB",  value: data.firstByteMs, color: "#F59E0B" },
                { label: "Total", value: data.totalMs,     color: "#F43F5E" }
            ]
            for (var k = 0; k < phases.length; k++)
                if (phases[k].value !== undefined) out.push(phases[k])
        }
        return out
    }
    // Gauge { value, maxValue, unit, gaugeColor } — cert validity days.
    readonly property var _gaugeSpec: {
        if (data.templateType === 2 && data.daysLeft !== undefined)
            return { value: Number(data.daysLeft), max: 365, unit: "d",
                     color: Th.ThemeEngine.colors.passGreen }
        return null
    }
    function _bindChart(item) {
        if (!item) return
        if (_chartSource.indexOf("BarChart") >= 0) {
            if (_chartSeries.length) item.values = _chartSeries
        } else if (_gaugeSpec) {
            item.value = _gaugeSpec.value
            item.maxValue = _gaugeSpec.max
            item.unit = _gaugeSpec.unit
            item.gaugeColor = _gaugeSpec.color
        }
    }
}
