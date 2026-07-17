import QtQuick
import "../theme"
import QtQuick.Controls
import QtQuick.Layouts

// ── SummaryCards — shared data, one row per category ─────────────────
ColumnLayout {
    id: summaryRoot
    spacing: 0
    property int pass: 0; property int warn: 0; property int fail: 0; property int skip: 0; property int info: 0

    // Header: "Summary" + "Total: N"
    RowLayout {
        Label { Layout.fillWidth: true; text: Tr.summary; font.family: ThemeEngine.monoFont; font.pixelSize: 11; font.weight: Font.DemiBold; color: ThemeEngine.textSecondary }
        Label { text: Tr.totalDiagsLabel + ": " + (pass+warn+fail+skip+info); font.family: ThemeEngine.monoFont; font.pixelSize: 10; color: ThemeEngine.textSecondary }
    }
    Item { Layout.preferredHeight: 6 }

    // 5WHY: showing five zero-count cards is noisy for first-time users.
    // Empty state shows a single hint instead of a wall of "0" values.
    Label {
        Layout.fillWidth: true; Layout.topMargin: 4
        visible: (pass+warn+fail+skip+info) === 0
        text: Tr.runDiag; font.family: ThemeEngine.monoFont; font.pixelSize: 11
        color: Qt.alpha(ThemeEngine.textSecondary, 0.5)
        horizontalAlignment: Text.AlignHCenter
    }
    // 5 result types — each with colored icon + badge count
    SummaryCard { Layout.fillWidth: true; accent: ThemeEngine.passGreen;  iconName: "badge-check";   label: Tr.summaryPass;    count: summaryRoot.pass;  visible: (pass+warn+fail+skip+info) > 0 }
    SummaryCard { Layout.fillWidth: true; accent: ThemeEngine.infoBlue;iconName: "badge-info";    label: Tr.summaryInfo;    count: summaryRoot.info;  visible: (pass+warn+fail+skip+info) > 0 }
    SummaryCard { Layout.fillWidth: true; accent: ThemeEngine.warnYellow; iconName: "badge-warning"; label: Tr.summaryWarning; count: summaryRoot.warn;   visible: (pass+warn+fail+skip+info) > 0 }
    SummaryCard { Layout.fillWidth: true; accent: ThemeEngine.failRed;   iconName: "badge-close";   label: Tr.summaryFail;    count: summaryRoot.fail;   visible: (pass+warn+fail+skip+info) > 0 }
    SummaryCard { Layout.fillWidth: true; accent: ThemeEngine.skipGray;  iconName: "badge-skip";    label: Tr.summarySkipped; count: summaryRoot.skip;   visible: (pass+warn+fail+skip+info) > 0 }

    Connections {
        target: appState
        function onProgressChanged() { refresh() }
        function onDiagCompleted() { refresh() }
        function onResultsReset() { pass=warn=fail=skip=info=0 }
    }
    // 5WHY: zero-padding via .slice(-2) for alignment — removed
    // to prevent truncation at >=100 (same bug as DiagnosticScreen).
    // Right-alignment already handles single-digit display correctly.
    Component.onCompleted: refresh()
    function refresh() {
        pass=0; warn=0; fail=0; skip=0; info=0
        var all = appState.allGroupStats // single C++ call (QVariantList), not 5
        for (var g=0; g<all.length; g++) {
            var s = all[g]
            pass += (s.pass||0); warn += (s.warn||0); fail += (s.fail||0); skip += (s.skip||0); info += (s.info||0)
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
                color: ThemeEngine.textSecondary
            }
            Item { width: 8 }
            Label {
                text: ThemeEngine.pad2(count)
                font.family: ThemeEngine.monoFont; font.pixelSize: 16; font.weight: Font.Bold; color: accent
                horizontalAlignment: Text.AlignRight
            }
        }
    }
}
