import QtQuick
import "../theme"
import QtQuick.Controls
import QtQuick.Layouts

// ── SummaryCards — shared data, one row per category ─────────────────
ColumnLayout {
    id: summaryRoot
    spacing: 0
    property int pass: 0; property int warn: 0; property int fail: 0; property int skip: 0; property int info: 0; property int error: 0

    // Header: "Summary" + "Total: N"
    RowLayout {
        AppLabel { Layout.fillWidth: true; text: T.tr("summary"); font.family: ThemeEngine.fontUi; font.pixelSize: 11; font.weight: Font.DemiBold; color: ThemeEngine.colors.textSecondary }
        Label { text: T.tr("totalDiagsLabel") + ": " + (pass+warn+fail+skip+info+error); font.family: ThemeEngine.monoFont; font.pixelSize: 11; color: ThemeEngine.colors.textSecondary }
    }
    Item { Layout.preferredHeight: 6 }

    // 5WHY: showing five zero-count cards is noisy for first-time users.
    // Empty state shows a single hint instead of a wall of "0" values.
    Label {
        Layout.fillWidth: true; Layout.topMargin: 4
        visible: (pass+warn+fail+skip+info+error) === 0
        text: T.tr("runDiag"); font.family: ThemeEngine.monoFont; font.pixelSize: 11
        color: Qt.alpha(ThemeEngine.colors.textSecondary, 0.5)
        horizontalAlignment: Text.AlignHCenter
    }
    // 6 result types — each with colored icon + badge count
    // 5WHY: DiagStatus::Error was missing from the summary — totals no
    // longer matched allGroupStats. Added the 6th card (errorRed).
    SummaryCard { Layout.fillWidth: true; accent: ThemeEngine.colors.passGreen;  iconName: "badge-check";   label: T.tr("summaryPass");    count: summaryRoot.pass;  visible: (pass+warn+fail+skip+info+error) > 0 }
    SummaryCard { Layout.fillWidth: true; accent: ThemeEngine.colors.infoBlue;iconName: "badge-info";    label: T.tr("summaryInfo");    count: summaryRoot.info;  visible: (pass+warn+fail+skip+info+error) > 0 }
    SummaryCard { Layout.fillWidth: true; accent: ThemeEngine.colors.warnYellow; iconName: "badge-warning"; label: T.tr("summaryWarning"); count: summaryRoot.warn;   visible: (pass+warn+fail+skip+info+error) > 0 }
    SummaryCard { Layout.fillWidth: true; accent: ThemeEngine.colors.failRed;   iconName: "badge-close";   label: T.tr("summaryFail");    count: summaryRoot.fail;   visible: (pass+warn+fail+skip+info+error) > 0 }
    SummaryCard { Layout.fillWidth: true; accent: ThemeEngine.colors.skipGray;  iconName: "badge-skip";    label: T.tr("summarySkipped"); count: summaryRoot.skip;   visible: (pass+warn+fail+skip+info+error) > 0 }
    SummaryCard { Layout.fillWidth: true; accent: ThemeEngine.colors.errorRed;  iconName: "badge-error";   label: T.tr("summaryError");   count: summaryRoot.error;  visible: (pass+warn+fail+skip+info+error) > 0 }

    Connections {
        target: appState
        // 5WHY: refresh() was wired to BOTH onProgressChanged and
        // onDiagCompleted — every completed test walked all 5 groups twice.
        // onProgressChanged already fires on every completion.
        function onProgressChanged() { refresh() }
        function onResultsReset() { pass=warn=fail=skip=info=error=0 }
    }
    // 5WHY: zero-padding via .slice(-2) for alignment — removed
    // to prevent truncation at >=100 (same bug as DiagnosticScreen).
    // Right-alignment already handles single-digit display correctly.
    Component.onCompleted: refresh()
    function refresh() {
        pass=0; warn=0; fail=0; skip=0; info=0; error=0
        var all = appState.allGroupStats // single C++ call (QVariantList), not 5
        for (var g=0; g<all.length; g++) {
            var s = all[g]
            pass += (s.pass||0); warn += (s.warn||0); fail += (s.fail||0); skip += (s.skip||0); info += (s.info||0); error += (s.error||0)
        }
    }

    component SummaryCard: Rectangle {
        property color accent: ThemeEngine.colors.passGreen
        property string label: ""
        property string iconName: "badge-info"
        property int count: 0
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
                text: label; font.family: ThemeEngine.monoFont; font.pixelSize: 11; font.weight: Font.Medium
                color: ThemeEngine.colors.textSecondary
            }
            Item { width: 8 }
            Label {
                text: ThemeEngine.pad2(count)
                font.family: ThemeEngine.monoFont; font.pixelSize: 16; font.weight: Font.Bold; color: accent
                // 5WHY: was hard-coded AlignRight; the row mirrors under RTL so
                // the count must hug the "end" edge instead (left in RTL).
                horizontalAlignment: T.textAlignEnd
            }
        }
    }
}
