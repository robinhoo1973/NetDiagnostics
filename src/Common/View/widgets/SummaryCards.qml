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
        Label { Layout.fillWidth: true; text: Tr.summary; font.family: ThemeEngine.monoFont; font.pixelSize: 11; font.weight: Font.DemiBold; color: ThemeEngine.textSecondary }
        Label { text: Tr.totalDiagsLabel + ": " + (pass+warn+fail+skip+info); font.family: ThemeEngine.monoFont; font.pixelSize: 10; color: ThemeEngine.textSecondary }
    }
    Item { Layout.preferredHeight: 6 }

    // 5 result types — each with colored icon + badge count
    SummaryCard { Layout.fillWidth: true; accent: ThemeEngine.passGreen;  iconName: "badge-check";   label: Tr.summaryPass;    count: summaryRoot.pass }
    SummaryCard { Layout.fillWidth: true; accent: ThemeEngine.accentBlue;iconName: "badge-info";    label: Tr.summaryInfo;    count: summaryRoot.info }
    SummaryCard { Layout.fillWidth: true; accent: ThemeEngine.warnYellow; iconName: "badge-warning"; label: Tr.summaryWarning; count: summaryRoot.warn }
    SummaryCard { Layout.fillWidth: true; accent: ThemeEngine.failRed;   iconName: "badge-close";   label: Tr.summaryFail;    count: summaryRoot.fail }
    SummaryCard { Layout.fillWidth: true; accent: ThemeEngine.skipGray;  iconName: "badge-skip";    label: Tr.summarySkipped; count: summaryRoot.skip }

    Connections {
        target: appState
        function onProgressChanged() { refresh() }
        function onDiagCompleted() { refresh() }
        function onResultsReset() { pass=warn=fail=skip=info=0 }
    }
    // 5WHY: zero-padding via .slice(-2) for alignment — removed
    // to prevent truncation at >=100 (same bug as DiagnosticScreen).
    // Right-alignment already handles single-digit display correctly.
    function _pad2Fixed(n) { return (n < 10 ? "0" : "") + n }
    Component.onCompleted: refresh()
    function refresh() {
        pass=0; warn=0; fail=0; skip=0; info=0
        for (var g=0; g<appState.groupLabels.length; g++) {
            var s = appState.groupStats(g)
            pass += s.pass; warn += s.warn; fail += s.fail; skip += s.skip; info += (s.info||0)
        }
    }

    component SummaryCard: Rectangle {
        property color accent: ThemeEngine.passGreen
        property string label: ""
        property string iconName: "badge-info"
        property int count: 0
        property bool rightAlign: false
        implicitHeight: 32; radius: 6; Layout.topMargin: 2
        color: Qt.alpha(accent, 0.06)
        border { width: 1; color: Qt.alpha(accent, 0.2) }

        RowLayout {
            anchors { fill: parent; leftMargin: 8; rightMargin: 8 }
            AppIcon {
                name: iconName
                size: 14; color: accent
            }
            Item { Layout.fillWidth: true }
            Label {
                text: label; font.family: ThemeEngine.monoFont; font.pixelSize: 10; font.weight: Font.Medium
                color: Qt.alpha(ThemeEngine.textSecondary, 0.8)
            }
            Item { width: 8 }
            Label {
                text: _pad2Fixed(count)
                font.family: ThemeEngine.monoFont; font.pixelSize: 16; font.weight: Font.Bold; color: accent
                horizontalAlignment: Text.AlignRight
            }
        }
    }
}
