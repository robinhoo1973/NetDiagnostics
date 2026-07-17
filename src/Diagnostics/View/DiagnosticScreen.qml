import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../widgets"
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
    Timer { id: toastTimer; interval: 3500; onTriggered: page.toast = "" }
    function doShare(fmt) { pendingShareFormat = fmt; shareStage = appState.isPremium ? 2 : 1 }
    function confirmShare() { shareStage = 0; appState.shareReport(pendingShareFormat) }

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
    readonly property bool _snapHasError: _snapTargetError !== ""
    property string _snapIconName: _snapHasError ? "badge-warning" : "badge-info"
    property color _snapIconColor: _snapHasError ? ThemeEngine.warnYellow : ThemeEngine.infoBlue
    property int _snapVersion: 0

    function takeSnapshot() {
        _snapTargetError = appState.targetValidationError()
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
    Component.onCompleted: { takeSnapshot(); console.warn("[DiagnosticScreen] loaded — DiagnosticToolbar should be visible") }

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
        for (var i = 0; i < 5; i++) {
            var s = appState.groupStats(i)
            // Only show groups that have at least one enabled test.
            // Groups with zero checked boxes are hidden — users manage
            // enable/disable from the Config page.
            // Only show groups that are active AND have at least one enabled test.
            if (appState.isGroupActive(i) && (s.enabled || 0) > 0) g.push(i)
        }
        return g
    }

    // ── Single-column layout ──────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent; spacing: 0

        // AppBar (matches Dashboard/Settings/Config — 48px Material compact)
        // 5WHY: Share buttons were in the results header, competing for space
        // with status badges on narrow screens. Moving them to the AppBar:
        // - fixes horizontal overflow (badges now have the full row)
        // - matches platform conventions (share actions belong in top chrome)
        // - shares space with the title, always visible when results exist
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 48; color: ThemeEngine.colors.navBar
            border { width: 1; color: ThemeEngine.colors.borderCard }
            RowLayout {
                anchors { fill: parent; leftMargin: 16; rightMargin: 12 }
                AppIcon { name: "diagnostics"; size: 20; color: ThemeEngine.cyan }
                Item { width: 10 }
                Label { text: Tr.diagnostics; font.family: ThemeEngine.monoFont; font.pixelSize: 15; font.weight: Font.DemiBold; color: ThemeEngine.textPrimary }
                Item { Layout.fillWidth: true }
                // 5WHY: theme color timing — using Qt.binding through
                // shareRoot.pdfAccent adds an indirection layer that may
                // evaluate before ThemeEngine.applyTheme() finishes during
                // init or theme switch. Directly bind accent to ThemeEngine
                // values, removing the intermediate property chain.
                ShareButtons {
                    id: appBarShareBtns
                    mode: "compact"
                    pdfAccent: ThemeEngine.cyan
                    htmlAccent: ThemeEngine.primary
                    visible: appState.runStatus === 2 && appState.totalCompleted > 0 && appState.totalCompleted >= appState.totalDiags
                    onShareRequested: function(fmt) { page.doShare(fmt) }
                }
            }
        }

        // ═══════════════ TOOLBAR ═══════════════════════════════════════
        DiagnosticToolbar {
            Layout.fillWidth: true
            wide: page.wide
        }

        // ═══════════════ RESULTS HEADER ════════════════════════════════
        // 5WHY: Single Row 1 (status label + count), Row 2 (badges
        // LEFT-aligned on all platforms). Desktop inline badges removed
        // — two-row layout is consistent across desktop and mobile.
        // Height is content-driven (no fixed implicitHeight).
        Rectangle {
            Layout.fillWidth: true
            readonly property bool _showBadges: appState.totalCompleted > 0
            implicitHeight: statusCol.implicitHeight + (isMobile ? 16 : 12)
            Layout.minimumHeight: appState.runStatus === 1 ? 36 : implicitHeight
            color: ThemeEngine.colors.navBar
            border { width: 1; color: ThemeEngine.colors.borderCard }
            visible: appState.totalCompleted > 0 || appState.runStatus === 1
            ColumnLayout {
                id: statusCol
                anchors { fill: parent; leftMargin: 12; rightMargin: 12; topMargin: 8; bottomMargin: 8 }
                spacing: 2
                // Row 1 — status label + count
                RowLayout {
                    spacing: 8
                    AppIcon {
                        id: statusSpinner
                        name: appState.runStatus === 1 ? "spinner" : "diagnostics"
                        size: 16
                        color: appState.runStatus === 1 ? ThemeEngine.cyan : ThemeEngine.colors.primary
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
                        color: appState.runStatus === 4 ? ThemeEngine.failRed : ThemeEngine.colors.textPrimary
                    }
                    Label {
                        visible: appState.runStatus === 1 && appState.totalDiags > 0
                        text: appState.totalCompleted + " / " + appState.totalDiags
                        font.family: ThemeEngine.monoFont; font.pixelSize: 12; font.weight: Font.DemiBold
                        color: ThemeEngine.cyan
                    }
                    Item { Layout.fillWidth: true }
                }
                // Row 2 — status badges, LEFT-aligned on all platforms
                RowLayout {
                    spacing: 4; visible: _showBadges
                    Item { width: 11 }
                    BadgeLabel { accent: ThemeEngine.passGreen;  iconName: "badge-check";   count: __agg.pass }
                    BadgeLabel { accent: ThemeEngine.infoBlue; iconName: "badge-info";    count: __agg.info }
                    BadgeLabel { accent: ThemeEngine.warnYellow; iconName: "badge-warning"; count: __agg.warn }
                    BadgeLabel { accent: ThemeEngine.failRed;    iconName: "badge-close";   count: __agg.fail }
                    BadgeLabel { accent: ThemeEngine.skipGray;   iconName: "badge-skip";    count: __agg.skip }
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
                    size: 80; color: appState.runStatus === 4 ? Qt.alpha(ThemeEngine.failRed, 0.3) : Qt.alpha(ThemeEngine.colors.textPrimary, 0.1) }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: appState.runStatus === 4 ? Tr.errorCheck : Tr.runDiag
                    font.family: ThemeEngine.monoFont; font.pixelSize: 15; font.weight: Font.Medium
                    color: appState.runStatus === 4 ? ThemeEngine.failRed : Qt.alpha(ThemeEngine.colors.textSecondary, 0.5)
                }
                // PM: Actionable error recovery guidance — uses Tr.* for i18n
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: appState.runStatus === 4
                    text: Tr.errorRecoveryHint
                    font.family: ThemeEngine.monoFont; font.pixelSize: 11
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
                contentWidth: width; contentHeight: treeColumn.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                Column {
                    id: treeColumn; width: parent.width; spacing: 4
                    Repeater {
                        model: visibleGroups
                        delegate: DiagGroupPanel {
                            anchors { left: parent.left; right: parent.right }
                            groupIndex: modelData
                            onDetailClicked: function(data) {
                                var tid = data.diagId
                                var d = appState.getDetailResult(tid)
                                dtTitle.text = (d && d.displayName) ? d.displayName : (data.displayName || "Test #" + tid)
                                var statStr = "Unknown"
                                if (d && d.status !== undefined) statStr = ["Pass","Warning","Fail","Skipped","Error","Info"][d.status] || "Unknown"
                                var durStr = (d && d.durationMs) ? d.durationMs : (data.durationMs || 0)
                                dtStatus.text = "Status: " + statStr + "    Duration: " + durStr + "ms"
                                dtSummary.text = (d && d.summary) ? d.summary : (data.summary || "")
                                dtOutput.text = (d && d.details) ? d.details : ""
                                page.currentDetail = d || {}
                                detailOverlay.visible = true
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
        Label { id: toastLabel; anchors.centerIn: parent; text: page.toast; font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.textPrimary }
    }

    // ── Share subscription dialog ─────────────────────────────────────
    Rectangle {
        id: shareDialog
        parent: page.parent ? page.parent : page; anchors.fill: parent
        color: Qt.alpha(ThemeEngine.colors.surface, 0.85)
        visible: page.shareStage !== 0; z: 1100
        MouseArea { anchors.fill: parent }
        Rectangle {
            anchors.centerIn: parent
            width: Math.min(420, parent.width * 0.92)
            implicitHeight: dlgCol.implicitHeight + 40; radius: 14; color: ThemeEngine.colors.card
            ColumnLayout {
                id: dlgCol
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 } spacing: 14
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter; width: 60; height: 60; radius: 30
                    color: Qt.alpha(page.shareStage === 1 ? ThemeEngine.warnYellow : ThemeEngine.cyan, 0.12)
                    AppIcon { anchors.centerIn: parent; name: page.shareStage === 1 ? "badge-info" : "report"; size: 28; color: page.shareStage === 1 ? ThemeEngine.warnYellow : ThemeEngine.cyan }
                }
                Label { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: page.shareStage === 1 ? Tr.subscribeTitle : Tr.confirmShareTitle; font.family: ThemeEngine.monoFont; font.pixelSize: 17; font.weight: Font.Bold; color: ThemeEngine.textPrimary; wrapMode: Text.WordWrap }
                Label { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: page.shareStage === 1 ? Tr.subscribeBody : Tr.confirmShareBody; font.family: ThemeEngine.monoFont; font.pixelSize: 13; color: ThemeEngine.textSecondary; wrapMode: Text.WordWrap }
                RowLayout { Layout.fillWidth: true; spacing: 10
                    Rectangle { Layout.fillWidth: true; implicitHeight: 42; radius: 8; color: "transparent"; border { width: 1; color: Qt.alpha(ThemeEngine.textSecondary, 0.5) }
                        Label { anchors.centerIn: parent; text: page.shareStage === 1 ? Tr.subscribeNotNow : Tr.dialogCancel; font.family: ThemeEngine.monoFont; font.pixelSize: 13; color: ThemeEngine.textSecondary }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: page.shareStage = 0 }
                        Accessible.name: page.shareStage === 1 ? Tr.subscribeNotNow : Tr.dialogCancel
                        Accessible.role: Accessible.Button
                    }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 42; radius: 8
                        color: page.shareStage === 1 ? ThemeEngine.warnYellow : ThemeEngine.cyan
                        Label { anchors.centerIn: parent; text: page.shareStage === 1 ? Tr.subscribeBtn : (page.isMobile ? Tr.shareBtn : Tr.emailBtn); font.family: ThemeEngine.monoFont; font.pixelSize: 13; font.weight: Font.DemiBold; color: ThemeEngine.bgDark }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { if (page.shareStage === 1) appState.requestSubscription(); else page.confirmShare() } }
                    }
                }
            }
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
            MouseArea { anchors.fill: parent }

            // 5WHY: Close button previously used failRed at rest — red signals
            // danger/destructive action, causing hesitation ("will this delete
            // my results?").  Now uses neutral textSecondary at rest, shifting
            // to failRed only on hover.  Always visible, never alarming.
            Rectangle {
                anchors { top: parent.top; right: parent.right; topMargin: 8; rightMargin: 8 }
                width: 44; height: 44; radius: 22
                color: closeBtnArea.containsMouse ? Qt.alpha(ThemeEngine.failRed, 0.30) : Qt.alpha(ThemeEngine.textSecondary, 0.08)
                border { width: 1; color: closeBtnArea.containsMouse ? ThemeEngine.failRed : Qt.alpha(ThemeEngine.textSecondary, 0.25) }
                AppIcon {
                    anchors.centerIn: parent
                    name: "close"; size: 18
                    color: closeBtnArea.containsMouse ? Qt.lighter(ThemeEngine.failRed, 1.3) : ThemeEngine.textSecondary
                }
                MouseArea {
                    id: closeBtnArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: detailOverlay.visible = false
                }
                Accessible.name: "Close details"
                Accessible.role: Accessible.Button
            }

            Flickable {
                anchors { fill: parent; margins: 16; topMargin: 44 }
                clip: true
                contentWidth: Math.max(width, detailCol.implicitWidth)
                contentHeight: detailCol.implicitHeight
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
                Column {
                    id: detailCol; spacing: 8
                    width: Math.max(parent.width, implicitWidth)
                    Label { id: dtTitle; text: ""; font.family:ThemeEngine.monoFont; font.pixelSize:16; font.weight:Font.DemiBold; color:ThemeEngine.colors.textPrimary; elide:Text.ElideRight }
                    Label { id: dtStatus; text: ""; font.family:ThemeEngine.monoFont; font.pixelSize:12; color:ThemeEngine.colors.textSecondary }
                    Label { id: dtSummary; text: ""; font.family:ThemeEngine.monoFont; font.pixelSize:12; color:ThemeEngine.colors.textPrimary; wrapMode:Text.WordWrap }
                    Rectangle { width: parent.width; height: 1; color: ThemeEngine.colors.borderCard }
                    Repeater {
                        model: currentDetail.properties || []
                        delegate: Row {
                            spacing: 4
                            Label { text: (modelData["label"]||"?")+":"; font.family:ThemeEngine.monoFont; font.pixelSize:11; font.weight:Font.DemiBold; color:ThemeEngine.colors.textSecondary; width:120 }
                            Label { text: modelData["value"]||""; font.family:ThemeEngine.monoFont; font.pixelSize:11; color:ThemeEngine.colors.textPrimary; wrapMode:Text.WordWrap }
                        }
                    }
                    Label { id: dtOutput; text: ""; font.family: dejavuMono.name; font.pixelSize:10; color:ThemeEngine.colors.textSecondary; wrapMode:Text.NoWrap; visible:text!=="" }
                }
            }
        }
    }


    // ── BadgeLabel: status icon + count (mirrors DiagGroupPanel.StatusBadge) ──
    // 5WHY: DiagnosticScreen used manual RowLayout { AppIcon + Label } which
    // diverged from DiagGroupPanel's StatusBadge format. Now extracted as a
    // shared component so both headers are visually identical.
    component BadgeLabel: RowLayout {
        property color accent: ThemeEngine.passGreen
        property string iconName: "badge-info"
        property int count: 0
        spacing: 2
        AppIcon { name: iconName; size: 14; color: accent }
        Label {
            text: ThemeEngine.pad2(count)
            font.family: ThemeEngine.monoFont; font.pixelSize: 12; font.weight: Font.Bold; color: accent
        }
    }
}
