import QtQuick
import "../theme"
import QtQuick.Controls
import QtQuick.Layouts
import "../widgets"
import "../dialogs"

// ── DashboardScreen with built-in Report Preview ──
Item {
    id: page
    objectName: "dashboard"
    property int _runStatus: appState.runStatus
    property int _totalCompleted: appState.totalCompleted
    property bool hasData: _totalCompleted > 0
    // 5WHY: canReport was missing — openPreview() guarded on it but it
    // was never declared, resolving to undefined → !undefined === true
    // → the function always returned early.  Declare it matching
    // ReportScreen's hasResults && !isRunning pattern.
    readonly property bool canReport: hasData && _runStatus !== 1
    readonly property var allStats: appState.allGroupStats || []
    readonly property bool isMobile: ThemeEngine.isMobile
    readonly property alias overlayVisible: previewOverlay.visible

    // ── Preview overlay state ──────────────────────────────────────────
    property string previewImagePath: ""
    property bool previewVisible: false
    property string toast: ""
    property int _previewGen: 0   // debounce: only the latest render request executes

    // Share flow
    property string pendingShareFormat: ""

    // 5WHY (review B7): Dashboard blocks open the same L5 DetailPage as the
    // Diagnostic screen.  Component cached after first load (mirrors the B9
    // fix in DiagnosticScreen) so repeated taps never recompile the page.
    property var _dashDetailComp: null
    function dashboardOpenDetail(diagId) {
        if (diagId === undefined) return
        var d = appState.getDetailResult(diagId)
        if (!d || Object.keys(d).length === 0) return
        var pushPage = function(comp) {
            var p = comp.createObject(page, { detail: d })
            page.StackView.view.push(p)
        }
        if (page._dashDetailComp !== null && page._dashDetailComp !== "loading") { pushPage(page._dashDetailComp); return }
        // Guard against concurrent loads: skip if a load is already inflight.
        if (page._dashDetailComp === "loading") return
        page._dashDetailComp = "loading"
        var comp = Qt.createComponent("qrc:/qml/screens/DetailPage.qml",
                                       Component.Asynchronous)
        if (comp.status === Component.Ready) {
            page._dashDetailComp = comp
            pushPage(comp)
        } else if (comp.status === Component.Loading) {
            comp.statusChanged.connect(function pushWhenReady() {
                if (comp.status === Component.Ready) {
                    page._dashDetailComp = comp
                    pushPage(comp)
                } else if (comp.status === Component.Error) {
                    console.warn("DetailPage.qml (Dashboard) async load failed:", comp.errorString())
                    page._dashDetailComp = null  // reset sentinel so next tap can retry
                }
            })
        } else if (comp.status === Component.Error) {
            console.warn("DetailPage.qml (Dashboard) failed to load:", comp.errorString())
            page._dashDetailComp = null  // reset sentinel so next tap can retry
        }
    }

    function openPreview() {
        if (!canReport) return
        // 5WHY: buildReportHtml() + renderPreviewImage() are synchronous
        // Q_INVOKABLE calls that iterate over all diagnostic results and
        // render HTML→image.  On 20+ completed tests this blocks the UI
        // thread for 500ms-2s — a visible freeze.  Show the overlay
        // immediately with a blank image, then defer the heavy work.
        // 5WHY (debounce): Qt.callLater alone only defers — repeated triggers
        // (theme toggle, per-test progress, save-path events) queue MULTIPLE
        // synchronous renders.  A generation counter lets the newest request
        // cancel all stale queued renders.
        previewVisible = true
        previewImagePath = ""
        _previewGen++
        var gen = _previewGen
        Qt.callLater(function() {
            if (gen !== page._previewGen || !page.previewVisible) return
            var darkBg = ThemeEngine.isDark
            var html = appState.buildReportHtml(true, darkBg)
            var imgPath = appState.renderPreviewImage(html, page.isMobile ? 480 : 960)
            page.previewImagePath = imgPath || ""
        })
    }
    // 5WHY (share flow): every share entry point must first check the Premium
    // Pro Unlocked state.  Locked → show the Premium Subscription card
    // (PremiumDialog — the exact same IAP component the Settings page uses).
    // Unlocked → skip ALL intermediate pages and jump straight to the OS share
    // sheet.  The old ShareSubscriptionDialog "confirm share" stage was
    // removed: it made unlocked users tap through an extra page.
    function doShare(fmt) {
        pendingShareFormat = fmt
        if (appState.isPremiumPlatform && !appState.isPremium)
            premiumDialog.openDialog()
        else
            appState.shareReport(fmt)
    }

    Timer { id: toastTimer; interval: ThemeEngine.toastDurationMs; onTriggered: page.toast = "" }

    Connections {
        target: ThemeEngine
        function onModeChanged() { if (page.previewVisible) page.openPreview() }
    }
    Connections {
        target: appState
        function onProgressChanged() {
            if (page.previewVisible && appState.runStatus !== 1) page.openPreview()
        }
        // 5WHY: onSavePathPicked was removed — it ignored `format` and always
        // exported PDF, and requestSavePath() has no QML caller since the
        // ReportScreen page was retired (sharing goes through doShare() →
        // appState.shareReport()). It could never fire.
        function onPremiumRequired() { page.toast = T.tr("premiumRequiredMsg"); toastTimer.restart() }
        function onPurchaseDeferred() { page.toast = T.tr("purchaseDeferred"); toastTimer.restart() }
        function onPurchaseFailed() { page.toast = T.tr("purchaseFailed"); toastTimer.restart() }
        function onReportShared(ok) { page.toast = ok ? T.tr("reportShareOk") : T.tr("reportShareFail"); toastTimer.restart() }
        function onPremiumChanged() {
            // Purchase/restore succeeded → close the IAP dialog, then continue
            // straight to the OS share sheet (no intermediate confirm page).
            if (appState.isPremium) {
                if (premiumDialog.open) premiumDialog.closeDialog()
                if (page.pendingShareFormat !== "") {
                    appState.shareReport(page.pendingShareFormat)
                    page.pendingShareFormat = ""
                }
            }
        }
    }

    // 5WHY: Status mapping was incomplete — only handled Pass(0) through
    // Skipped(3), defaulting all other statuses to "badge-info"/accentBlue.
    // DiagStatus::Error(4) and DiagStatus::Info(5) are distinct states that
    // need separate visual treatment (Error→errorRed, Info→infoBlue).  Now
    // matches DiagResultItem status mappings exactly.
    //
    // 5WHY (theme reactivity): statusColor() was a JavaScript function called
    // from QML bindings.  The QML binding engine cannot trace into JS function
    // bodies to discover that ThemeEngine.colors.xxx is read — so color
    // bindings using statusColor(s) only re-evaluate when `s` changes, never
    // when the user switches themes.  Fixed by replacing the function with a
    // property var array whose binding expression directly references
    // ThemeEngine.colors.xxx — QML CAN track these dependencies, so the array
    // is rebuilt on every theme switch and downstream color bindings re-evaluate.
    // 5WHY: Both _statusColors and statusIcon() replaced by centralized
    // ThemeEngine.statusColors[] and statusIconNames[] arrays.
    function fmtTimestamp() {
        var now = new Date();
        return ("0"+now.getHours()).slice(-2) + ":" + ("0"+now.getMinutes()).slice(-2) + ":" + ("0"+now.getSeconds()).slice(-2);
    }
    // 5WHY: the per-group results Repeater and the layer-timing Repeater
    // each built the same filtered group list inline — one could drift from
    // the other.  Single source for "visible, non-empty groups".
    function activeGroupIndices() {
        var groups = []
        for (var g = 0; g < appState.groupLabels.length; g++) {
            var s = appState.groupStats(g)
            if (appState.isGroupActive(g) && ((s.enabled || 0) > 0 || (s.total || 0) > 0)) groups.push(g)
        }
        return groups
    }

    // AppBar
    AppBar {
        id: appBar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        iconName: "dashboard"
        title: T.tr("dashboard")
        Item { Layout.fillWidth: true }
    }

    // Empty state
    Column {
        anchors.centerIn: parent; spacing: 16; visible: !hasData
        AppIcon { anchors.horizontalCenter: parent.horizontalCenter; name: "dashboard"; size: 80; color: Qt.alpha(ThemeEngine.colors.textPrimary, 0.15) }
        Label { anchors.horizontalCenter: parent.horizontalCenter; text: T.tr("noData"); font.family: ThemeEngine.monoFont; font.pixelSize: 18; font.weight: Font.Medium; color: Qt.alpha(ThemeEngine.colors.textSecondary, 0.6) }
        Label { anchors.horizontalCenter: parent.horizontalCenter; text: T.tr("runFromDiag"); font.family: ThemeEngine.monoFont; font.pixelSize: 13; color: Qt.alpha(ThemeEngine.colors.textSecondary, 0.4); horizontalAlignment: Text.AlignHCenter; lineHeight: 1.5 }
    }

    Flickable {
        anchors { left: parent.left; right: parent.right; top: appBar.bottom; bottom: parent.bottom }
        clip: true; visible: hasData
        // 5WHY: no scrollbar — inconsistent with Settings/Report/Config and
        // gave no drag-position feedback on long dashboards. Added for parity.
        ScrollBar.vertical: ScrollBar { }
        contentHeight: dashBody.implicitHeight + 24

        ColumnLayout {
            id: dashBody; width: parent.width - 48; x: 24; spacing: 0
            Item { Layout.preferredHeight: 24 }

            // ── Run Info Header Card — conditionally shows completion or running status ──
            // 5WHY: Previously showed "Diagnostic Run Complete" unconditionally
            // as soon as any test completed (hasData===true), even while
            // diagnostics were still running.  Now shows "Running Diagnostics..."
            // with a spinner while runStatus===1, and only displays the
            // completion card after the run finishes.
            Rectangle {
                Layout.fillWidth: true; implicitHeight: infoCol.implicitHeight + 32; radius: 12
                color: ThemeEngine.colors.card; border { width: 1; color: ThemeEngine.colors.borderCard }
                RowLayout {
                    id: infoCol
                    anchors { fill: parent; margins: 16 }
                    AppIcon {
                        name: appState.runStatus === 1 ? "spinner" : "check"
                        size: 28
                        color: appState.runStatus === 1 ? ThemeEngine.colors.cyan : ThemeEngine.colors.passGreen
                        RotationAnimation on rotation {
                            running: appState.runStatus === 1
                            from: 0; to: 360; duration: 1000; loops: Animation.Infinite
                            onStopped: { /* reset handled by target-property binding on name */ }
                        }
                    }
                    Item { width: 14 }
                    ColumnLayout { spacing: 4
                        Label {
                            text: appState.runStatus === 1 ? T.tr("runningDots") :
                                  appState.runStatus === 2 ? T.tr("diagRunComplete") :
                                  appState.runStatus === 3 ? T.tr("cancelled") : T.tr("diagRunComplete")
                            font.family: ThemeEngine.monoFont; font.pixelSize: 16; font.weight: Font.DemiBold
                            color: appState.runStatus === 3 ? ThemeEngine.colors.textSecondary : ThemeEngine.colors.textPrimary
                        }
                        RowLayout { spacing: 4
                            AppIcon { name: "monitor"; size: 12; color: Qt.alpha(ThemeEngine.colors.textPrimary, 0.7) }
                            Label { text: T.tr("targetLabel") + (appState.target || T.tr("naLabel")); font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.colors.textSecondary }
                        }
                        RowLayout { spacing: 4
                            AppIcon { name: "timer"; size: 12; color: Qt.alpha(ThemeEngine.colors.textPrimary, 0.7) }
                            Label { text: fmtTimestamp(); font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.colors.textSecondary }
                        }
                    }
                }
            }
            Item { Layout.preferredHeight: 24 }

            SummaryCards { Layout.fillWidth: true }
            Item { Layout.preferredHeight: 32 }

            // ── Per-Group Results header ────────────────────────────────
            Label { text: T.tr("perGroup"); font.family: ThemeEngine.monoFont; font.pixelSize: 15; font.weight: Font.DemiBold; color: ThemeEngine.colors.textPrimary }
            Item { Layout.preferredHeight: 12 }

            Repeater {
                model: page.activeGroupIndices()
                delegate: DashboardGroupRow { groupIndex: modelData; Layout.fillWidth: true }
            }
            Item { Layout.preferredHeight: 32 }

            // ── Overall Summary (Flutter: _buildOverallSection) ──────────
            Rectangle {
                Layout.fillWidth: true; implicitHeight: sumCol.implicitHeight + 32; radius: 12
                color: ThemeEngine.colors.card; border { width: 1; color: ThemeEngine.colors.borderCard }
                ColumnLayout { id: sumCol; anchors { fill: parent; margins: 16 }
                    Label { text: T.tr("summary"); font.family: ThemeEngine.monoFont; font.pixelSize: 15; font.weight: Font.DemiBold; color: ThemeEngine.colors.textPrimary }
                    Item { Layout.preferredHeight: 16 }
                    ColumnLayout {
                    Layout.fillWidth: true; spacing: 10
                    SummaryStat { appIcon: "list-checks"; clr: ThemeEngine.colors.cyan; val: appState.totalDiags; lbl: T.tr("totalDiagsLabel") }
                    SummaryStat { appIcon: "timer"; clr: ThemeEngine.colors.secondary; val: calcTotalTime(); lbl: T.tr("totalTimeLabel") }
                    SummaryStat { appIcon: "check"; clr: ThemeEngine.colors.passGreen; val: _totalCompleted; lbl: T.tr("completedLabel") }
                    }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: ThemeEngine.colors.borderCard; visible: _totalCompleted > 0 }
                    Item { Layout.preferredHeight: 12; visible: _totalCompleted > 0 }
                    Label { text: T.tr("layerTimings"); font.family: ThemeEngine.monoFont; font.pixelSize: 12; font.weight: Font.DemiBold; color: ThemeEngine.colors.textSecondary; visible: _totalCompleted > 0 }
                    Item { Layout.preferredHeight: 8; visible: _totalCompleted > 0 }
                    Repeater {
                        model: page.activeGroupIndices()
                        delegate: RowLayout {
                            visible: hasData
                            // 5WHY: replaced generic dot bullet with a per-layer
                            // semantic icon (G1..G5 map to the diagnostic groups'
                            // actual subject matter) so each row is identifiable
                            // at a glance instead of all rows looking identical.
                            // Dedicated icons (not reused from elsewhere in the
                            // app) so this list has its own distinct visual set:
                            // cpu=system hardware/adapters, shield=security,
                            // dns-lookup=DNS/internet name resolution (globe +
                            // magnifier, distinct from plain "cloud" which reads
                            // as generic cloud-storage rather than DNS lookup),
                            // terminal=remote host access, layers=protocol stack.
                            AppIcon {
                                name: ({0:"cpu",1:"shield",2:"dns-lookup",3:"terminal",4:"layers"}[modelData] || "circle")
                                size: 14; color: ThemeEngine.colors.secondary
                            }
                            Item { width: 8 }
                            AppLabel { Layout.fillWidth: true; text: T.groupName(modelData); font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.colors.textPrimary }
                            Label { text: calcLayerTiming(modelData); font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.colors.textSecondary }
                        }
                    }
                }
            }
            // ── Review Report section (visible only when run complete + results exist) ──
            // 5WHY: was visible as soon as hasData (totalCompleted>0) — user could
            // click Review Report while diagnostics were still running, producing a
            // partial report.  Now requires runStatus !== 1 (not running).
            Rectangle {
                Layout.fillWidth: true; implicitHeight: repCol.implicitHeight + 32; radius: 12
                Layout.topMargin: 16; visible: hasData && appState.runStatus !== 1
                color: ThemeEngine.colors.card; border { width: 1; color: ThemeEngine.colors.borderCard }
                ColumnLayout {
                    id: repCol; anchors { fill: parent; margins: 16 } spacing: 12
                    Label { text: T.tr("report"); font.family: ThemeEngine.fontUi; font.pixelSize: 16; font.weight: Font.DemiBold; color: ThemeEngine.colors.textPrimary }
                    AppLabel { text: T.tr("reportExportHint"); font.family: ThemeEngine.monoFont; font.pixelSize: 13; color: ThemeEngine.colors.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    // Review Report button
                    Rectangle {
                        id: reviewBtn
                        Layout.fillWidth: true; implicitHeight: 48; radius: 10
                        color: Qt.alpha(ThemeEngine.colors.cyan, 0.10)
                        border { width: 1; color: Qt.alpha(ThemeEngine.colors.cyan, 0.35) }
                        RowLayout {
                            anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                            AppIcon { name: "report"; size: 18; color: ThemeEngine.colors.cyan }
                            Item { width: 12 }
                            AppLabel { Layout.fillWidth: true; text: T.tr("reportReviewBtn"); color: ThemeEngine.colors.textPrimary; font.family: ThemeEngine.monoFont; font.pixelSize: 13; font.weight: Font.Medium }
                        }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: page.openPreview() }
                    }
                }
            }
            Item { Layout.preferredHeight: 24 }
        }
    }

    // ── Helper functions ────────────────────────────────────────────────
    function calcGroupStat(idx) {
        var s = appState.groupStats(idx);
        return s || {pass:0,warn:0,fail:0,skip:0,info:0,total:0,enabled:0}
    }
    function getDurFromResults(groupIdx) {
        var results = appState.resultsForGroup(groupIdx)
        var totalMs = 0
        for (var i = 0; i < results.length; i++) {
            totalMs += (results[i]["durationMs"] || results[i].durationMs || 0)
        }
        return totalMs > 0 ? ThemeEngine.formatDuration(totalMs) : "—"
    }
    function calcLayerTiming(idx) { return getDurFromResults(idx) }
    function calcTotalTime() {
        var total = 0
        for (var g = 0; g < appState.groupLabels.length; g++) {
            var results = appState.resultsForGroup(g)
            for (var i = 0; i < results.length; i++) {
                total += (results[i].durationMs || 0)
            }
        }
        return total > 0 ? ThemeEngine.formatDuration(total) : "—"
    }

    // ── Preview overlay (zoomable + share buttons) ──────────────────────
    Rectangle {
        id: previewOverlay
        parent: page.parent ? page.parent : page
        anchors.fill: parent
        color: Qt.alpha(ThemeEngine.colors.surface, 0.85)
        visible: page.previewVisible; z: 1000
        MouseArea { anchors.fill: parent; onClicked: page.previewVisible = false }
        Rectangle {
            anchors { fill: parent; margins: isMobile ? 0 : 8 }
            MouseArea { anchors.fill: parent; onClicked: {} }  // absorb clicks inside card
            radius: isMobile ? 0 : 12; color: ThemeEngine.colors.card; clip: true
            border { width: isMobile ? 0 : 2; color: ThemeEngine.colors.borderFocused }
            ColumnLayout {
                anchors { fill: parent; margins: 12 } spacing: 10
                // ── Header ────────────────────────────────────────────
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 48; radius: 8
                    color: Qt.alpha(ThemeEngine.colors.cyan, 0.08)
                    RowLayout {
                        anchors { fill: parent; margins: 8 }
                        AppIcon { name: "report"; size: 20; color: ThemeEngine.colors.cyan }
                        Item { width: 8 }
                        AppLabel { Layout.fillWidth: true; text: T.tr("reportReviewBtn"); font.family: ThemeEngine.monoFont; font.pixelSize: 16; font.weight: Font.Bold; color: ThemeEngine.colors.textPrimary }
                        Rectangle {
                            id: closeBtn
                            // 5WHY: close button was 36dp — below MD3 48dp minimum
                            // touch target. Now matches ReportScreen pattern: 48dp
                            // mobile, 34dp desktop.  Hover feedback added to match
                            // ReportScreen close button visual style.
                            readonly property int _btnSz: isMobile ? 48 : 34
                            implicitWidth: _btnSz; implicitHeight: _btnSz; radius: _btnSz / 2
                            color: closeMouse.containsMouse ? Qt.alpha(ThemeEngine.colors.failRed, 0.35)
                                                            : Qt.alpha(ThemeEngine.colors.failRed, 0.15)
                            AppIcon { anchors.centerIn: parent; name: "close"; size: 14; color: ThemeEngine.colors.failRed }
                            MouseArea {
                                id: closeMouse
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                hoverEnabled: true
                                onClicked: page.previewVisible = false
                            }
                        }
                    }
                }
                // ── Zoomable image area ────────────────────────────────
                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true; radius: 8; clip: true
                    color: ThemeEngine.colors.surface
                    border { width: 1; color: ThemeEngine.colors.borderCard }
                    Flickable {
                        id: previewFlick
                        anchors { fill: parent; margins: 14 }
                        clip: true
                        contentWidth: previewImg.width * previewScale
                        contentHeight: previewImg.height * previewScale
                        property real previewScale: 1.0
                        property real startScale: 1.0
                        property bool pinching: false
                        interactive: !pinching
                        PinchHandler {
                            target: null
                            onActiveChanged: {
                                previewFlick.pinching = active
                                if (active) { previewFlick.startScale = previewFlick.previewScale; previewFlick.returnToBounds() }
                            }
                            onScaleChanged: { previewFlick.previewScale = Math.max(0.25, Math.min(previewFlick.startScale * scale, 5.0)) }
                        }
                        Image {
                            id: previewImg
                            width: previewFlick.width
                            source: page.previewImagePath; fillMode: Image.PreserveAspectFit; cache: false; smooth: true; mipmap: true
                            transform: Scale { origin.x: previewImg.width/2; origin.y: previewImg.height/2; xScale: previewFlick.previewScale; yScale: previewFlick.previewScale }
                        }
                    }
                    ZoomBar {
                        id: zoomBar
                        anchors { bottom: parent.bottom; right: parent.right; margins: 8 }
                        zoomLevel: previewFlick.previewScale
                        onZoomLevelChanged: { previewFlick.previewScale = zoomBar.zoomLevel }
                    }
                }
                // ── Share buttons (PDF + HTML) ───────────────────────────
                // 5WHY: no pdfAccent/htmlAccent overrides — used defaults
                // (failRed + accentBlue). Now uses theme-appropriate accents.
                ShareButtons {
                    Layout.fillWidth: true
                    mode: "labeled"
                    pdfAccent: ThemeEngine.colors.cyan
                    htmlAccent: ThemeEngine.colors.primary
                    onShareRequested: function(fmt) { page.doShare(fmt) }
                }
}
        }
    }

    // ── Toast banner ────────────────────────────────────────────────────
    Rectangle {
        // 5WHY (z-order, 2026-08-09): the preview overlay is reparented to
        // page.parent (StackView layer) with z:1000, so anything left inside
        // the page — including this toast — renders BENEATH it.  Promote the
        // toast to the same layer with z:2000 so share-result feedback stays
        // visible while the preview is open.
        parent: page.parent ? page.parent : page
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 24 }
        implicitWidth: toastLabel.implicitWidth + 24; implicitHeight: 36; radius: 18
        color: ThemeEngine.colors.card; visible: page.toast !== ""; z: 2000
        border { width: 1; color: ThemeEngine.colors.borderFocused }
        Label { id: toastLabel; anchors.centerIn: parent; text: page.toast; font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.colors.textPrimary }
    }

    // ── Premium IAP dialog — auto-restore probe + guided purchase ───────
    // 5WHY (z-order, 2026-08-09): the preview overlay is reparented to
    // page.parent with z:1000, so this dialog — a child of the page with its
    // internal z:1200 — was COMPLETELY HIDDEN behind the overlay when sharing
    // from the preview (cross-parent z-index is not comparable in QML).
    // Promote the dialog to the same layer: its root z:1200 then stacks above
    // previewOverlay(1000), detailOverlay(1000) and cellularDialog(1150).
    PremiumDialog {
        id: premiumDialog
        parent: page.parent ? page.parent : page
        anchors.fill: parent
        isMobile: page.isMobile
    }

    // ── Inline components ───────────────────────────────────────────────
    // 5WHY: PreviewShareBtn component removed — dead code (never instantiated).
    // The share buttons in the preview overlay use the shared ShareButtons widget.
    component DashboardGroupRow: Rectangle {
        property int groupIndex: 0
        // Compute the group stats once per row instead of 5x (one per badge).
        property var _stat: calcGroupStat(groupIndex)
        Layout.fillWidth: true; implicitHeight: grpCol.implicitHeight + 28; radius: 10
        Layout.bottomMargin: 8
        color: ThemeEngine.colors.card; border { width: 1; color: ThemeEngine.colors.borderCard }
        ColumnLayout {
            id: grpCol; anchors { fill: parent; margins: 14 } spacing: 4
            RowLayout {
                Rectangle { Layout.preferredWidth: 3; implicitHeight: 20; radius: 2; color: ThemeEngine.colors.secondary }
                Item { width: 10 }
                AppLabel { Layout.fillWidth: true; text: T.groupName(groupIndex); font.family: ThemeEngine.monoFont; font.pixelSize: 13; font.weight: Font.DemiBold; color: ThemeEngine.colors.textPrimary }
                DashboardBadge { accent: ThemeEngine.colors.passGreen;  v: _stat.pass }
                DashboardBadge { accent: ThemeEngine.colors.warnYellow; v: _stat.warn }
                DashboardBadge { accent: ThemeEngine.colors.failRed;   v: _stat.fail }
                DashboardBadge { accent: ThemeEngine.colors.skipGray;  v: _stat.skip }
                DashboardBadge { accent: ThemeEngine.colors.infoBlue; v: _stat.info||0 }
                Item { width: 8 }
                Label { text: getDurFromResults(groupIndex); font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.colors.textSecondary }
            }
            Rectangle { Layout.fillWidth: true; implicitHeight: 4; radius: 2; color: ThemeEngine.colors.borderCard
                Rectangle {
                    height:4; radius:2
                    width: parent.width * (_stat.total > 0 ? (_stat.pass + _stat.warn + _stat.fail) / _stat.total : 0)
                    color: _stat.fail > 0 ? ThemeEngine.colors.warnYellow : ThemeEngine.colors.passGreen
                }
            }
            Item { Layout.preferredHeight: 10 }
            // 5WHY (review B7 / doc D11): reuse the Living Diagnostics square
            // block grid so Dashboard per-group results share the Diagnostic
            // screen's visual language (was a dense 28px text list).  Compact
            // mode: smaller icons, no metric.  Tap opens the same L5 DetailPage.
            Flow {
                id: dashFlow
                Layout.fillWidth: true
                spacing: 8
                // Fixed compact size — Dashboard is a summary; the Diagnostic
                // screen owns the responsive full-size grid (D6).
                property real blockSize: 104
                Repeater {
                    model: appState.resultsForGroup(groupIndex)
                    delegate: DiagBlock {
                        blockSize: dashFlow.blockSize
                        compact: true
                        itemData: modelData
                        testRunning: false
                        onClicked: function(data) { page.dashboardOpenDetail(data.diagId) }
                    }
                }
            }
        }
    }

    component DashboardBadge: Rectangle {
        property color accent: ThemeEngine.colors.passGreen; property int v: 0
        visible: v > 0; implicitWidth: 22; implicitHeight: 16; radius: 4; color: Qt.alpha(accent, 0.15)
        Label { anchors.centerIn: parent; text: v; font.family: ThemeEngine.monoFont; font.pixelSize: 11; font.weight: Font.Bold; color: accent }
    }

    component SummaryStat: RowLayout {
        property string appIcon: ""; property color clr: ThemeEngine.colors.cyan; property var val: 0; property string lbl: ""
        Layout.fillWidth: true
        spacing: 10
        AppIcon { name: appIcon; size: 16; color: clr; Layout.alignment: Qt.AlignVCenter }
        Label {
            Layout.fillWidth: true
            text: lbl; font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.colors.textSecondary
            // 5WHY: elide/hAlign were hard-coded for LTR; under RTL the label
            // mirrors to the right and text must hug the start edge + elide
            // at the start (left for RTL).
            elide: T.textElideStart; horizontalAlignment: T.textAlignStart; verticalAlignment: Text.AlignVCenter
        }
        Label {
            text: val; font.family: ThemeEngine.monoFont; font.pixelSize: 16; font.weight: Font.Bold; color: clr
            horizontalAlignment: T.textAlignEnd; verticalAlignment: Text.AlignVCenter
        }
    }
}