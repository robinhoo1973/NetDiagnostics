import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../widgets"
import "../dialogs"
import "../theme"

// ── Single-column layout — toolbar + results ──────────────────────────
Item {
    id: page
    objectName: "diagnostic"
    FontLoader { id: dejavuMono; source: "qrc:/fonts/DejaVuSansMono.ttf" }
    readonly property bool wide: width >= 600
    readonly property alias overlayVisible: detailOverlay.visible
    readonly property bool isMobile: ThemeEngine.isMobile

    // ── Share flow state ───────────────────────────────────────────────
    property int shareStage: 0
    property string pendingShareFormat: ""
    property string toast: ""
    Timer { id: toastTimer; interval: ThemeEngine.toastDurationMs; onTriggered: page.toast = "" }
    function doShare(fmt) { pendingShareFormat = fmt; shareStage = appState.isPremium ? 2 : 1 }
    function confirmShare() { shareStage = 0; appState.shareReport(pendingShareFormat) }

    // ── Mobile data warning ──────────────────────────────────────────
    // Cancel = just hide the dialog (cellularWarnVisible=false via QML binding)

    Connections {
        target: appState
        function onPremiumRequired() { page.toast = Tr.premiumRequiredMsg; toastTimer.restart() }
        function onReportShared(ok) { page.toast = ok ? Tr.reportShareOk : Tr.reportShareFail; toastTimer.restart() }
        function onPremiumChanged() { if (appState.isPremium && page.shareStage === 1) page.shareStage = 2 }
    }

    // ── Run state ─────────────────────────────────────────────────────
    property bool _runActive: false
    property int _cachedGen: -1
    property string _snapTargetError: ""
    // 5WHY: _snapHasError was a derivable property (_snapTargetError !== "") —
    // inlined directly where used to eliminate one QML binding slot.
    property string _snapIconName: _snapTargetError !== "" ? "badge-warning" : "badge-info"
    property color _snapIconColor: _snapTargetError !== "" ? ThemeEngine.colors.warnYellow : ThemeEngine.colors.infoBlue
    property int _snapVersion: 0

    // Keep detail-overlay population behind one function so every caller
    // updates the title, status, summary, and detail text consistently.
    function showDetailOverlay(detail) {
        dtTitle.text = detail.displayName || ""
        var statusNames = [Tr.summaryPass, Tr.summaryWarning, Tr.summaryFail, Tr.summarySkipped, Tr.errorStatus, Tr.summaryInfo]
        var s = detail.status !== undefined ? detail.status : 0
        dtStatus.text = Tr.detailStatusLabel + (statusNames[s] || Tr.detailUnknownStatus)
            + "    " + Tr.detailDurationLabel + (detail.durationMs !== undefined ? detail.durationMs : 0) + "ms"
        dtSummary.text = detail.summary || ""
        dtOutput.text = detail.details || ""
        detailOverlay.visible = true
    }

    // Close any existing detail before showing another one so stale data
    // cannot remain visible during rapid user navigation.
    function dismissDetailOverlay() {
        detailOverlay.visible = false
    }

    function takeSnapshot() {
        _snapTargetError = appState.targetValidationErrorText
        _snapVersion++
    }
    function syncState() {
        var v = appState.stateVersion
        if (v === _cachedGen) return; _cachedGen = v
        var ns = appState.runStatus
        if (ns === 1 && !_runActive) { takeSnapshot(); _runActive = true }
        else if (ns !== 1 && _runActive) { _runActive = false }
        if (!_runActive) takeSnapshot()
    }
    Connections { target: appState; function onStateVersionChanged() { syncState() } }
    Component.onCompleted: takeSnapshot()

    // Aggregate badge counts — refreshed on each completed test.
    // 5WHY: was 5 independent properties each calling groupStats(-1)
    // (5× loop over groups = 25 iterations per change).  Single object
    // property calls groupStats(-1) once, saving 4 C++ calls per update.
    property var __agg: {
        var _ = appState.totalCompleted; var s = appState.groupStats(-1)
        return { pass: s.pass||0, info: s.info||0, warn: s.warn||0, fail: s.fail||0, skip: s.skip||0 }
    }

    property var currentDetail: ({})
    property var visibleGroups: {
        var _ = _snapVersion
        var g = []
        for (var i = 0; i < appState.groupLabels.length; i++) {
            var s = appState.groupStats(i)
            if (appState.isGroupActive(i) && (s.enabled || 0) > 0) g.push(i)
        }
        return g
    }

    // ── Single-column layout ──────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent; spacing: 0

        // AppBar (matches Dashboard/Settings — 48px Material compact.
        // ConfigScreen is 84px because its TabBar lives in a separate block.)
        // 5WHY: Share buttons moved to results header to avoid competing for
        // horizontal space with the title.
        AppBar {
            Layout.fillWidth: true
            iconName: "diagnostics"
            title: Tr.diagnostics
            Item { width: 8 }
            Item { Layout.fillWidth: true }
        }

        // ═══════════════ TOOLBAR ═══════════════════════════════════════
        DiagnosticToolbar {
            Layout.fillWidth: true
            wide: page.wide
        }

        // ═══════════════ RESULTS HEADER ════════════════════════════════
        // 5WHY: Share buttons moved from AppBar to here, vertically centered
        // across both rows of status content.  On the AppBar they competed for
        // horizontal space with the title.  Here they sit next to "Diagnostic
        // Complete" — the natural place for export actions.
        Rectangle {
            id: resultsHeader
            Layout.fillWidth: true
            readonly property bool _showBadges: appState.totalCompleted > 0
            // 5WHY: Use fixed 36px minimum instead of binding Layout.minimumHeight
            // to implicitHeight — prevents a latent binding cycle if future children
            // use Layout.fillHeight (which would make their height depend on the
            // parent's allocated height → implicitHeight becomes allocation-dependent).
            implicitHeight: Math.max(statusCol.implicitHeight,
                                     (statusBarShareBtns.visible ? statusBarShareBtns.implicitHeight : 0))
                            + (isMobile ? 12 : 8)
            Layout.minimumHeight: 36
            color: ThemeEngine.colors.navBar
            border { width: 1; color: ThemeEngine.colors.borderCard }
            visible: appState.totalCompleted > 0 || appState.runStatus === 1
            RowLayout {
                anchors { fill: parent; leftMargin: 12; rightMargin: 10; topMargin: 4; bottomMargin: 4 }
                spacing: 6
                ColumnLayout {
                    id: statusCol
                    Layout.fillWidth: true
                    spacing: 2
                    // Row 1 — status label + count
                    RowLayout {
                        spacing: 8
                        AppIcon {
                            id: statusSpinner
                            name: appState.runStatus === 1 ? "spinner" : "diagnostics"
                            size: 16
                            color: appState.runStatus === 1 ? ThemeEngine.colors.cyan : ThemeEngine.colors.primary
                            RotationAnimation on rotation {
                                running: appState.runStatus === 1
                                from: 0; to: 360; duration: 1000; loops: Animation.Infinite
                                onStopped: statusSpinner.rotation = 0
                            }
                        }
                        Item { width: 4 }
                        Label {
                            text: appState.runStatus === 1 ? Tr.runningDots :
                                  appState.runStatus === 2 ? Tr.complete :
                                  appState.runStatus === 3 ? Tr.cancelled :
                                  appState.runStatus === 4 ? Tr.errorStatus : Tr.results
                            font.family: ThemeEngine.monoFont; font.pixelSize: 13; font.weight: Font.DemiBold
                            color: appState.runStatus === 4 ? ThemeEngine.colors.errorRed : ThemeEngine.colors.textPrimary
                        }
                        Label {
                            visible: appState.runStatus === 1 && appState.totalDiags > 0
                            text: appState.totalCompleted + " / " + appState.totalDiags
                            font.family: ThemeEngine.monoFont; font.pixelSize: 12; font.weight: Font.DemiBold
                            color: ThemeEngine.colors.cyan
                        }
                        Item { Layout.fillWidth: true }
                    }
                    // Row 2 — status badges, LEFT-aligned on all platforms
                    RowLayout {
                        spacing: 4; visible: resultsHeader._showBadges
                        Item { width: 11 }
                        StatusBadge { statusCode: 0; count: __agg.pass }
                        StatusBadge { statusCode: 5; count: __agg.info }
                        StatusBadge { statusCode: 1; count: __agg.warn }
                        StatusBadge { statusCode: 2; count: __agg.fail }
                        StatusBadge { statusCode: 3; count: __agg.skip }
                    }
                }
                // 5WHY: Share buttons moved from AppBar to results header.
                // Vertically centered across both status rows via
                // Layout.alignment: Qt.AlignVCenter so the 40dp icons span
                // the full height of both rows naturally.  No extra
                // ColumnLayout wrapper needed — ShareButtons is a RowLayout
                // that aligns correctly on its own.
                ShareButtons {
                    id: statusBarShareBtns
                    Layout.alignment: Qt.AlignVCenter
                    mode: "bare"
                    pdfAccent: ThemeEngine.colors.cyan
                    htmlAccent: ThemeEngine.colors.primary
                    visible: appState.runStatus === 2 && appState.totalCompleted > 0 && appState.totalCompleted >= appState.totalDiags
                    onShareRequested: function(fmt) { page.doShare(fmt) }
                }
            }
        }


        // ═══════════════ RESULTS ═══════════════════════════════════════
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true

            // 5WHY: Show empty state whenever totalCompleted===0 and not running.
            // Previous condition (runStatus===0 && totalCompleted===0) left a gap:
            // if status was 2/3 with 0 results, the user saw a blank area.
            // PM review: error state now shows actionable recovery guidance instead
            // of just "Run Diagnostics" — helps users self-diagnose common issues.
            Column {
                anchors.centerIn: parent; spacing: 16
                visible: appState.totalCompleted === 0 && appState.runStatus !== 1
                AppIcon { anchors.horizontalCenter: parent.horizontalCenter
                    name: appState.runStatus === 4 ? "badge-error" : "diagnostics"
                    size: 80; color: appState.runStatus === 4 ? Qt.alpha(ThemeEngine.colors.errorRed, 0.3) : Qt.alpha(ThemeEngine.colors.textPrimary, 0.1) }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: appState.runStatus === 4 ? Tr.errorCheck : Tr.runDiag
                    font.family: ThemeEngine.monoFont; font.pixelSize: 15; font.weight: Font.Medium
                    color: appState.runStatus === 4 ? ThemeEngine.colors.errorRed : Qt.alpha(ThemeEngine.colors.textSecondary, 0.5)
                }
                // PM: Actionable error recovery guidance — uses Tr.* for i18n
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: appState.runStatus === 4
                    // 5WHY: AppState already exposes the specific reason when
                    // no runnable tests exist, but this screen discarded it
                    // for a generic checklist. Show the concrete reason first
                    // so users can correct configuration without guessing.
                    // 5WHY: errorMessage is a C++ message — route through
                    // Tr.trMsg() so it translates on language switch.
                    text: appState.errorMessage !== "" ? Tr.trMsg(appState.errorMessage) : Tr.errorRecoveryHint
                    font.family: ThemeEngine.monoFont; font.pixelSize: 12
                    color: Qt.alpha(ThemeEngine.colors.textSecondary, 0.6)
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    width: Math.min(400, parent.width - 40)
                }
            }

            // Results list
            Flickable {
                id: resultsFlick
                anchors { fill: parent; margins: 4 }
                visible: appState.totalCompleted > 0 || appState.runStatus === 1
                clip: true
                // 5WHY: results pane had no scrollbar — on desktop with many
                // groups there was no affordance or drag position feedback.
                // Consistent with Settings/Report/Config which all show one.
                ScrollBar.vertical: ScrollBar { }
                contentWidth: width; contentHeight: treeColumn.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                Column {
                    id: treeColumn; width: parent.width; spacing: 4
                    Repeater {
                        model: visibleGroups
                        delegate: DiagGroupPanel {
                            anchors { left: parent.left; right: parent.right }
                            groupIndex: modelData
                            // 5WHY: previously duplicated overlay-population logic also in
                            // showDetailOverlay(). Refactored to call the shared function
                            // so bugfixes to overlay display apply uniformly.
                            onDetailClicked: function(data) {
                                var tid = data.diagId
                                var d = appState.getDetailResult(tid)
                                showDetailOverlay({
                                    displayName: (d && d.displayName) ? d.displayName : (data.displayName || Tr.testIdPrefix + tid),
                                    status: (d && d.status !== undefined) ? d.status : 0,
                                    // 5WHY: falsy-zero bug — a test that took 0ms has
                                    // d.durationMs===0 which is falsy, falling to the
                                    // wrong default. Use strict undefined check instead.
                                    durationMs: (d && d.durationMs !== undefined) ? d.durationMs : (data.durationMs || 0),
                                    summary: (d && d.summary) ? d.summary : (data.summary || ""),
                                    details: (d && d.details) ? d.details : ""
                                })
                                page.currentDetail = d || {}
                            }
                        }
                    }
                }
            }

        }
    }

    // ── Toast banner ──────────────────────────────────────────────────
    Rectangle {
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 24 }
        implicitWidth: toastLabel.implicitWidth + 24; implicitHeight: 36; radius: 18
        color: ThemeEngine.colors.card; visible: page.toast !== ""; z: 2000
        border { width: 1; color: ThemeEngine.colors.borderFocused }
        Label { id: toastLabel; anchors.centerIn: parent; text: page.toast; font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.colors.textPrimary }
    }

    // ── Mobile data warning dialog ────────────────────────────────────
    Rectangle {
        id: cellularDialog
        parent: page.parent ? page.parent : page; anchors.fill: parent
        color: Qt.alpha(ThemeEngine.colors.surface, 0.82)
        visible: appState.cellularWarnVisible; z: 1150

        // Backdrop: tap to dismiss → cancel entire diagnostic run
        MouseArea {
            anchors.fill: parent
            onClicked: {
                appState.cellularWarnVisible = false; appState.cancel()
            }
        }
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(380, parent.width * 0.88)
            implicitHeight: cellCol.implicitHeight + 48
            radius: 20; color: ThemeEngine.colors.card
            border { width: 1; color: ThemeEngine.colors.borderSubtle }
            ColumnLayout {
                id: cellCol
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 28 }
                spacing: 16
                // Icon — signal/cellular indicator
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: 64; implicitHeight: 64; radius: 32
                    color: Qt.alpha(ThemeEngine.colors.warnYellow, 0.10)
                    // 5WHY: "wifi" icon was misleading — this dialog warns about
                    // cellular data, not WiFi.  Changed to "warning" which
                    // is semantically correct for a warning/alert dialog.
                    AppIcon {
                        anchors.centerIn: parent
                        name: "warning"; size: 32
                        color: ThemeEngine.colors.warnYellow
                    }
                }
                // Title
                Label {
                    Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                    text: Tr.cellularWarnTitle
                    font.family: ThemeEngine.monoFont; font.pixelSize: 18
                    font.weight: Font.Bold; color: ThemeEngine.colors.textPrimary
                }
                // Body — context-aware: mentions G3 tests are next
                Label {
                    Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                    text: Tr.cellularWarnBody
                    font.family: ThemeEngine.monoFont; font.pixelSize: 13
                    color: ThemeEngine.colors.textSecondary; lineHeight: 1.5; wrapMode: Text.WordWrap
                }
                // Buttons
                RowLayout {
                    Layout.topMargin: 4; spacing: 12
                    Layout.fillWidth: true
                    // Cancel — subtle outline
                    Rectangle {
                        Layout.fillWidth: true; implicitHeight: 44; radius: 12
                        color: "transparent"
                        border { width: 1.5; color: Qt.alpha(ThemeEngine.colors.textSecondary, 0.30) }
                        Label {
                            anchors.centerIn: parent
                            text: Tr.cellularCancel
                            font.family: ThemeEngine.monoFont; font.pixelSize: 14
                            font.weight: Font.Medium; color: ThemeEngine.colors.textSecondary
                        }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                appState.cellularWarnVisible = false; appState.cancel()
                            }
                        }
                    }
                    // Continue — prominent filled action
                    Rectangle {
                        Layout.fillWidth: true; implicitHeight: 44; radius: 12
                        color: ThemeEngine.colors.cyan
                        Label {
                            anchors.centerIn: parent
                            text: Tr.cellularContinue
                            font.family: ThemeEngine.monoFont; font.pixelSize: 14
                            font.weight: Font.Bold; color: ThemeEngine.colors.textOnAccent
                        }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                appState.continueAfterCellularWarn()
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Share subscription dialog ─────────────────────────────────────
    ShareSubscriptionDialog {
        shareStage: page.shareStage; isMobile: page.isMobile
        onDismissed: page.shareStage = 0
        onActionRequested: {
            if (page.shareStage === 1) appState.requestSubscription()
            else page.confirmShare()
        }
    }

    // ═══════════════ DETAIL OVERLAY ═══════════════════════════════════
    Rectangle {
        id: detailOverlay
        parent: page.parent ? page.parent : page
        anchors.fill: parent
        // 5WHY: Overlay was hardcoded semi-transparent black — doesn't adapt
        // to light theme. Now uses ThemeEngine surface color with opacity for
        // proper theme-aware dimming.
        color: Qt.alpha(ThemeEngine.colors.surface, 0.85); visible: false; z: 1000
        focus: visible  // 5WHY: needs focus to receive Escape key for keyboard dismiss
        onVisibleChanged: {
            if (!visible) {
                dtTitle.text=""; dtStatus.text=""; dtSummary.text=""; dtOutput.text=""; page.currentDetail = {}
            }
        }
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Escape || event.key === Qt.Key_Enter || event.key === Qt.Key_Return) {
                detailOverlay.visible = false
                event.accepted = true
            }
        }
        MouseArea { anchors.fill: parent; onClicked: detailOverlay.visible = false }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(700, parent.width - 20)
            height: Math.min(parent.height - 40, 620)
            radius: 14
            color: ThemeEngine.colors.card
            border { width: 1.5; color: ThemeEngine.colors.borderFocused }

            // 5WHY: Background MouseArea (line 240) closes the overlay on any
            // click. Without this, clicking inside the card to select text or
            // scroll also dismisses the overlay. This empty MouseArea absorbs
            // clicks within the card so only clicks OUTSIDE the card dismiss.
            MouseArea { anchors.fill: parent; onClicked: {} }

            // 5WHY: Close button previously used failRed at rest — red signals
            // danger/destructive action, causing hesitation ("will this delete
            // my results?").  Now uses neutral textSecondary at rest, shifting
            // to failRed only on hover.  Always visible, never alarming.
            Rectangle {
                anchors { top: parent.top; right: parent.right; topMargin: 8; rightMargin: 8 }
                width: 44; height: 44; radius: 22
                color: closeBtnArea.containsMouse ? Qt.alpha(ThemeEngine.colors.failRed, 0.30) : Qt.alpha(ThemeEngine.colors.textSecondary, 0.08)
                border { width: 1; color: closeBtnArea.containsMouse ? ThemeEngine.colors.failRed : Qt.alpha(ThemeEngine.colors.textSecondary, 0.25) }
                AppIcon {
                    anchors.centerIn: parent
                    name: "close"; size: 18
                    color: closeBtnArea.containsMouse ? Qt.lighter(ThemeEngine.colors.failRed, 1.3) : ThemeEngine.colors.textSecondary
                }
                MouseArea {
                    id: closeBtnArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: detailOverlay.visible = false
                }
                Accessible.name: Tr.accCloseDetails
                Accessible.role: Accessible.Button
            }

            Flickable {
                anchors { fill: parent; margins: 16; topMargin: 44 }
                clip: true
                contentWidth: Math.max(width, detailCol.implicitWidth)
                contentHeight: detailCol.implicitHeight
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
                Column {
                    id: detailCol; spacing: 10
                    // 5WHY: width = max(parent.width, implicitWidth) means the
                    // Column always expands to fit its widest child (typically
                    // dtOutput's terminal lines).  In that case, Labels are never
                    // width-constrained — elide: Text.ElideRight is dead code.
                    // The Flickable provides horizontal scrolling for overflow;
                    // elide is removed to avoid misleading future developers.
                    width: Math.max(parent.width, implicitWidth)
                    Label { id: dtTitle; objectName: "dtTitle"; text: ""; font.family:ThemeEngine.monoFont; font.pixelSize:16; font.weight:Font.DemiBold; color:ThemeEngine.colors.textPrimary }
                    Label { id: dtStatus; objectName: "dtStatus"; text: ""; font.family:ThemeEngine.monoFont; font.pixelSize:12; color:ThemeEngine.colors.textSecondary }
                    // 5WHY: 10px summary/output were below comfortable reading
                    // size for a detail pane (the primary place users read
                    // prose). Bumped: summary 10→12, output 10→11 (data lines
                    // keep slight density), property rows 11→12.
                    Label { id: dtSummary; objectName: "dtSummary"; text: ""; font.family:ThemeEngine.monoFont; font.pixelSize:12; color:ThemeEngine.colors.textPrimary; wrapMode:Text.WordWrap }
                    Rectangle { width: parent.width; height: 1; color: ThemeEngine.colors.borderCard }
                    Repeater {
                        model: currentDetail.properties || []
                        delegate: Row {
                            spacing: 4
                            Label { text: (modelData["label"]||"?")+":"; font.family:ThemeEngine.monoFont; font.pixelSize:12; font.weight:Font.DemiBold; color:ThemeEngine.colors.textSecondary; width:120 }
                            Label { text: modelData["value"]||""; font.family:ThemeEngine.monoFont; font.pixelSize:12; color:ThemeEngine.colors.textPrimary; wrapMode:Text.WordWrap }
                        }
                    }
                    Label { id: dtOutput; objectName: "dtOutput"; text: ""; font.family: dejavuMono.name; font.pixelSize:11; color:ThemeEngine.colors.textSecondary; wrapMode:Text.NoWrap; visible:text!=="" }
                }
            }
        }
    }


    // BadgeLabel → shared StatusBadge (src/Common/View/widgets/StatusBadge.qml)
}
