// PageChartsSection.qml — DetailPage 图表（page-detail.md §2.6；Viz.ResultChart）
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import theme
import widgets
import detail.viz
import core

PageSection {
    id: root
    backgroundStyle: PageSection.Card
    bottomMargin: ThemeEngine.spacing.lg
    contentSpacing: 8

    property var detailData: ({})

    readonly property bool _hasChart: chartView.hasChart
    active: _hasChart && detailData.showCharts !== false
    property bool _expanded: false
    Component.onCompleted: _expanded = chartView.seriesCount <= 8
    onDetailDataChanged: _expanded = chartView.seriesCount <= 8   // UI-9：切换结果重置展开

    ColumnLayout {
        spacing: 0
        CollapsibleSectionHeader {
            Layout.fillWidth: true
            title: T.tr("detailData")
            expanded: root._expanded
            onToggleRequested: root._expanded = !root._expanded
        }
        ResultChart {
            id: chartView
            Layout.fillWidth: true
            data: root.detailData
            expanded: root._expanded
        }
    }
}
