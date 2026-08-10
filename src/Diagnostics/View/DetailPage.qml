import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
// 5WHY: source-tree-relative paths (../../Common/View/...) resolve on disk
// but NOT from QRC aliases (qrc:/qml/screens/).  Use single-level "../" which
// maps to the qml/ QRC prefix: ../theme → qrc:/qml/theme, ../widgets → qrc:/qml/widgets.
// Matches DiagnosticScreen.qml and all other screens in qml/screens/.
import "../theme" as T
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
        ? (T.ThemeEngine.statusColors[status] || T.ThemeEngine.colors.skipGray)
        : T.ThemeEngine.colors.textSecondary

    background: Rectangle { color: T.ThemeEngine.colors.surface }

    // ── Header bar ───────────────────────────────────────────────────────
    header: ToolBar {
        id: topBar
        background: Rectangle { color: T.ThemeEngine.colors.card }
        // Bottom separator
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 1; color: T.ThemeEngine.colors.borderCard
        }

        RowLayout {
            anchors { fill: parent; leftMargin: 8; rightMargin: 12 }
            spacing: 4

            // Back button
            ToolButton {
                id: backBtn
                icon.source: "qrc:/icons/ffffff/chevron-right.svg"
                icon.color: T.ThemeEngine.colors.textSecondary
                // 5WHY: mirror in RTL for consistent "back along reading direction"
                LayoutMirroring.enabled: T.T ? T.T.isRtl : false
                onClicked: { page.StackView.view.pop() }
                Accessible.name: T.T ? T.T.tr("accBack") : "Back"
            }

            W.AppIcon {
                name: page.diagId >= 0 ? appState.diagIconName(page.diagId) : "circle"
                size: 22; color: page._statusColor
            }

            Label {
                text: T.T ? T.T.diagName(page.diagId) : (page.detail.displayName || "")
                font.family: T.ThemeEngine.monoFont
                font.pixelSize: 16; font.weight: Font.DemiBold
                color: T.ThemeEngine.colors.textPrimary
                Layout.fillWidth: true; elide: Text.ElideRight
            }

            // Status badge
            Rectangle {
                radius: 4; implicitWidth: statusText.implicitWidth + 12
                implicitHeight: 22
                color: Qt.alpha(page._statusColor, 0.12)
                Label {
                    id: statusText; anchors.centerIn: parent
                    text: T.ThemeEngine.statusIconNames[page.status] ? T.ThemeEngine.statusIconNames[page.status].replace("badge-","") : ""
                    font.family: T.ThemeEngine.monoFont; font.pixelSize: 11; font.weight: Font.Bold
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
                radius: T.ThemeEngine.radius.lg
                color: T.ThemeEngine.colors.card
                border { width: 1; color: T.ThemeEngine.colors.borderCard }

                W.AppIcon {
                    anchors.centerIn: parent
                    name: page.diagId >= 0 ? appState.diagIconName(page.diagId) : "circle"
                    size: 72; color: page._statusColor
                }

                // Completion badge overlay
                W.AppIcon {
                    anchors { right: parent.right; bottom: parent.bottom; margins: 12 }
                    name: T.ThemeEngine.statusIconNames[page.status] || "badge-skip"
                    size: 28; color: page._statusColor
                }
            }

            // ── Key metric card ──────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                radius: T.ThemeEngine.radius.md
                color: T.ThemeEngine.colors.card
                border { width: 1; color: T.ThemeEngine.colors.borderCard }

                RowLayout {
                    anchors { centerIn: parent; margins: 12 }
                    spacing: 8
                    Label {
                        text: _keyMetricValue; font.family: T.ThemeEngine.monoFont
                        font.pixelSize: 28; font.weight: Font.Bold
                        color: page._statusColor
                    }
                    Label {
                        text: _keyMetricLabel; font.family: T.ThemeEngine.monoFont
                        font.pixelSize: 13; color: T.ThemeEngine.colors.textSecondary
                        Layout.alignment: Qt.AlignBottom; Layout.bottomMargin: 6
                    }
                }
            }

            // ── Summary ──────────────────────────────────────────────────
            Label {
                Layout.fillWidth: true
                text: page.detail.summary || ""
                font.family: T.ThemeEngine.monoFont; font.pixelSize: 12
                color: T.ThemeEngine.colors.textSecondary
                wrapMode: Text.WordWrap; visible: text !== ""
            }

            // ── Properties section (collapsible) ─────────────────────────
            Rectangle {
                Layout.fillWidth: true; implicitHeight: propsCol.implicitHeight + 16
                radius: T.ThemeEngine.radius.md
                color: T.ThemeEngine.colors.card
                border { width: 1; color: T.ThemeEngine.colors.borderCard }

                ColumnLayout {
                    id: propsCol
                    anchors { fill: parent; margins: 12 }; spacing: 4

                    // Section header (tap to toggle)
                    RowLayout {
                        Label {
                            text: "Properties"; font.family: T.ThemeEngine.monoFont
                            font.pixelSize: 12; font.weight: Font.Bold; color: T.ThemeEngine.colors.textPrimary
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: propsExpanded ? "▲" : "▼"
                            font.pixelSize: 10; color: T.ThemeEngine.colors.textSecondary
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: propsExpanded = !propsExpanded
                            cursorShape: Qt.PointingHandCursor
                        }
                    }

                    // Property rows
                    ColumnLayout {
                        visible: propsExpanded; spacing: 2
                        Repeater {
                            model: page.detail.properties || []
                            delegate: RowLayout {
                                spacing: 8
                                Label {
                                    text: modelData.label + ":"; font.family: T.ThemeEngine.monoFont
                                    font.pixelSize: 11; color: T.ThemeEngine.colors.textMuted
                                    Layout.preferredWidth: Math.min(implicitWidth, 140)
                                    elide: Text.ElideRight
                                }
                                Label {
                                    text: modelData.value || ""; font.family: T.ThemeEngine.monoFont
                                    font.pixelSize: 11; color: T.ThemeEngine.colors.textPrimary
                                    Layout.fillWidth: true; wrapMode: Text.WrapAnywhere
                                }
                            }
                        }
                    }
                }
            }
            property bool propsExpanded: true

            // ── Detailed Data section (charts, default collapsed) ────────
            Rectangle {
                Layout.fillWidth: true; implicitHeight: chartsCol.implicitHeight + 16
                radius: T.ThemeEngine.radius.md
                color: T.ThemeEngine.colors.card
                border { width: 1; color: T.ThemeEngine.colors.borderCard }
                visible: _hasChartData

                ColumnLayout {
                    id: chartsCol
                    anchors { fill: parent; margins: 12 }; spacing: 8

                    RowLayout {
                        Label {
                            text: "Detailed Data"; font.family: T.ThemeEngine.monoFont
                            font.pixelSize: 12; font.weight: Font.Bold; color: T.ThemeEngine.colors.textPrimary
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: chartsExpanded ? "▲" : "▼"; font.pixelSize: 10
                            color: T.ThemeEngine.colors.textSecondary
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: chartsExpanded = !chartsExpanded
                            cursorShape: Qt.PointingHandCursor
                        }
                    }

                    // Chart area — content varies by template
                    Loader {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 120
                        active: chartsExpanded
                        source: ""  // P5: load template-specific chart
                        visible: active
                    }
                }
            }
            property bool chartsExpanded: false

            // ── Terminal output ──────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true; implicitHeight: termBlock.implicitHeight + 16
                radius: T.ThemeEngine.radius.md
                color: "#0F172A"  // terminal bg — always dark for readability
                border { width: 1; color: T.ThemeEngine.colors.borderCard }

                ColumnLayout {
                    anchors { fill: parent; margins: 12 }; spacing: 6
                    Label {
                        text: "Terminal Output"; font.family: T.ThemeEngine.monoFont
                        font.pixelSize: 11; font.weight: Font.Bold
                        color: T.ThemeEngine.colors.passGreen
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
                                item.typewriter = true
                            }
                        }
                    }
                }
            }
        }  // bodyColumn
    }  // Flickable

    // ── Language change handler ──────────────────────────────────────────
    Connections {
        target: T.T
        function onLangChanged() {
            // Force re-evaluation of translated bindings
            // All text is bound via T.T.diagName() etc — QML re-evaluates
            // bindings when their dependencies change.
        }
    }

    // ── Key metric helpers ───────────────────────────────────────────────
    readonly property string _keyMetricValue: {
        if (!_hasData) {
            var d = detail.durationMs || 0
            return d > 0 ? ThemeEngine.formatDuration(d).replace(/[a-z]/gi,'').trim() : "--"
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
}
