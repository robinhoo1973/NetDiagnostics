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
            // 5WHY (复核 2026-08-19 图表全灭): 曾传外层 resultFor 图——ResultChart
            // 读的是嵌套 data 键（templateType/individualRtts/hops/waterfall…），
            // 顶层全为 undefined → _source 恒空、hasChart 恒假，图表区块对
            // 全部 46 项从未渲染（v0.0.3 的 RTT/跳数/瀑布/证书天数全无页面
            // 元素）。注入嵌套 data 子图。
            data: root.detailData.data
            expanded: root._expanded
        }
    }
}
