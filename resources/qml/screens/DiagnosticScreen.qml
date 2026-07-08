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

    property var currentDetail: ({})
    property var visibleGroups: {
        let _ = _snapVersion
        var g = []
        for (var i = 0; i < 5; i++) {
            var s = appState.groupStats(i)
            if ((s.enabled || 0) > 0 || (s.total || 0) > 0) g.push(i)
        }
        return g
    }

    // ── Single-column layout ──────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent; spacing: 0

        // ═══════════════ TOOLBAR ═══════════════════════════════════════
        DiagnosticToolbar {
            Layout.fillWidth: true
            wide: page.wide
        }

        // ═══════════════ RESULTS HEADER ════════════════════════════════
        // Status bar — visible during/after run
        Rectangle {
            Layout.fillWidth: true; implicitHeight: 36
            color: ThemeEngine.colors.navBar
            visible: appState.totalCompleted > 0 || appState.runStatus === 1
            RowLayout {
                anchors { fill: parent; leftMargin: 12; rightMargin: 12 }
                AppIcon {
                    name: appState.runStatus === 1 ? "spinner" : "diagnostics"
                    size: 16
                    color: appState.runStatus === 1 ? ThemeEngine.cyan : ThemeEngine.colors.primary
                }
                Item { width: 8 }
                Label {
                    text: appState.runStatus === 1 ? Tr.runningDots :
                          appState.runStatus === 2 ? Tr.complete :
                          appState.runStatus === 3 ? Tr.cancelled : Tr.results
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
                // Summary stats — compact badges
                RowLayout {
                    spacing: 4; visible: appState.totalCompleted > 0
                    Rectangle { implicitWidth: 28; implicitHeight: 18; radius: 4; color: Qt.alpha(ThemeEngine.passGreen, 0.15)
                        Label { anchors.centerIn: parent; text: appState.groupStats(-1).pass||"0"; font.family: ThemeEngine.monoFont; font.pixelSize: 10; color: ThemeEngine.passGreen; font.weight: Font.Bold } }
                    Rectangle { implicitWidth: 28; implicitHeight: 18; radius: 4; color: Qt.alpha(ThemeEngine.warnYellow, 0.15)
                        Label { anchors.centerIn: parent; text: appState.groupStats(-1).warn||"0"; font.family: ThemeEngine.monoFont; font.pixelSize: 10; color: ThemeEngine.warnYellow; font.weight: Font.Bold } }
                    Rectangle { implicitWidth: 28; implicitHeight: 18; radius: 4; color: Qt.alpha(ThemeEngine.failRed, 0.15)
                        Label { anchors.centerIn: parent; text: appState.groupStats(-1).fail||"0"; font.family: ThemeEngine.monoFont; font.pixelSize: 10; color: ThemeEngine.failRed; font.weight: Font.Bold } }
                }
            }
        }

        // ═══════════════ RESULTS ═══════════════════════════════════════
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true

            // Empty state
            Column {
                anchors.centerIn: parent; spacing: 16
                visible: appState.runStatus === 0 && appState.totalCompleted === 0
                AppIcon { anchors.horizontalCenter: parent.horizontalCenter; name: "diagnostics"; size: 80; color: Qt.alpha(ThemeEngine.colors.textPrimary, 0.1) }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Tr.runDiag
                    font.family: ThemeEngine.monoFont; font.pixelSize: 15; font.weight: Font.Medium
                    color: Qt.alpha(ThemeEngine.colors.textSecondary, 0.5)
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

    // ═══════════════ DETAIL OVERLAY ═══════════════════════════════════
    Rectangle {
        id: detailOverlay
        parent: page.parent ? page.parent : page
        anchors.fill: parent
        color: "#88000000"; visible: false; z: 1000
        onVisibleChanged: {
            if (!visible) { dtTitle.text=""; dtStatus.text=""; dtSummary.text=""; dtOutput.text=""; page.currentDetail = {} }
        }
        MouseArea { anchors.fill: parent; onClicked: detailOverlay.visible = false }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(700, parent.width - 20)
            height: Math.min(parent.height - 40, 620)
            radius: 14
            color: ThemeEngine.colors.card
            border { width: 1.5; color: ThemeEngine.colors.borderFocused }

            Rectangle {
                anchors { top: parent.top; right: parent.right; topMargin: 10; rightMargin: 10 }
                width: 28; height: 28; radius: 14; color: ThemeEngine.failRed
                Label {
                    anchors.centerIn: parent
                    text: "×"; font.family: ThemeEngine.monoFont; font.pixelSize: 16
                    font.weight: Font.Bold; color: "white"
                }
                MouseArea { anchors.fill: parent; onClicked: detailOverlay.visible = false }
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
}
