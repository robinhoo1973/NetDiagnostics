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
    // 5WHY: readonly enforces the immutability contract — MetricCard and
    // TerminalBlock Loaders use direct assignment (no binding overhead)
    // under the assumption detail never changes for the page lifetime.
    readonly property var detail: ({})
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
    // 5WHY (allocation): the keys object was rebuilt inside the binding on
    // every re-evaluation — hoisted to a static readonly table.
    readonly property var _statusKeys: ({
        0: "summaryPass", 1: "summaryWarning", 2: "summaryFail",
        3: "summarySkipped", 4: "errorStatus", 5: "summaryInfo"
    })
    readonly property string _statusText: {
        var key = page._statusKeys[page.status]
        return key ? T.tr(key) : ""
    }
    // Hero meta line: tested host only.
    // 5WHY: duration was duplicated here AND in the header ToolBar — the
    // hero's "host · 1.2s" repeated what the header already shows beside the
    // test name.  Removed the duration part; the header is the single place
    // for elapsed time, the hero keeps the identity line.
    readonly property string _metaLine: {
        var host = page.resultData.target || page.resultData.host || ""
        return host ? String(host) : ""
    }
    // 5WHY (P2): charts default-collapsed hid the visualizations.  Compact
    // series (≤8 bars) start expanded; long traceroutes stay collapsed.
    // Set once on open — data is static for the page lifetime.
    Component.onCompleted: { page.chartsExpanded = chartView.seriesCount <= 8 }

    // Pre-computed data-presence gates — evaluated once (page.detail is immutable).
    readonly property bool _hasErrorOutput: (page.detail.errorOutput || "") !== ""
    readonly property bool _hasProperties: (page.detail.properties || []).length > 0
    // 5WHY (efficiency): _terminalText caches the compound expression once;
    // _terminalLines gates visibility (use _terminalLines > 0 instead of a
    // separate boolean — avoids two properties needing to stay in sync).
    readonly property string _terminalText: page.detail.details || page.detail.rawOutput || ""
    // 5WHY: native String.split() runs at C++ speed; the char-by-char
    // JS loop was measurably slower on large outputs (traceroute 30+ lines).
    readonly property int _terminalLines: _terminalText === ""
        ? 0 : _terminalText.split('\n').length

    background: Rectangle { color: Th.ThemeEngine.colors.surface }

    // ── Header — single-row title bar ──────────────────────────────────
    // 5WHY (header-body overlap): the old two-row header (icon+name+status
    // stacked) was ~70px tall. Page.contentItem auto-offsets below the
    // header, but the ToolBar default implicitHeight adapter sometimes
    // mis-sizes on iOS/Android static builds.  Keep it flat and compact:
    //   <| test name (left)  ···  duration | COPY
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
        // 5WHY: +20 was arbitrary bottom padding — bodyColumn already handles
        // bottom spacing via its own children's implicit heights.  Use the
        // natural implicitHeight without extra padding.
        contentHeight: bodyColumn.implicitHeight
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
            // to spacing:0 with each section OWNING its trailing gap via
            // bottomMargin so the layout reads as three visual groups:
            //   Hero(top:16, bottom:8) — MetricCard(8) — Error(8)
            //   Properties(16)
            //   Charts(16)
            //   Terminal(16)
            // Theme-adaptive: values reference ThemeEngine.spacing tokens
            // (sm=8, lg=16) rather than hardcoded numbers.  Adding a new
            // section touches ONLY that section — no parent re-spacing.
            spacing: 0

            // ── Hero: result headline card (P2) — now a ConditionalCard ──
            // 5WHY: the old hero was a 120px decorative icon slab duplicating
            // the header icon and carrying zero information.  Now it is the
            // page's result headline: status, summary, target/duration.
            // 5WHY (format fix): a FIXED 96px height CLIPPED multi-line
            // summaries (e.g. DNS listing 6 addresses).  Height is now
            // content-driven (bodyLayout.implicitHeight + 24 margins, min 80).
            // 5WHY (padding drift, 2026-08-11): the formula used +32 but the
            // RowLayout's margins:12 provides only 24px of padding (12 top +
            // 12 bottom).  The 8px discrepancy added dead space at the bottom
            // of the hero card, which accumulated with Terminal's bottomMargin
            // to create an abnormally large visual gap when all middle sections
            // (Metric/Error/Properties/Charts) were hidden.
            // 5WHY (min-height dead space, 2026-08-11): the old floor of 96px
            // left a 16px empty strip for compact heroes (56px status disc +
            // 24px margins = 80px natural height).  Floor lowered to 80 = the
            // disc + margins, so no empty bottom strip remains.
            // 5WHY (gap ownership, 2026-08-12): the hero is the first card and
            // owns its header breathing room (topMargin:lg — replaces the old
            // standalone 16px spacer Item) plus its trailing gap to the
            // MetricCard (bottomMargin:sm).  Every card now carries its own
            // spacing; no element computes against the previous one.
            W.ConditionalCard {
                active: true
                topMargin: Th.ThemeEngine.spacing.lg
                bottomMargin: Th.ThemeEngine.spacing.sm
                minHeight: 80
                cardRadius: Th.ThemeEngine.radius.lg

                RowLayout {
                    id: heroRow
                    // 5WHY: margins:16 was inconsistent with other sections (margins:12)
                    // — 8px narrower content area inside the same bodyColumn width.
                    // Unified to 12px (ConditionalCard bodyLayout margins) for consistency.
                    Layout.fillWidth: true
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
            // phantom 8px gap when MetricCard was hidden.  Gap now uses
            // bottomMargin (owned by this section, gap to the next card) and
            // is bound to visibility so hidden → content AND gap collapse.
            Loader {
                Layout.fillWidth: true
                Layout.preferredHeight: _keyMetric.ok ? 72 : 0
                Layout.bottomMargin: _keyMetric.ok ? Th.ThemeEngine.spacing.sm : 0
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
            W.ConditionalCard {
                active: _hasErrorOutput
                bottomMargin: Th.ThemeEngine.spacing.sm
                contentSpacing: 6
                cardColor: Qt.alpha(Th.ThemeEngine.colors.failRed, 0.06)
                borderColor: Qt.alpha(Th.ThemeEngine.colors.failRed, 0.5)

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

                Accessible.name: T.tr("detailError")
                Accessible.role: Accessible.StaticText
            }

            // ── Properties section (collapsible) ─────────────────────────
            // 5WHY: several diagnostics emit no properties — the empty
            // section header was dead UI.  _hasProperties pre-computes this.
            W.ConditionalCard {
                active: _hasProperties
                bottomMargin: Th.ThemeEngine.spacing.lg

                // Section header — shared CollapsibleSectionHeader.
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
                            RowLayout {
                                spacing: 6
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
                                    Layout.leftMargin: 16
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

            // ── Detailed Data section (charts, default collapsed) ────────
            // 5WHY: gate on ResultChart.hasChart — a real visualization
            // exists for this template's data.
            W.ConditionalCard {
                active: chartView.hasChart
                bottomMargin: Th.ThemeEngine.spacing.lg
                contentSpacing: 8

                W.CollapsibleSectionHeader {
                    Layout.fillWidth: true
                    title: T.tr("detailData")
                    expanded: page.chartsExpanded
                    onToggleRequested: page.chartsExpanded = !page.chartsExpanded
                }

                Viz.ResultChart {
                    id: chartView
                    Layout.fillWidth: true
                    data: page.resultData
                    expanded: page.chartsExpanded
                }
            }

            // ── Terminal output ──────────────────────────────────────────
            // 5WHY: the terminal section was always visible, showing an empty
            // dark rectangle when the test produced no terminal output.
            // Gate the entire section on output presence.
            // _terminalLines gates both the ConditionalCard (layout collapse)
            // and the inner Loader height (single property, single source).
            W.ConditionalCard {
                active: _terminalLines > 0
                bottomMargin: Th.ThemeEngine.spacing.lg
                contentSpacing: 6
                cardColor: Th.ThemeEngine.isDark ? Th.ThemeEngine.colors.surface : "#1E293B"

                Label {
                    text: T.tr("detailTerminal"); font.family: Th.ThemeEngine.monoFont
                    font.pixelSize: 11; font.weight: Font.Bold
                    color: Th.ThemeEngine.colors.textPrimary
                    Accessible.name: T.tr("detailTerminal")
                    Accessible.role: Accessible.StaticText
                }
                Loader {
                    Layout.fillWidth: true
                    // Height bounded [72, 360]; parent ConditionalCard already
                    // collapses the card when _terminalLines === 0, so no
                    // ternary guard needed here.
                    Layout.preferredHeight: Math.min(360, Math.max(72, _terminalLines * 18 + 24))
                    active: _terminalLines > 0
                    source: "qrc:/qml/detail/TerminalBlock.qml"
                    onLoaded: {
                        if (item) {
                            // Direct assignment — page.detail is immutable for
                            // the page lifetime, no need for binding overhead.
                            item.text = _terminalText
                            item.typewriter = (resultData.terminalTypewriter === true)
                                && _terminalText.length < 2000
                                && _terminalLines < 100
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
