import QtQuick
import "../theme"
import QtQuick.Controls
import QtQuick.Layouts

// ── SummaryCards — shared data, one row per category ─────────────────
ColumnLayout {
    id: summaryRoot
    spacing: 0
    property int pass: 0; property int warn: 0; property int fail: 0; property int skip: 0; property int info: 0
    property bool compact: false

    // Header: "Summary" + "Total: N"
    RowLayout {
        Label { Layout.fillWidth: true; text: Tr.summary; font.family: Theme.monoFont; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.textSecondary }
        Label { text: Tr.totalDiagsLabel + (pass+warn+fail+skip+info); font.family: Theme.monoFont; font.pixelSize: 10; color: Theme.textSecondary }
    }
    Item { Layout.preferredHeight: 6 }

    SummaryCard { Layout.fillWidth: true; accent: Theme.passGreen;  label: Tr.summaryPass;    count: summaryRoot.pass;  rightAlign: summaryRoot.compact }
    SummaryCard { Layout.fillWidth: true; accent: Theme.accentBlue;label: Tr.summaryInfo;    count: summaryRoot.info;  rightAlign: summaryRoot.compact }
    SummaryCard { Layout.fillWidth: true; accent: Theme.warnYellow; label: Tr.summaryWarning; count: summaryRoot.warn;  rightAlign: summaryRoot.compact }
    SummaryCard { Layout.fillWidth: true; accent: Theme.failRed;   label: Tr.summaryFail;    count: summaryRoot.fail;  rightAlign: summaryRoot.compact }
    SummaryCard { Layout.fillWidth: true; accent: Theme.skipGray;  label: Tr.summarySkipped; count: summaryRoot.skip;  rightAlign: summaryRoot.compact }

    Connections {
        target: appState
        function onProgressChanged() { refresh() }
        function onDiagCompleted() { refresh() }
        function onResultsReset() { pass=warn=fail=skip=info=0 }
    }
    Component.onCompleted: refresh()
    function refresh() {
        pass=0; warn=0; fail=0; skip=0; info=0
        for (var g=0; g<appState.groupLabels.length; g++) {
            var s = appState.groupStats(g)
            pass += s.pass; warn += s.warn; fail += s.fail; skip += s.skip; info += (s.info||0)
        }
    }

    component SummaryCard: Rectangle {
        property color accent: Theme.passGreen
        property string label: ""
        property int count: 0
        property bool rightAlign: false
        implicitHeight: 32; radius: 6; Layout.topMargin: 2
        color: Qt.alpha(accent, 0.06)
        border { width: 1; color: Qt.alpha(accent, 0.2) }

        RowLayout {
            anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
            AppIcon {
                name: label === "Pass" ? "badge-check" : (label === "Warning" ? "badge-warning" : (label === "Fail" ? "badge-close" : (label === "Skipped" ? "badge-skip" : "badge-info")))
                size: 14; color: accent
            }
            Item { Layout.fillWidth: true }
            Label {
                text: label; font.family: Theme.monoFont; font.pixelSize: 10; font.weight: Font.Medium
                color: Qt.alpha(Theme.textSecondary, 0.8)
            }
            Item { width: 8 }
            Label {
                text: ("   " + count).slice(-3)
                font.family: Theme.monoFont; font.pixelSize: 16; font.weight: Font.Bold; color: accent
                horizontalAlignment: Text.AlignRight
            }
        }
    }
}
