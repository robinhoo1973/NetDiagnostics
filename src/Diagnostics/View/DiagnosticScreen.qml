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
    readonly property bool isMobile: Qt.platform.os === "ios" || Qt.platform.os === "android"

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
    property bool _snapG0chk: true; property bool _snapG1chk: true
    property bool _snapG2chk: true; property bool _snapG3chk: false
    property bool _snapG4chk: false
    property bool _snapG3en: false; property bool _snapG4en: false
    property int _snapVersion: 0

    function takeSnapshot() {
        _snapG0chk = appState.isGroupAllEnabled(0) || appState.isGroupAnyEnabled(0)
        _snapG1chk = appState.isGroupAllEnabled(1) || appState.isGroupAnyEnabled(1)
        _snapG2chk = appState.isGroupAllEnabled(2) || appState.isGroupAnyEnabled(2)
        _snapG3chk = appState.isGroupAllEnabled(3) || appState.isGroupAnyEnabled(3)
        _snapG4chk = appState.isGroupAllEnabled(4) || appState.isGroupAnyEnabled(4)
        _snapG3en = !appState.isTargetEmpty()
        _snapG4en = appState.hasUrlScheme()
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
    // groupStats(-1) sums G0-G4 via the C++ aggregate branch.
    property int __aggPass: { let _ = appState.totalCompleted; return (appState.groupStats(-1).pass||0) }
    property int __aggInfo: { let _ = appState.totalCompleted; return (appState.groupStats(-1).info||0) }
    property int __aggWarn: { let _ = appState.totalCompleted; return (appState.groupStats(-1).warn||0) }
    property int __aggFail: { let _ = appState.totalCompleted; return (appState.groupStats(-1).fail||0) }
    property int __aggSkip: { let _ = appState.totalCompleted; return (appState.groupStats(-1).skip||0) }

    property var currentDetail: ({})
    property var visibleGroups: {
        let _ = _snapVersion
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
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 48; color: ThemeEngine.colors.navBar
            border { width: 1; color: ThemeEngine.colors.borderCard }
            RowLayout {
                anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
                AppIcon { name: "diagnostics"; size: 20; color: ThemeEngine.cyan }
                Item { width: 10 }
                Label { text: Tr.diagnostics; font.family: ThemeEngine.monoFont; font.pixelSize: 15; font.weight: Font.DemiBold; color: ThemeEngine.textPrimary }
                Item { Layout.fillWidth: true }
            }
        }

        // ═══════════════ TOOLBAR ═══════════════════════════════════════
        DiagnosticToolbar {
            Layout.fillWidth: true
            wide: page.wide
        }

        // ═══════════════ RESULTS HEADER ════════════════════════════════
        // Status bar — visible during/after run.
        // Desktop: single row with 5 status badges inline (icon + count).
        // Phone portrait: title on row 1, 5 colored-icon badges on row 2
        // matching the DiagGroupPanel StatusBadge pattern.
        Rectangle {
            Layout.fillWidth: true; implicitHeight: page.wide ? 36 : 56
            color: ThemeEngine.colors.navBar
            visible: appState.totalCompleted > 0 || appState.runStatus === 1
            ColumnLayout {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12; topMargin: 4; bottomMargin: 4 }
                spacing: 2
                // Row 1 — status label + progress count (+ badges inline on desktop)
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
                            // 5WHY: When the animation stops, rotation stays at the last angle
                            // (e.g. 247°), leaving the diagnostic icon skewed. Reset to 0.
                            onStopped: statusSpinner.rotation = 0
                        }
                    }
                    Item { width: 4 }
                    Label {
                        // 5WHY: Was mapping Error(4)→Tr.cancelled and Cancelled(3)→Tr.results.
                        // Now correctly shows all 5 states: Running/Complete/Cancelled/Error/Idle.
                        text: appState.runStatus === 1 ? Tr.runningDots :
                              appState.runStatus === 2 ? Tr.complete :
                              appState.runStatus === 3 ? Tr.cancelled :
                              appState.runStatus === 4 ? Tr.errorStatus : Tr.results
                        font.family: ThemeEngine.monoFont; font.pixelSize: 13; font.weight: Font.DemiBold
                        color: ThemeEngine.colors.textPrimary
                    }
                    Label {
                        visible: appState.runStatus === 1 && appState.totalDiags > 0
                        text: appState.totalCompleted + " / " + appState.totalDiags
                        font.family: ThemeEngine.monoFont; font.pixelSize: 12; font.weight: Font.DemiBold
                        color: ThemeEngine.cyan
                    }
                    Item { Layout.fillWidth: true }
                    // 5 status badges inline — desktop only
                    RowLayout {
                        spacing: 4; visible: page.wide && appState.totalCompleted > 0
                        // Inline StatusBadge (same shape as DiagGroupPanel.StatusBadge)
                        RowLayout { spacing: 2; AppIcon { name: "badge-check";   size: 10; color: ThemeEngine.passGreen  } Label { text: ("  " + __aggPass).slice(-2); font.family: ThemeEngine.monoFont; font.pixelSize: 10; font.weight: Font.Bold; color: ThemeEngine.passGreen  } }
                        RowLayout { spacing: 2; AppIcon { name: "badge-info";    size: 10; color: ThemeEngine.accentBlue } Label { text: ("  " + __aggInfo).slice(-2); font.family: ThemeEngine.monoFont; font.pixelSize: 10; font.weight: Font.Bold; color: ThemeEngine.accentBlue } }
                        RowLayout { spacing: 2; AppIcon { name: "badge-warning"; size: 10; color: ThemeEngine.warnYellow } Label { text: ("  " + __aggWarn).slice(-2); font.family: ThemeEngine.monoFont; font.pixelSize: 10; font.weight: Font.Bold; color: ThemeEngine.warnYellow } }
                        RowLayout { spacing: 2; AppIcon { name: "badge-close";   size: 10; color: ThemeEngine.failRed    } Label { text: ("  " + __aggFail).slice(-2); font.family: ThemeEngine.monoFont; font.pixelSize: 10; font.weight: Font.Bold; color: ThemeEngine.failRed    } }
                        RowLayout { spacing: 2; AppIcon { name: "badge-skip";    size: 10; color: ThemeEngine.skipGray   } Label { text: ("  " + __aggSkip).slice(-2); font.family: ThemeEngine.monoFont; font.pixelSize: 10; font.weight: Font.Bold; color: ThemeEngine.skipGray   } }
                    }
                }
                // Row 2 — 5 status badges on their own line (phone portrait only)
                RowLayout {
                    spacing: 4; visible: !page.wide && appState.totalCompleted > 0
                    Item { width: 20 }  // indent to align with status label
                    RowLayout { spacing: 2; AppIcon { name: "badge-check";   size: 10; color: ThemeEngine.passGreen  } Label { text: ("  " + __aggPass).slice(-2); font.family: ThemeEngine.monoFont; font.pixelSize: 10; font.weight: Font.Bold; color: ThemeEngine.passGreen  } }
                    RowLayout { spacing: 2; AppIcon { name: "badge-info";    size: 10; color: ThemeEngine.accentBlue } Label { text: ("  " + __aggInfo).slice(-2); font.family: ThemeEngine.monoFont; font.pixelSize: 10; font.weight: Font.Bold; color: ThemeEngine.accentBlue } }
                    RowLayout { spacing: 2; AppIcon { name: "badge-warning"; size: 10; color: ThemeEngine.warnYellow } Label { text: ("  " + __aggWarn).slice(-2); font.family: ThemeEngine.monoFont; font.pixelSize: 10; font.weight: Font.Bold; color: ThemeEngine.warnYellow } }
                    RowLayout { spacing: 2; AppIcon { name: "badge-close";   size: 10; color: ThemeEngine.failRed    } Label { text: ("  " + __aggFail).slice(-2); font.family: ThemeEngine.monoFont; font.pixelSize: 10; font.weight: Font.Bold; color: ThemeEngine.failRed    } }
                    RowLayout { spacing: 2; AppIcon { name: "badge-skip";    size: 10; color: ThemeEngine.skipGray   } Label { text: ("  " + __aggSkip).slice(-2); font.family: ThemeEngine.monoFont; font.pixelSize: 10; font.weight: Font.Bold; color: ThemeEngine.skipGray   } }
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

            // ── Share buttons (visible when run completes with results) ──
            RowLayout {
                Layout.fillWidth: true; Layout.topMargin: 12; spacing: 10
                visible: appState.runStatus === 2 && appState.totalCompleted > 0 && appState.totalCompleted >= appState.totalDiags
                DiagShareBtn {
                    Layout.fillWidth: true
                    iconName: "file-pdf"; label: page.isMobile ? Tr.sharePdfBtn : Tr.emailPdfBtn
                    accent: ThemeEngine.failRed; locked: !appState.isPremium
                    onClicked: page.doShare("pdf")
                }
                DiagShareBtn {
                    Layout.fillWidth: true
                    iconName: "file-html"; label: page.isMobile ? Tr.shareHtmlBtn : Tr.emailHtmlBtn
                    accent: ThemeEngine.accentBlue; locked: !appState.isPremium
                    onClicked: page.doShare("html")
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
            if (event.key === Qt.Key_Escape) {
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

            // 5WHY: Close button was invisible at rest — subtle borderCard background
            // + textPrimary icon blended into the card surface, forcing users to
            // hover to discover the dismiss affordance.  Modal overlays need an
            // OBVIOUS, always-visible close button.
            // Now: matches ReportScreen.qml's proven pattern — failRed icon at rest
            // for high contrast against any theme (dark/light), with intensified
            // red background on hover.  No hover required to see the button.
            Rectangle {
                anchors { top: parent.top; right: parent.right; topMargin: 8; rightMargin: 8 }
                width: 44; height: 44; radius: 22
                color: closeBtnArea.containsMouse ? Qt.alpha(ThemeEngine.failRed, 0.30) : Qt.alpha(ThemeEngine.failRed, 0.12)
                border { width: 1; color: closeBtnArea.containsMouse ? ThemeEngine.failRed : Qt.alpha(ThemeEngine.failRed, 0.35) }
                AppIcon {
                    anchors.centerIn: parent
                    name: "close"; size: 18
                    color: closeBtnArea.containsMouse ? Qt.lighter(ThemeEngine.failRed, 1.3) : ThemeEngine.failRed
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

    // ── Share button component (PDF/HTML) ───────────────────────────
    component DiagShareBtn: Rectangle {
        id: dsb
        property string iconName: ""; property string label: ""; property color accent: ThemeEngine.cyan; property bool locked: false
        signal clicked()
        implicitHeight: 42; radius: 8; opacity: locked ? 0.4 : 1.0
        color: Qt.alpha(accent, 0.10); border { width: 1; color: Qt.alpha(accent, 0.35) }
        RowLayout { anchors { fill: parent; leftMargin: 12; rightMargin: 12 } spacing: 8
            AppIcon { name: dsb.iconName; size: 16; color: dsb.accent }
            Label { Layout.fillWidth: true; text: dsb.label; font.family: ThemeEngine.monoFont; font.pixelSize: 12; color: ThemeEngine.textPrimary; elide: Text.ElideRight }
            AppIcon { name: dsb.locked ? "badge-check" : ""; size: 14; color: ThemeEngine.warnYellow; visible: dsb.locked }
        }
        MouseArea { anchors.fill: parent; enabled: !locked; cursorShape: Qt.PointingHandCursor; onClicked: dsb.clicked() }
    }
}
