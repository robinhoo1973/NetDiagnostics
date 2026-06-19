import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../widgets"

// ── Flutter DashboardScreen 1:1 — with AppBar + timestamp + real durations ──
Item {
    id: page
    objectName: "dashboard"
    // ── Polled state (C++ signals unreliable on ARM64) ────────────────
    property int _runStatus: 0
    property int _totalCompleted: 0
    Timer {
        interval: 200; running: true; repeat: true
        onTriggered: {
            _runStatus = appState.runStatus
            _totalCompleted = appState.totalCompleted
        }
    }
    property bool hasData: _totalCompleted > 0 && _runStatus === 2
    readonly property var allStats: appState.allGroupStats || []

    function statusIcon(s) { switch(s) { case 0: return "✓"; case 1: return "⚠"; case 2: return "✗"; case 3: return "⊖"; default: return "ⓘ" } }
    function statusColor(s) { switch(s) { case 0: return Theme.passGreen; case 1: return Theme.warnYellow; case 2: return Theme.failRed; case 3: return Theme.skipGray; default: return Theme.accentBlue } }
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

    // AppBar (Flutter: Scaffold.appBar with title + reset button)
    Rectangle {
        id: appBar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        implicitHeight: 52; color: "#1A1A2E"
        border { width: 1; color: "#3A3A5A" }
        RowLayout {
            anchors { fill: parent; leftMargin: 16; rightMargin: 16 }
            AppIcon { name: "dashboard"; size: 20; color: Theme.cyan }
            Item { width: 10 }
            Label { text: "Dashboard"; font.family: "JetBrains Mono"; font.pixelSize: 15; font.weight: Font.DemiBold; color: Theme.textPrimary }
            Item { Layout.fillWidth: true }
            // Reset button (Flutter: IconButton refresh)
            Rectangle {
                visible: hasData
                implicitWidth: 60; implicitHeight: 32; radius: 6; color: "transparent"
                border { width: 1; color: "#5A5A7A" }
                MouseArea { anchors.fill: parent; onClicked: appState.reset() }
                Label { anchors.centerIn: parent; text: "↻ Reset"; font.family: "JetBrains Mono"; font.pixelSize: 12; color: Theme.textSecondary }
            }
        }
    }

    // Empty state
    Column {
        anchors.centerIn: parent; spacing: 16; visible: !hasData
        AppIcon { anchors.horizontalCenter: parent.horizontalCenter; name: "dashboard"; size: 80; color: Qt.alpha(Theme.textSecondary, 0.2) }
        Label { anchors.horizontalCenter: parent.horizontalCenter; text: "No diagnostic data yet"; font.family: "JetBrains Mono"; font.pixelSize: 18; font.weight: Font.Medium; color: Qt.alpha(Theme.textSecondary, 0.6) }
        Label { anchors.horizontalCenter: parent.horizontalCenter; text: "Run a diagnostic from the Diagnostics screen\nto see results here."; font.family: "JetBrains Mono"; font.pixelSize: 13; color: Qt.alpha(Theme.textSecondary, 0.4); horizontalAlignment: Text.AlignHCenter; lineHeight: 1.5 }
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
                color: Theme.bgCard; border { width: 1; color: "#2A2A4A" }
                RowLayout {
                    id: infoCol
                    anchors { fill: parent; margins: 16 }
                    AppIcon { name: "check"; size: 28; color: Theme.passGreen }
                    Item { width: 14 }
                    ColumnLayout { spacing: 4
                        Label { text: "Diagnostic Run Complete"; font.family: "JetBrains Mono"; font.pixelSize: 16; font.weight: Font.DemiBold; color: Theme.textPrimary }
                        RowLayout { spacing: 4
                            AppIcon { name: "target"; size: 12; color: Theme.textSecondary }
                            Label { text: "Target: " + (appState.target || "N/A"); font.family: "JetBrains Mono"; font.pixelSize: 12; color: Theme.textSecondary }
                        }
                        RowLayout { spacing: 4
                            AppIcon { name: "timer"; size: 12; color: Theme.textSecondary }
                            Label { text: fmtTimestamp(); font.family: "JetBrains Mono"; font.pixelSize: 12; color: Theme.textSecondary }
                        }
                    }
                }
            }
            Item { Layout.preferredHeight: 24 }

            SummaryCards { Layout.fillWidth: true }
            Item { Layout.preferredHeight: 32 }

            // ── Per-Group Results header ────────────────────────────────
            Label { text: "Per-Group Results"; font.family: "JetBrains Mono"; font.pixelSize: 15; font.weight: Font.DemiBold; color: Theme.textPrimary }
            Item { Layout.preferredHeight: 12 }

            Repeater {
                model: appState.groupLabels.length
                delegate: DashboardGroupRow { groupIndex: index; Layout.fillWidth: true }
            }
            Item { Layout.preferredHeight: 32 }

            // ── Overall Summary (Flutter: _buildOverallSection) ──────────
            Rectangle {
                Layout.fillWidth: true; implicitHeight: sumCol.implicitHeight + 32; radius: 12
                color: Theme.bgCard; border { width: 1; color: "#2A2A4A" }
                ColumnLayout { id: sumCol; anchors { fill: parent; margins: 16 }
                    Label { text: "Summary"; font.family: "JetBrains Mono"; font.pixelSize: 15; font.weight: Font.DemiBold; color: Theme.textPrimary }
                    Item { Layout.preferredHeight: 16 }
                    RowLayout {
                    SummaryStat { appIcon: "config"; clr: Theme.cyan; val: appState.totalTests; lbl: "Total Tests" }
                    Item { width: 24 }
                    SummaryStat { appIcon: "timer"; clr: Theme.accentBlue; val: calcTotalTime(); lbl: "Total Time" }
                    Item { width: 24 }
                    SummaryStat { appIcon: "check"; clr: Theme.passGreen; val: _totalCompleted; lbl: "Completed" }
                    }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#2A2A4A"; visible: _totalCompleted > 0 }
                    Item { Layout.preferredHeight: 12; visible: _totalCompleted > 0 }
                    Label { text: "Layer Timings"; font.family: "JetBrains Mono"; font.pixelSize: 12; font.weight: Font.DemiBold; color: Theme.textSecondary; visible: _totalCompleted > 0 }
                    Item { Layout.preferredHeight: 8; visible: _totalCompleted > 0 }
                    Repeater {
                        model: appState.groupLabels.length
                        delegate: RowLayout {
                            visible: hasData
                            Rectangle { implicitWidth: 8; implicitHeight: 8; radius: 2; color: Theme.accentBlue }
                            Item { width: 10 }
                            Label { Layout.fillWidth: true; text: appState.groupLabels[index] || ""; font.family: "JetBrains Mono"; font.pixelSize: 12; color: Theme.textPrimary }
                            Label { text: calcLayerTiming(index); font.family: "JetBrains Mono"; font.pixelSize: 12; color: Theme.textSecondary }
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
        return s || {pass:0,warn:0,fail:0,skip:0,total:0,enabled:0}
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

    // ── Inline components ───────────────────────────────────────────────
    component DashboardGroupRow: Rectangle {
        property int groupIndex: 0
        Layout.fillWidth: true; implicitHeight: grpCol.implicitHeight + 28; radius: 10
        Layout.bottomMargin: 8
        color: Theme.bgCard; border { width: 1; color: "#2A2A4A" }
        ColumnLayout {
            id: grpCol; anchors { fill: parent; margins: 14 } spacing: 0
            RowLayout {
                Rectangle { Layout.preferredWidth: 3; implicitHeight: 20; radius: 2; color: Theme.accentBlue }
                Item { width: 10 }
                Label { Layout.fillWidth: true; text: appState.groupLabels[groupIndex] || ""; font.family: "JetBrains Mono"; font.pixelSize: 13; font.weight: Font.DemiBold; color: Theme.textPrimary }
                DashboardBadge { accent: Theme.passGreen;  v: calcGroupStat(groupIndex).pass }
                DashboardBadge { accent: Theme.warnYellow; v: calcGroupStat(groupIndex).warn }
                DashboardBadge { accent: Theme.failRed;   v: calcGroupStat(groupIndex).fail }
                DashboardBadge { accent: Theme.skipGray;  v: calcGroupStat(groupIndex).skip }
                Item { width: 8 }
                Label { text: getDurFromResults(groupIndex); font.family: "JetBrains Mono"; font.pixelSize: 11; color: Theme.textSecondary }
            }
            Rectangle { Layout.fillWidth: true; implicitHeight: 4; radius: 2; color: "#2A2A4A"
                Rectangle {
                    height:4; radius:2
                    width: parent.width * (calcGroupStat(groupIndex).total > 0 ? (calcGroupStat(groupIndex).pass + calcGroupStat(groupIndex).warn + calcGroupStat(groupIndex).fail) / calcGroupStat(groupIndex).total : 0)
                    color: calcGroupStat(groupIndex).fail > 0 ? Theme.warnYellow : Theme.passGreen
                }
            }
            Item { Layout.preferredHeight: 8 }
            Repeater {
                id: dashResultsRepeater
                model: appState.resultsForGroup(groupIndex)
                delegate: RowLayout {
                    Label { text: page.statusIcon(modelData.status); font.pixelSize: 12; color: page.statusColor(modelData.status) }
                    Item { width: 6 }
                    Label { Layout.fillWidth: true; text: modelData.displayName||""; font.family:"JetBrains Mono"; font.pixelSize:11; color:Theme.textSecondary; elide:Text.ElideRight }
                    Label { text: page.fmtDur(modelData.durationMs); font.family:"JetBrains Mono"; font.pixelSize:10; color:Qt.alpha(Theme.textSecondary,0.6) }
                }
            }
        }
    }

    component DashboardBadge: Rectangle {
        property color accent: Theme.passGreen; property int v: 0
        visible: v > 0; implicitWidth: 22; implicitHeight: 16; radius: 4; color: Qt.alpha(accent, 0.15)
        Label { anchors.centerIn: parent; text: v; font.family: "JetBrains Mono"; font.pixelSize: 10; font.weight: Font.Bold; color: accent }
    }

    component SummaryStat: RowLayout {
        property string appIcon: ""; property color clr: Theme.cyan; property var val: 0; property string lbl: ""
        AppIcon { name: appIcon; size: 20; color: clr }
        Item { width: 8 }
        ColumnLayout { spacing: 0
            Label { text: val; font.family: "JetBrains Mono"; font.pixelSize: 22; font.weight: Font.Bold; color: Theme.textPrimary }
            Label { text: lbl; font.family: "JetBrains Mono"; font.pixelSize: 10; color: Theme.textSecondary }
        }
    }
}
