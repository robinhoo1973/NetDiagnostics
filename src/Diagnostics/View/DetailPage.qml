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
import "../detail/KeyMetric.js" as KM
import "../detail/viz" as Viz

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
    // 5WHY: named resultData — `data` is Item's RESERVED default property
    // (the list of child objects).  Shadowing it compiles and runs but is
    // fragile: qmllint resolves `data` to QQmlListProperty<QObject> and any
    // future child-collection code could collide.  A distinct name removes
    // the ambiguity entirely.
    property var resultData: detail.data || {}

    // Template classification comes from C++ (DiagTemplateType injected into
    // resultData.templateType by ResultsModel) — no QML duck-typing needed.

    readonly property color _statusColor: status >= 0
        ? (Th.ThemeEngine.statusColors[status] || Th.ThemeEngine.colors.skipGray)
        : Th.ThemeEngine.colors.textSecondary

    // 5WHY (review B16): expand/collapse flags were declared mid-body inside
    // bodyColumn's child list, mixed with layout items — moved to the page
    // root next to the other state for readability and single ownership.
    property bool propsExpanded: true
    property bool chartsExpanded: false

    // Copy-feedback toast state
    property string _copied: ""
    Timer { id: copyTimer; interval: 1600; onTriggered: page._copied = "" }

    // 5WHY: status→translation-key was a POSITIONAL array — coupling the
    // DiagStatus enum ORDER to array index.  An explicit map makes the
    // coupling visible and unknown statuses degrade to "" instead of
    // mislabelling the next enum value.
    readonly property string _statusText: {
        var keys = { 0: "summaryPass", 1: "summaryWarning", 2: "summaryFail",
                     3: "summarySkipped", 4: "errorStatus", 5: "summaryInfo" }
        return keys[page.status] ? T.tr(keys[page.status]) : ""
    }
    // Hero meta line: tested host · duration (previously only in terminal text).
    readonly property string _metaLine: {
        var parts = []
        var host = page.resultData.target || page.resultData.host || ""
        if (host) parts.push(String(host))
        var dur = page.detail.durationMs || 0
        if (dur > 0) parts.push(KM.formatDuration(dur))
        return parts.join(" · ")
    }
    // 5WHY (P2): charts default-collapsed hid the visualizations.  Compact
    // series (≤8 bars) start expanded; long traceroutes stay collapsed.
    // Set once on open — data is static for the page lifetime.
    Component.onCompleted: { page.chartsExpanded = chartView.seriesCount <= 8 }

    // Consolidate repeated compound expressions — computed once, read many times
    readonly property bool _hasTerminalOutput: (page.detail.details || page.detail.rawOutput || "") !== ""
    readonly property bool _hasErrorOutput: (page.detail.errorOutput || "") !== ""
    readonly property bool _hasProperties: (page.detail.properties || []).length > 0
    readonly property int _terminalLines: {
        if (!_hasTerminalOutput) return 0
        var txt = page.detail.details || page.detail.rawOutput || ""
        var count = 0
        for (var i = 0; i < txt.length; i++)
            if (txt[i] === '\n') count++
        return count + 1  // lines = newlines + 1
    }

    background: Rectangle { color: Th.ThemeEngine.colors.surface }

    // ── Header — single-row title bar ──────────────────────────────────
    // 5WHY (header-body overlap): the old two-row header (icon+name+status
    // stacked) was ~70px tall. Page.contentItem auto-offsets below the
    // header, but the ToolBar default implicitHeight adapter sometimes
    // mis-sizes on iOS/Android static builds.  Keep it flat and compact:
    //   <| test name (left)  ···  duration | COPY | RESTART
    // All result identity (icon, status, summary) lives in the body Hero.
    header: ToolBar {
        id: topBar
        // Explicit compact height — single row, no platform variance
        topPadding: 0; bottomPadding: 0; leftPadding: 4; rightPadding: 8
        implicitHeight: 42
        background: Rectangle { color: Th.ThemeEngine.colors.card }

        RowLayout {
            anchors { fill: parent; leftMargin: 2; rightMargin: 4 }
            spacing: 4

            // Back
            ToolButton {
                id: backBtn
                implicitWidth: 34; implicitHeight: 34
                contentItem: W.AppIcon {
                    name: "chevron-right"; size: 16
                    color: Th.ThemeEngine.colors.textSecondary
                    mirror: T.isRtl
                }
                onClicked: { page.StackView.view.pop() }
                Accessible.name: T.tr("accBack")
            }

            // Test name — left-aligned, fills space
            Label {
                Layout.fillWidth: true
                text: T.diagName(page.diagId) || page.detail.displayName || ""
                font.family: Th.ThemeEngine.monoFont
                font.pixelSize: 14; font.weight: Font.DemiBold
                color: Th.ThemeEngine.colors.textPrimary
                elide: Text.ElideRight; maximumLineCount: 1
            }

            // Duration
            Label {
                text: KM.formatDuration(page.detail.durationMs || 0)
                visible: (page.detail.durationMs || 0) > 0
                font.family: Th.ThemeEngine.monoFont; font.pixelSize: 11
                color: Th.ThemeEngine.colors.textSecondary
            }

            // | separator
            Rectangle {
                implicitWidth: 1; implicitHeight: 16
                color: Th.ThemeEngine.colors.borderCard
            }

            // Copy
            ToolButton {
                id: copyBtn2
                implicitWidth: 32; implicitHeight: 32
                contentItem: W.AppIcon {
                    name: "clipboard"; size: 14
                    color: Th.ThemeEngine.colors.textSecondary
                }
                onClicked: {
                    appState.copyDetailToClipboard(page.diagId)
                    page._copied = T.tr("detailCopied")
                    copyTimer.restart()
                }
                Accessible.name: T.tr("detailCopy")
            }

            // Restart
            ToolButton {
                id: rerunBtn2
                implicitWidth: 32; implicitHeight: 32
                contentItem: W.AppIcon {
                    name: "diagnostics"; size: 14
                    color: Th.ThemeEngine.colors.textSecondary
                }
                onClicked: appState.rerunDiag(page.diagId)
                Accessible.name: T.tr("detailRerun")
            }
        }
    }

    // ── Scrollable body ──────────────────────────────────────────────────
    // 5WHY (scrolling): ColumnLayout inside Flickable works correctly when
    // contentHeight tracks implicitHeight reactively.  The old code set
    // contentHeight in the declaration (one-shot snapshot of 0) → no scroll.
    // Now uses a binding that updates as children load.
    Flickable {
        id: bodyFlick
        anchors { fill: parent; topMargin: 0 }
        contentWidth: width
        contentHeight: bodyColumn.implicitHeight + 20
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: bodyColumn
            width: bodyFlick.width - 32
            x: 16
            // 5WHY (spacing hierarchy): a uniform spacing:10 treats every
            // section as equal-weight — no visual distinction between a
            // subordinate MetricCard and a major section boundary.  Switched
            // to spacing:0 with explicit per-item Layout.topMargin so the
            // layout reads as three visual groups:
            //   [16px header-body gap] Hero — MetricCard(8px) — Error(8px)
            //   [16px boundary]       Properties
            //   [16px boundary]       Charts
            //   [16px boundary]       Terminal
            // Theme-adaptive: values reference ThemeEngine.spacing tokens
            // (sm=8, lg=16) rather than hardcoded numbers.
            spacing: 0

            // Header-body breathing room — 16dp below the 42px ToolBar header.
            // MD3 recommends ≥16dp content padding below app bars; Apple HIG
            // recommends ≥8pt.  16dp satisfies both and matches ThemeEngine.spacing.lg.
            Item { Layout.preferredHeight: Th.ThemeEngine.spacing.lg }

            // ── Hero: result headline card (P2) ───────────────────────
            // 5WHY: the old hero was a 120px decorative icon slab duplicating
            // the header icon and carrying zero information.  Now it is the
            // page's result headline: status, summary, target/duration.
            // 5WHY (format fix): a FIXED 96px height CLIPPED multi-line
            // summaries (e.g. DNS listing 6 addresses).  Height is now
            // content-driven (heroRow.implicitHeight + 32 margins, min 96).
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(96, heroRow.implicitHeight + 32)
                radius: Th.ThemeEngine.radius.lg
                color: Th.ThemeEngine.colors.card
                border { width: 1; color: Th.ThemeEngine.colors.borderCard }

                RowLayout {
                    id: heroRow
                    anchors { fill: parent; margins: 16 }
                    spacing: 14

                    // Status icon in a tinted disc
                    Rectangle {
                        implicitWidth: 56; implicitHeight: 56; radius: 28
                        color: Qt.alpha(page._statusColor, 0.12)
                        W.AppIcon {
                            anchors.centerIn: parent
                            name: Th.ThemeEngine.statusIconNames[page.status] || "badge-skip"
                            size: 30; color: page._statusColor
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        // Status text — localized, color-coded
                        Label {
                            text: page._statusText
                            font.family: Th.ThemeEngine.monoFont
                            font.pixelSize: 15; font.weight: Font.Bold
                            color: page._statusColor
                        }
                        // Summary
                        Label {
                            Layout.fillWidth: true
                            text: page.detail.summary || ""
                            font.family: Th.ThemeEngine.monoFont
                            font.pixelSize: 12
                            color: Th.ThemeEngine.colors.textPrimary
                            wrapMode: Text.WordWrap
                            visible: text !== ""
                        }
                        // Target · duration meta
                        Label {
                            Layout.fillWidth: true
                            visible: page._metaLine !== ""
                            text: page._metaLine
                            font.family: Th.ThemeEngine.monoFont
                            font.pixelSize: 11
                            color: Th.ThemeEngine.colors.textSecondary
                            elide: Text.ElideRight
                            maximumLineCount: 1
                        }
                    }
                }

                Accessible.name: page._statusText + (page.detail.summary ? ": " + page.detail.summary : "")
                Accessible.role: Accessible.Graphic
            }

            // ── Key metric card (MetricCard.qml with count-up animation) ──
            // 5WHY (format fix): the summary now lives ONLY in the hero — the
            // old standalone summary label AND the fallback card both
            // re-rendered page.detail.summary, showing the exact same text
            // twice on screen.  When no structured metric exists the hero's
            // status + summary IS the headline; no placeholder is needed.
            // 5WHY (layout): fixed 72px preferredHeight left an empty gap
            // when no metric exists — bind height to _keyMetric.ok so the
            // Loader collapses completely when inactive.
            // 5WHY (spacing collapse): Layout.topMargin was unconditional —
            // phantom 8px gap when MetricCard was hidden.  Bind to visibility.
            Loader {
                Layout.fillWidth: true
                Layout.preferredHeight: _keyMetric.ok ? 72 : 0
                Layout.topMargin: _keyMetric.ok ? Th.ThemeEngine.spacing.sm : 0
                visible: _keyMetric.ok
                // 5WHY: _keyMetric is now a structured object from the shared
                // KeyMetric module — no string parsing (parseFloat on "1:23"
                // corrupted durations) and no unit mangling ("% loss"→"loss").
                active: _keyMetric.ok
                source: "qrc:/qml/detail/MetricCard.qml"
                onLoaded: {
                    if (item) {
                        // Translated strings → binding (re-eval on lang change)
                        item.label = Qt.binding(function() { return T.tr(_keyMetric.labelKey) })
                        item.unit = Qt.binding(function() { return T.tr(_keyMetric.unitKey) })
                        // Static numbers → direct assignment (data is immutable)
                        item.value = _keyMetric.value
                        item.precision = _keyMetric.precision
                        item.format = _keyMetric.format
                        item.trailing = _keyMetric.trailing
                        item.accentColor = page._statusColor
                    }
                }
            }

            // ── Error output ───────────────────────────────────────────
            // 5WHY: errorOutput was serialized by C++ (getDetailResult) but
            // never rendered — failed tests with only an errorOutput lost
            // their error detail on the detail page.
            // 5WHY (spacing collapse): bind topMargin to visibility so the
            // error section's margin collapses when there is no error.
            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: _hasErrorOutput ? Th.ThemeEngine.spacing.sm : 0
                implicitHeight: errCol.implicitHeight + 24
                visible: _hasErrorOutput
                radius: Th.ThemeEngine.radius.md
                color: Qt.alpha(Th.ThemeEngine.colors.failRed, 0.06)
                border { width: 1; color: Qt.alpha(Th.ThemeEngine.colors.failRed, 0.5) }
                ColumnLayout {
                    id: errCol; anchors { fill: parent; margins: 12 }
                    spacing: 6
                    Label {
                        text: T.tr("detailError")
                        font.family: Th.ThemeEngine.monoFont; font.pixelSize: 12; font.weight: Font.Bold
                        color: Th.ThemeEngine.colors.failRed
                    }
                    Label {
                        Layout.fillWidth: true
                        text: page.detail.errorOutput
                        font.family: Th.ThemeEngine.monoFont; font.pixelSize: 11
                        color: Th.ThemeEngine.colors.textPrimary
                        wrapMode: Text.WrapAnywhere
                    }
                }
                Accessible.name: T.tr("detailError")
                Accessible.role: Accessible.StaticText
            }

            // ── Properties section (collapsible) ─────────────────────────
            // 5WHY (spacing collapse): bind topMargin to visibility so
            // phantom 16px gap does not appear when Properties are absent.
            Rectangle {
                Layout.fillWidth: true; implicitHeight: propsCol.implicitHeight + 16
                Layout.topMargin: _hasProperties ? Th.ThemeEngine.spacing.lg : 0
                radius: Th.ThemeEngine.radius.md
                color: Th.ThemeEngine.colors.card
                border { width: 1; color: Th.ThemeEngine.colors.borderCard }
                // 5WHY: several diagnostics emit no properties — the empty
                // section header was dead UI.  _hasProperties pre-computes this.
                visible: _hasProperties

                ColumnLayout {
                    id: propsCol
                    anchors { fill: parent; margins: 12 }
                    spacing: 4

                    // Section header — shared CollapsibleSectionHeader.
                    // 5WHY: the tap-to-toggle header (label + ▲/▼ + MouseArea
                    // + Accessible) was duplicated for Properties and Detailed
                    // Data — one shared component keeps them in sync.
                    W.CollapsibleSectionHeader {
                        Layout.fillWidth: true
                        title: T.tr("detailProperties")
                        expanded: page.propsExpanded
                        onToggleRequested: page.propsExpanded = !page.propsExpanded
                    }

                    // Property rows — with severity dots + nested children
                    ColumnLayout {
                        visible: page.propsExpanded; spacing: 2
                        Repeater {
                            model: page.detail.properties || []
                            delegate: ColumnLayout {
                                spacing: 1
                                // Main property row
                                RowLayout {
                                    spacing: 6
                                    // Severity indicator dot (color-coded)
                                    // 5WHY: explicit width/height on a layout-managed
                                    // item is undefined behavior (qmllint) — declare
                                    // preferred sizes and let the RowLayout allocate.
                                    Rectangle {
                                        Layout.preferredWidth: 6; Layout.preferredHeight: 6
                                        radius: 3
                                        color: modelData.severity === 2 ? Th.ThemeEngine.colors.failRed
                                               : modelData.severity === 1 ? Th.ThemeEngine.colors.warnYellow
                                               : Th.ThemeEngine.colors.textMuted
                                        Layout.alignment: Qt.AlignVCenter
                                        visible: modelData.severity !== undefined
                                    }
                                    Label {
                                        text: T.trProp(modelData.label) + ":"; font.family: Th.ThemeEngine.monoFont
                                        font.pixelSize: 11; color: Th.ThemeEngine.colors.textMuted
                                        Layout.preferredWidth: Math.min(implicitWidth, 140)
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        text: modelData.value || ""; font.family: Th.ThemeEngine.monoFont
                                        font.pixelSize: 11; color: Th.ThemeEngine.colors.textPrimary
                                        Layout.fillWidth: true; wrapMode: Text.WrapAnywhere
                                        Accessible.name: modelData.label + ": " + (modelData.value || "")
                                        Accessible.role: Accessible.StaticText
                                    }
                                }
                                // Nested children (indented)
                                Repeater {
                                    model: modelData.children || []
                                    delegate: RowLayout {
                                        spacing: 6
                                        Layout.leftMargin: 16  // indent children
                                        Rectangle {
                                            Layout.preferredWidth: 4; Layout.preferredHeight: 4
                                            radius: 2
                                            color: modelData.severity === 2 ? Th.ThemeEngine.colors.failRed
                                                   : modelData.severity === 1 ? Th.ThemeEngine.colors.warnYellow
                                                   : Th.ThemeEngine.colors.borderCard
                                            Layout.alignment: Qt.AlignVCenter
                                        }
                                        Label {
                                            text: T.trProp(modelData.label) + ":"; font.family: Th.ThemeEngine.monoFont
                                            font.pixelSize: 10; color: Th.ThemeEngine.colors.textMuted
                                            Layout.preferredWidth: Math.min(implicitWidth, 120)
                                            elide: Text.ElideRight
                                        }
                                        Label {
                                            text: modelData.value || ""; font.family: Th.ThemeEngine.monoFont
                                            font.pixelSize: 10; color: Th.ThemeEngine.colors.textSecondary
                                            Layout.fillWidth: true; wrapMode: Text.WrapAnywhere
                                            Accessible.name: modelData.label + ": " + (modelData.value || "")
                                            Accessible.role: Accessible.StaticText
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ── Detailed Data section (charts, default collapsed) ────────
            Rectangle {
                Layout.fillWidth: true; implicitHeight: chartsCol.implicitHeight + 16
                // 5WHY (spacing collapse): bind topMargin to visibility so
                // the Charts section boundary collapses when no chart data.
                Layout.topMargin: chartView.hasChart
                                  ? Th.ThemeEngine.spacing.lg : 0
                radius: Th.ThemeEngine.radius.md
                color: Th.ThemeEngine.colors.card
                border { width: 1; color: Th.ThemeEngine.colors.borderCard }
                // 5WHY: gate on the shared ResultChart's hasChart — a real
                // visualization exists for this template's data.
                visible: chartView.hasChart

                ColumnLayout {
                    id: chartsCol
                    anchors { fill: parent; margins: 12 }
                    spacing: 8

                    W.CollapsibleSectionHeader {
                        Layout.fillWidth: true
                        title: T.tr("detailData")
                        expanded: page.chartsExpanded
                        onToggleRequested: page.chartsExpanded = !page.chartsExpanded
                    }

                    // Chart area — all chart wiring (source selection, series,
                    // gauge spec, height, language re-bind) lives in the
                    // shared ResultChart component.
                    Viz.ResultChart {
                        id: chartView
                        Layout.fillWidth: true
                        data: page.resultData
                        expanded: page.chartsExpanded
                    }
                }
            }

            // ── Terminal output ──────────────────────────────────────────
            // 5WHY: the terminal section was always visible, showing an empty
            // dark rectangle with just the "Terminal Output" header when the
            // test produced no terminal output (e.g. G1 system property tests
            // that emit only properties[]).  Gate the entire section on output
            // presence so the empty shell never renders.
            // 5WHY (spacing collapse): bind topMargin to visibility so the
            // 16px boundary collapses when there is no terminal output.
            // 5WHY (efficiency): _hasTerminalOutput + _terminalLines consolidate
            // the 7 duplicated compound expressions into one computed property.
            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: _hasTerminalOutput ? Th.ThemeEngine.spacing.lg : 0
                implicitHeight: _hasTerminalOutput ? termBlock.implicitHeight + 16 : 0
                visible: _hasTerminalOutput
                radius: Th.ThemeEngine.radius.md
                color: Th.ThemeEngine.isDark ? Th.ThemeEngine.colors.surface : "#1E293B"
                border { width: 1; color: Th.ThemeEngine.colors.borderCard }

                ColumnLayout {
                    anchors { fill: parent; margins: 12 }
                    spacing: 6
                    Label {
                        text: T.tr("detailTerminal"); font.family: Th.ThemeEngine.monoFont
                        font.pixelSize: 11; font.weight: Font.Bold
                        color: Th.ThemeEngine.colors.textPrimary
                        Accessible.name: T.tr("detailTerminal")
                        Accessible.role: Accessible.StaticText
                    }
                    Loader {
                        id: termLoader
                        Layout.fillWidth: true
                        // Height follows line count (18px/line, clamp 120-360px)
                        Layout.preferredHeight: _hasTerminalOutput
                            ? Math.min(360, Math.max(120, _terminalLines * 18 + 24)) : 0
                        active: _hasTerminalOutput
                        source: "qrc:/qml/detail/TerminalBlock.qml"
                        onLoaded: {
                            if (item) {
                                item.text = Qt.binding(function() {
                                    return page.detail.details || page.detail.rawOutput || ""
                                })
                                // Cap typewriter animation to concise output
                                item.typewriter = _terminalLines < 100
                            }
                        }
                    }
                }
            }
        }  // bodyColumn
    }  // Flickable

    // ── Copy feedback toast ─────────────────────────────────────────────
    Rectangle {
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 24 }
        implicitWidth: copyToastLabel.implicitWidth + 24; implicitHeight: 36; radius: 18
        color: Th.ThemeEngine.colors.card; visible: page._copied !== ""
        border { width: 1; color: Th.ThemeEngine.colors.borderFocused }
        Label {
            id: copyToastLabel; anchors.centerIn: parent
            text: page._copied; font.family: Th.ThemeEngine.monoFont; font.pixelSize: 12
            color: Th.ThemeEngine.colors.textPrimary
        }
    }

    // ── Key metric (single source: shared KeyMetric.js module) ──────────
    // 5WHY: _keyMetricValue/_keyMetricLabel/_keyMetricUnit were three string
    // properties that mangled units ("% loss"→"loss") and corrupted
    // durations (parseFloat("1:23")→1).  Now a structured {value,unitKey,
    // labelKey,precision,format} object — same module DiagBlock consumes.
    readonly property var _keyMetric: KM.keyMetric(resultData, page.detail.durationMs || 0)

}
