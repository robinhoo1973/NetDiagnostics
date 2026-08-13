// =============================================================================
// DashboardSummaryComp.qml — 摘要统计层（page-dashboard.md §2.5）
//
// 注入 PageStatusHeaderSection.headerExtra（Dashboard → Common 依赖方向）。
// 层计时/总时长：onProgressChanged/onRunStatusChanged 命令式刷新（UI-2）。
// =============================================================================
import NetDiagnostics.App 1.0
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme

Item {
    id: root
    implicitHeight: 56

    property var _agg: ({ total: 0, completed: 0, pass: 0, fail: 0 })
    property string _timeText: "—"

    function _refresh() {   // UI-2：命令式赋值
        var s = AppState.groupStats(-1)
        _agg = { total: s.total || 0, completed: s.completed || 0,
                 pass: s.pass || 0, fail: s.fail || 0 }
        _timeText = ThemeEngine.formatDuration(s.durationMs || 0)
    }
    Connections {
        target: AppState
        function onProgressChanged() { root._refresh() }
        function onRunStatusChanged() { root._refresh() }
    }
    Component.onCompleted: _refresh()

    RowLayout {
        anchors.fill: parent
        spacing: ThemeEngine.spacing.sm

        SummaryCell { big: String(root._agg.total); caption: T.tr("totalDiags") }
        SummaryCell { big: String(root._agg.completed); caption: T.tr("completed") }
        SummaryCell { big: String(root._agg.pass); caption: T.tr("summaryPass") }
        SummaryCell { big: String(root._agg.fail); caption: T.tr("summaryFail") }
        SummaryCell { big: root._timeText; caption: T.tr("totalTime") }
    }

    component SummaryCell: ColumnLayout {
        required property string big
        required property string caption
        Layout.fillWidth: true
        spacing: 2
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: big
            font.family: ThemeEngine.monoFont
            font.pixelSize: ThemeEngine.fontSize.title
            font.weight: Font.Bold
            color: ThemeEngine.colors.primary
            elide: Text.ElideRight
            maximumLineCount: 1
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: caption
            font.family: ThemeEngine.fontUi
            font.pixelSize: ThemeEngine.fontSize.caption
            color: ThemeEngine.colors.textSecondary
            elide: Text.ElideRight
            maximumLineCount: 1
        }
    }
}
