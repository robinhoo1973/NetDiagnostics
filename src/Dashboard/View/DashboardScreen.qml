import QtQuick
import "../theme"
import QtQuick.Controls
import QtQuick.Layouts
import "../widgets"

// ── DashboardScreen with built-in Report Preview ──
Item {
    id: page
    objectName: "dashboard"
    property int _runStatus: appState.runStatus
    property int _totalCompleted: appState.totalCompleted
    property bool hasData: _totalCompleted > 0
    readonly property var allStats: appState.allGroupStats || []
    readonly property bool isMobile: Qt.platform.os === "ios" || Qt.platform.os === "android"

    // ── Preview overlay state ──────────────────────────────────────────
    property string previewImagePath: ""
    property bool previewVisible: false
    property string toast: ""

    // Share/subscription flow
    property int shareStage: 0
    property string pendingShareFormat: ""

    function openPreview() {
        var darkBg = ThemeEngine.isDark
        var html = appState.buildReportHtml(true, darkBg)
        var imgPath = appState.renderPreviewImage(html, page.isMobile ? 480 : 960)
        previewImagePath = imgPath || ""
        previewVisible = true
    }
    function doShare(fmt) {
        pendingShareFormat = fmt
        shareStage = appState.isPremium ? 2 : 1
    }
    function confirmShare() {
        var fmt = pendingShareFormat
        shareStage = 0
        appState.shareReport(fmt)
    }

    Timer { id: toastTimer; interval: 3500; onTriggered: page.toast = "" }

    Connections {
        target: ThemeEngine
        function onModeChanged() { if (page.previewVisible) page.openPreview() }
    }
    Connections {
        target: appState
        function onProgressChanged() {
            if (page.previewVisible && appState.runStatus !== 1) page.openPreview()
        }
        function onSavePathPicked(format, path) { appState.exportPdf(path) }
        function onPremiumRequired() { page.toast = Tr.premiumRequiredMsg; toastTimer.restart() }
        function onReportShared(ok) { page.toast = ok ? Tr.reportShareOk : Tr.reportShareFail; toastTimer.restart() }
        function onPremiumChanged() { if (appState.isPremium && page.shareStage === 1) page.shareStage = 2 }
    }

    function statusIcon(s) { switch(s) { case 0: return "badge-check"; case 1: return "badge-warning"; case 2: return "badge-close"; case 3: return "badge-skip"; default: return "badge-info" } }
    function statusColor(s) { switch(s) { case 0: return ThemeEngine.passGreen; case 1: return ThemeEngine.warnYellow; case 2: return ThemeEngine.failRed; case 3: return ThemeEngine.skipGray; default: return ThemeEngine.accentBlue } }
    function fmtDur(ms) {
        if (ms < 1000) return ms + "ms"
        if (ms < 60000) return (ms/1000).toFixed(1) + "s"
        var min = Math.floor(ms / 60000)
        var sec = Math.round((ms % 60000) / 1000)
        return min + "m " + sec + "s"
    }
    function fmtTimestamp() {
        var now = new Date();
        return ("0"+now.getHours()).slice(-2) + ":" + ("0"+now.getMinutes()).slice(-2) + ":" + ("0"+now.getSeconds()).slice(-2);
    }

    // AppBar (Flutter: Scaffold.appBar with title)
    Rectangle {
        id: appBar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        implicitHeight: 48; color: ThemeEngine.colors.navBar
        border { width: 1; color: ThemeEngine.colors.borderCard }
        RowLayout {
            anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
            AppIcon { name: "dashboard"; size: 20; color: ThemeEngine.cyan }
            Item { width: 10 }
            Label { text: Tr.dashboard; font.family: ThemeEngine.monoFont; font.pixelSize: 15; font.weight: Font.DemiBold; color: ThemeEngine.textPrimary }
            Item { Layout.fillWidth: true }
        }
    }

    // Empty state
    Column {
        anchors.centerIn: parent; spacing: 16; visible: !hasData
        AppIcon { anchors.horizontalCenter: parent.horizontalCenter; name: "dashboard"; size: 80; color: Qt.alpha(ThemeEngine.textPrimary, 0.15) }
        Label { anchors.horizontalCenter: parent.horizontalCenter; text: Tr.noData; font.family: ThemeEngine.monoFont; font.pixelSize: 18; font.weight: Font.Medium; color: Qt.alpha(ThemeEngine.textSecondary, 0.6) }
        Label { anchors.horizontalCenter: parent.horizontalCenter; text: Tr.runFromDiag; font.family: ThemeEngine.monoFont; font.pixelSize: 13; color: Qt.alpha(ThemeEngine.textSecondary, 0.4); horizontalAlignment: Text.AlignHCenter; lineHeight: 1.5 }
    }

    Flickable {
        anchors { left: parent.left; right: parent.right; top: appBar.bottom; bottom: parent.bottom }
        clip: true; visible: hasData
        contentHeight: dashBody.implicitHeight + 24

        ColumnLayout {
            id: dashBody; width: parent.width - 48; x: 24; spacing: 0
            Item { Layout.preferredHeight: 24 }

            // ── Run Info Header Card (Flutter: check_circle + "Diagnostic Run Complete" + target/timestamp) ──
            Rectangle {
                Layout.fillWidth: true; implicitHeight: infoCol.implicitHeight + 32; radius: 12
                color: ThemeEngine.bgCard; border { width: 1; color: ThemeEngine.colors.borderCard }
                RowLayout {
                    id: infoCol
                    anchors { fill: parent; margins: 16 }
                    AppIcon { name: "check"; size: 28; color: ThemeEngine.passGreen }
                    Item { width: 14 }
                    ColumnLayout { spacing: 4
                        Label { text: Tr.diagRunComplete; font.family: ThemeEngine.monoFont; font.pixelSize: 16; font.weight: Font.DemiBold; color: ThemeEngine.textPrimary }
                        RowLayout { spacing: 4
                            AppIcon { name: "target"; size: 12; color: Qt.alpha(ThemeEngine.textPrimary, 0.7) }
                            Label { text: Tr.targetLabel + (appState.target || Tr.naLabel); font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.textSecondary }
                        }
                        RowLayout { spacing: 4
                            AppIcon { name: "timer"; size: 12; color: Qt.alpha(ThemeEngine.textPrimary, 0.7) }
                            Label { text: fmtTimestamp(); font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.textSecondary }
                        }
                    }
                }
            }
            Item { Layout.preferredHeight: 24 }

            SummaryCards { Layout.fillWidth: true; compact: true }
            Item { Layout.preferredHeight: 32 }

            // ── Per-Group Results header ────────────────────────────────
            Label { text: Tr.perGroup; font.family: ThemeEngine.monoFont; font.pixelSize: 15; font.weight: Font.DemiBold; color: ThemeEngine.textPrimary }
            Item { Layout.preferredHeight: 12 }

            Repeater {
                model: {
                    var groups = []
                    for (var g = 0; g < appState.groupLabels.length; g++) {
                        var s = appState.groupStats(g)
                        if (appState.isGroupActive(g) && ((s.enabled || 0) > 0 || (s.total || 0) > 0)) groups.push(g)
                    }
                    return groups
                }
                delegate: DashboardGroupRow { groupIndex: modelData; Layout.fillWidth: true }
            }
            Item { Layout.preferredHeight: 32 }

            // ── Overall Summary (Flutter: _buildOverallSection) ──────────
            Rectangle {
                Layout.fillWidth: true; implicitHeight: sumCol.implicitHeight + 32; radius: 12
                color: ThemeEngine.bgCard; border { width: 1; color: ThemeEngine.colors.borderCard }
                ColumnLayout { id: sumCol; anchors { fill: parent; margins: 16 }
                    Label { text: Tr.summary; font.family: ThemeEngine.monoFont; font.pixelSize: 15; font.weight: Font.DemiBold; color: ThemeEngine.textPrimary }
                    Item { Layout.preferredHeight: 16 }
                    ColumnLayout {
                    Layout.fillWidth: true; spacing: 10
                    SummaryStat { appIcon: "config"; clr: ThemeEngine.cyan; val: appState.totalDiags; lbl: Tr.totalDiagsLabel }
                    SummaryStat { appIcon: "timer"; clr: ThemeEngine.accentBlue; val: calcTotalTime(); lbl: Tr.totalTimeLabel }
                    SummaryStat { appIcon: "check"; clr: ThemeEngine.passGreen; val: _totalCompleted; lbl: Tr.completedLabel }
                    }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: ThemeEngine.colors.borderCard; visible: _totalCompleted > 0 }
                    Item { Layout.preferredHeight: 12; visible: _totalCompleted > 0 }
                    Label { text: Tr.layerTimings; font.family: ThemeEngine.monoFont; font.pixelSize: 12; font.weight: Font.DemiBold; color: ThemeEngine.textSecondary; visible: _totalCompleted > 0 }
                    Item { Layout.preferredHeight: 8; visible: _totalCompleted > 0 }
                    Repeater {
                        model: {
                            var groups = []
                             for (var g = 0; g < appState.groupLabels.length; g++) {
                                var s = appState.groupStats(g)
                                if (appState.isGroupActive(g) && ((s.enabled || 0) > 0 || (s.total || 0) > 0)) groups.push(g)
                            }
                            return groups
                        }
                        delegate: RowLayout {
                            visible: hasData
                            Rectangle { implicitWidth: 8; implicitHeight: 8; radius: 2; color: ThemeEngine.accentBlue }
                            Item { width: 10 }
                            Label { Layout.fillWidth: true; text: Tr.groupName(modelData); font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.textPrimary }
                            Label { text: calcLayerTiming(modelData); font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.textSecondary }
                        }
                    }
                }
            }
            // ── Review Report section (visible only when results exist) ──
            Rectangle {
                Layout.fillWidth: true; implicitHeight: repCol.implicitHeight + 32; radius: 12
                Layout.topMargin: 16
                color: ThemeEngine.bgCard; border { width: 1; color: ThemeEngine.colors.borderCard }
                ColumnLayout {
                    id: repCol; anchors { fill: parent; margins: 16 } spacing: 12
                    Label { text: Tr.report; font.family: ThemeEngine.monoFont; font.pixelSize: 15; font.weight: Font.DemiBold; color: ThemeEngine.textPrimary }
                    Label { text: Tr.reportExportHint; font.family: ThemeEngine.monoFont; font.pixelSize: 13; color: ThemeEngine.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    // Review Report button
                    Rectangle {
                        id: reviewBtn
                        Layout.fillWidth: true; implicitHeight: 48; radius: 10
                        color: Qt.alpha(ThemeEngine.cyan, 0.10)
                        border { width: 1; color: Qt.alpha(ThemeEngine.cyan, 0.35) }
                        RowLayout {
                            anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                            AppIcon { name: "report"; size: 18; color: ThemeEngine.cyan }
                            Item { width: 12 }
                            Label { Layout.fillWidth: true; text: Tr.reportReviewBtn; color: ThemeEngine.textPrimary; font.family: ThemeEngine.monoFont; font.pixelSize: 13; font.weight: Font.Medium }
                        }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: page.openPreview() }
                    }
                    // Share buttons
                    RowLayout {
                        Layout.fillWidth: true; spacing: 10
                        PreviewShareBtn {
                            Layout.fillWidth: true
                            iconName: "file-pdf"; label: isMobile ? Tr.sharePdfBtn : Tr.emailPdfBtn
                            accent: ThemeEngine.failRed; locked: !appState.isPremium
                            onClicked: page.doShare("pdf")
                        }
                        PreviewShareBtn {
                            Layout.fillWidth: true
                            iconName: "file-html"; label: isMobile ? Tr.shareHtmlBtn : Tr.emailHtmlBtn
                            accent: ThemeEngine.accentBlue; locked: !appState.isPremium
                            onClicked: page.doShare("html")
                        }
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
        return totalMs > 0 ? fmtDur(totalMs) : "—"
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
        return total > 0 ? fmtDur(total) : "—"
    }

    // ── Preview overlay (zoomable + share buttons) ──────────────────────
    Rectangle {
        id: previewOverlay
        parent: page.parent ? page.parent : page
        anchors.fill: parent
        color: Qt.alpha(ThemeEngine.colors.surface, 0.85)
        visible: page.previewVisible; z: 1000
        MouseArea { anchors.fill: parent }
        Rectangle {
            anchors { fill: parent; margins: isMobile ? 0 : 8 }
            radius: isMobile ? 0 : 12; color: ThemeEngine.colors.card; clip: true
            border { width: isMobile ? 0 : 2; color: ThemeEngine.colors.borderFocused }
            ColumnLayout {
                anchors { fill: parent; margins: 12 } spacing: 10
                // ── Header ────────────────────────────────────────────
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 48; radius: 8
                    color: Qt.alpha(ThemeEngine.cyan, 0.08)
                    RowLayout {
                        anchors { fill: parent; margins: 8 }
                        AppIcon { name: "report"; size: 20; color: ThemeEngine.cyan }
                        Item { width: 8 }
                        Label { Layout.fillWidth: true; text: Tr.reportReviewBtn; font.family: ThemeEngine.monoFont; font.pixelSize: 16; font.weight: Font.Bold; color: ThemeEngine.textPrimary }
                        Rectangle {
                            implicitWidth: 36; implicitHeight: 36; radius: 18; color: "transparent"
                            border { width: 1; color: ThemeEngine.colors.borderCard }
                            AppIcon { anchors.centerIn: parent; name: "close"; size: 14; color: ThemeEngine.failRed }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: page.previewVisible = false }
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
                // ── Share buttons (PDF + HTML) ─────────────────────────
                RowLayout {
                    Layout.fillWidth: true; spacing: 10
                    PreviewShareBtn {
                        Layout.fillWidth: true
                        iconName: "file-pdf"; label: isMobile ? Tr.sharePdfBtn : Tr.emailPdfBtn
                        accent: ThemeEngine.failRed; locked: !appState.isPremium
                        onClicked: page.doShare("pdf")
                    }
                    PreviewShareBtn {
                        Layout.fillWidth: true
                        iconName: "file-html"; label: isMobile ? Tr.shareHtmlBtn : Tr.emailHtmlBtn
                        accent: ThemeEngine.accentBlue; locked: !appState.isPremium
                        onClicked: page.doShare("html")
                    }
                }
            }
        }
    }

    // ── Toast banner ────────────────────────────────────────────────────
    Rectangle {
        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 24 }
        implicitWidth: toastLabel.implicitWidth + 24; implicitHeight: 36; radius: 18
        color: ThemeEngine.colors.card; visible: page.toast !== ""; z: 2000
        border { width: 1; color: ThemeEngine.colors.borderFocused }
        Label { id: toastLabel; anchors.centerIn: parent; text: page.toast; font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.textPrimary }
    }

    // ── Share subscription dialog ───────────────────────────────────────
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
            border { width: 1.5; color: ThemeEngine.colors.borderFocused }
            ColumnLayout {
                id: dlgCol
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: 20 } spacing: 14
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 60; height: 60; radius: 30
                    color: Qt.alpha(page.shareStage === 1 ? ThemeEngine.warnYellow : ThemeEngine.cyan, 0.12)
                    AppIcon { anchors.centerIn: parent; name: page.shareStage === 1 ? "badge-info" : "report"; size: 28; color: page.shareStage === 1 ? ThemeEngine.warnYellow : ThemeEngine.cyan }
                }
                Label { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: page.shareStage === 1 ? Tr.subscribeTitle : Tr.confirmShareTitle; font.family: ThemeEngine.monoFont; font.pixelSize: 17; font.weight: Font.Bold; color: ThemeEngine.textPrimary; wrapMode: Text.WordWrap }
                Label { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; text: page.shareStage === 1 ? Tr.subscribeBody : Tr.confirmShareBody; font.family: ThemeEngine.monoFont; font.pixelSize: 13; color: ThemeEngine.textSecondary; wrapMode: Text.WordWrap; lineHeight: 1.25 }
                RowLayout { Layout.fillWidth: true; Layout.topMargin: 4; spacing: 10
                    Rectangle {
                        Layout.fillWidth: true; implicitHeight: 42; radius: 8; color: "transparent"
                        border { width: 1; color: Qt.alpha(ThemeEngine.textSecondary, 0.5) }
                        Label { anchors.centerIn: parent; text: page.shareStage === 1 ? Tr.subscribeNotNow : Tr.dialogCancel; font.family: ThemeEngine.monoFont; font.pixelSize: 13; color: ThemeEngine.textSecondary }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: page.shareStage = 0 }
                    }
                    Rectangle {
                        Layout.fillWidth: true; implicitHeight: 42; radius: 8
                        color: page.shareStage === 1 ? ThemeEngine.warnYellow : ThemeEngine.cyan
                        Label { anchors.centerIn: parent; text: page.shareStage === 1 ? Tr.subscribeBtn : (page.isMobile ? Tr.shareBtn : Tr.emailBtn); font.family: ThemeEngine.monoFont; font.pixelSize: 13; font.weight: Font.DemiBold; color: ThemeEngine.bgDark }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { if (page.shareStage === 1) appState.requestSubscription(); else page.confirmShare() } }
                    }
                }
            }
        }
    }

    // ── Inline components ───────────────────────────────────────────────
    component PreviewShareBtn: Rectangle {
        id: psb
        property string iconName: ""; property string label: ""; property color accent: ThemeEngine.cyan; property bool locked: false
        signal clicked()
        implicitHeight: 42; radius: 8; opacity: locked ? 0.4 : 1.0
        color: Qt.alpha(accent, 0.10); border { width: 1; color: Qt.alpha(accent, 0.35) }
        RowLayout { anchors { fill: parent; leftMargin: 12; rightMargin: 12 } spacing: 8
            AppIcon { name: psb.iconName; size: 16; color: psb.accent }
            Label { Layout.fillWidth: true; text: psb.label; font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.textPrimary; elide: Text.ElideRight }
            AppIcon { name: psb.locked ? "badge-check" : ""; size: 14; color: ThemeEngine.warnYellow; visible: psb.locked }
        }
        MouseArea { anchors.fill: parent; enabled: !locked; cursorShape: Qt.PointingHandCursor; onClicked: psb.clicked() }
    }
    component DashboardGroupRow: Rectangle {
        property int groupIndex: 0
        // Compute the group stats once per row instead of 5x (one per badge).
        property var _stat: calcGroupStat(groupIndex)
        Layout.fillWidth: true; implicitHeight: grpCol.implicitHeight + 28; radius: 10
        Layout.bottomMargin: 8
        color: ThemeEngine.bgCard; border { width: 1; color: ThemeEngine.colors.borderCard }
        ColumnLayout {
            id: grpCol; anchors { fill: parent; margins: 14 } spacing: 0
            RowLayout {
                Rectangle { Layout.preferredWidth: 3; implicitHeight: 20; radius: 2; color: ThemeEngine.accentBlue }
                Item { width: 10 }
                Label { Layout.fillWidth: true; text: Tr.groupName(groupIndex); font.family: ThemeEngine.monoFont; font.pixelSize: 13; font.weight: Font.DemiBold; color: ThemeEngine.textPrimary }
                DashboardBadge { accent: ThemeEngine.passGreen;  v: _stat.pass }
                DashboardBadge { accent: ThemeEngine.warnYellow; v: _stat.warn }
                DashboardBadge { accent: ThemeEngine.failRed;   v: _stat.fail }
                DashboardBadge { accent: ThemeEngine.skipGray;  v: _stat.skip }
                DashboardBadge { accent: ThemeEngine.accentBlue; v: _stat.info||0 }
                Item { width: 8 }
                Label { text: getDurFromResults(groupIndex); font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.textSecondary }
            }
            Rectangle { Layout.fillWidth: true; implicitHeight: 4; radius: 2; color: ThemeEngine.colors.borderCard
                Rectangle {
                    height:4; radius:2
                    width: parent.width * (calcGroupStat(groupIndex).total > 0 ? (calcGroupStat(groupIndex).pass + calcGroupStat(groupIndex).warn + calcGroupStat(groupIndex).fail) / calcGroupStat(groupIndex).total : 0)
                    color: calcGroupStat(groupIndex).fail > 0 ? ThemeEngine.warnYellow : ThemeEngine.passGreen
                }
            }
            Item { Layout.preferredHeight: 8 }
            Repeater {
                id: dashResultsRepeater
                model: appState.resultsForGroup(groupIndex)
                delegate: RowLayout {
                    AppIcon { name: page.statusIcon(modelData.status); size: 10; color: page.statusColor(modelData.status) }
                    Item { width: 6 }
                    Label { Layout.fillWidth: true; text: modelData.displayName||""; font.family:ThemeEngine.monoFont; font.pixelSize:11; color:ThemeEngine.textSecondary; elide:Text.ElideRight }
                    Label { text: page.fmtDur(modelData.durationMs); font.family:ThemeEngine.monoFont; font.pixelSize:10; color:Qt.alpha(ThemeEngine.textSecondary,0.6) }
                }
            }
        }
    }

    component DashboardBadge: Rectangle {
        property color accent: ThemeEngine.passGreen; property int v: 0
        visible: v > 0; implicitWidth: 22; implicitHeight: 16; radius: 4; color: Qt.alpha(accent, 0.15)
        Label { anchors.centerIn: parent; text: v; font.family: ThemeEngine.monoFont; font.pixelSize: 10; font.weight: Font.Bold; color: accent }
    }

    component SummaryStat: RowLayout {
        property string appIcon: ""; property color clr: ThemeEngine.cyan; property var val: 0; property string lbl: ""
        Layout.fillWidth: true
        spacing: 10
        AppIcon { name: appIcon; size: 16; color: clr; Layout.alignment: Qt.AlignVCenter }
        Label {
            Layout.fillWidth: true
            text: lbl; font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.textSecondary
            elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
        }
        Label {
            text: val; font.family: ThemeEngine.monoFont; font.pixelSize: 16; font.weight: Font.Bold; color: clr
            horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
        }
    }
}