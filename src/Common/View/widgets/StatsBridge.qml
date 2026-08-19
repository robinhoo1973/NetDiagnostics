// =============================================================================
// StatsBridge.qml — groupStats(-1) 聚合统计订阅桥
//
// 5WHY (复核 2026-08-19 单一订阅点): 状态头/空态/摘要卡三消费方各自维护
// W.normalize(groupStats(-1)) + Connections{progress/runStatus/filteredData}
// + onCompleted 刷新的同一套接线——语义信号轮次曾两轮逐文件补接（漏接=
// 同屏数字矛盾，5WHY 记录两次）。桥收敛"订阅 + 归一化 + 离屏门控 + 揭示
// 自愈"为单一组件：消费方读 _s / 接 refreshed，新语义信号只改一处。
// =============================================================================
import QtQuick
import NetDiagnostics.App 1.0
import "StatsUtil.js" as W

Item {
    id: root

    // 归一化聚合（null 起步安全；每次刷新替换身份驱动消费方绑定重估）
    property var _s: W.normalize(null)
    // 屏幕注入的可见性（离屏整体禁用订阅；重新可见补刷新）
    property bool screenVisible: true
    signal refreshed()

    function _refresh() {
        _s = W.normalize(AppState.groupStats(-1))
        refreshed()
    }

    onScreenVisibleChanged: if (screenVisible) _refresh()

    Connections {
        target: AppState
        enabled: root.screenVisible
        function onProgressChanged() { root._refresh() }
        function onRunStatusChanged() { root._refresh() }
        // 5WHY (复核 2026-08-18 语义信号): 换 scheme 不重跑只发
        // filteredDataChanged（旧接 targetChanged+stateVersionChanged 双发
        // 双刷、误触发）；语义信号单次驱动。
        function onFilteredDataChanged() { root._refresh() }
    }
    Component.onCompleted: _refresh()
}
