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
    // 5WHY (复核 2026-08-20 身份稳定): 空图回落曾内联 ({}) ——每次绑定
    // 重估新建对象身份、ResultChart.data 下游绑定随之重跑。稳定哨兵。
    readonly property var _emptyData: ({})

    readonly property bool _hasChart: chartView.hasChart
    active: _hasChart && detailData.showCharts !== false
    property bool _expanded: false
    Component.onCompleted: _expanded = chartView.seriesCount <= 8
    // 5WHY (复核 2026-08-19 陈旧读取): 曾直接赋值——属性变更处理器先于依赖
    // 绑定重估执行（PageHeroSection 同款已证），chartView.data 仍是上一项
    // 的图：seriesCount 返回旧计数，大图/小图切换时展开态判定颠倒。延迟
    // 一帧：绑定已随 detailData 更新，判定基于新图。
    onDetailDataChanged: Qt.callLater(function() {
        if (!root) return
        root._expanded = chartView.seriesCount <= 8   // UI-9：切换结果重置展开
    })

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
            // 5WHY (复核 2026-08-19 启动 TypeError): 详情浮层预建、DetailPage
            // push 前 detailData 均为 {} ——data 绑定求值为 undefined，
            // ResultChart._source 读 root.data.templateType 抛 TypeError
            // （iOS 静态 Qt 视 QML 错误为启动崩溃链）。缺 data 键时回落空图。
            data: root.detailData.data !== undefined ? root.detailData.data : root._emptyData
            expanded: root._expanded
        }
    }
}
