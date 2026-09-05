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

    // 5WHY (simplify 2026-09-05 语义漂移修正): statsEqual 门把 refreshed()
    // 的语义从"刷新发生"改成了"统计变化"——终端事件（运行完成，统计可能
    // 无 delta）被门吞掉，消费方的墙钟派生（如 "Total time"）冻结，只得
    // 自己另接 runStatusChanged（接线回到桥外，桥的"新语义信号只改一处"
    // 契约被侵蚀）。修正：生命周期转场（runStatusChanged）无条件广播，
    // stats-only tick（progress/filteredData）走门控。
    // 5WHY (simplify 2026-09-05 每 tick 双扫): 版本门早退——statsVersion 未变
    // 即跳过 groupStats C++ 全量重建（44 项双遍 + normalize 分配）。
    property int _lastStatsVersion: -1
    function _refresh(force) {
        // 单次跨界读取（simplify 二轮：曾比较+赋值双读 C++ int）
        var v = AppState.statsVersion
        if (!force && v === _lastStatsVersion) return
        _lastStatsVersion = v
        var s = W.normalize(AppState.groupStats(-1))
        if (!force && W.statsEqual(_s, s)) return
        _s = s
        refreshed()
    }

    onScreenVisibleChanged: if (screenVisible) _refresh(false)

    Connections {
        target: AppState
        enabled: root.screenVisible
        function onProgressChanged() { root._refresh(false) }
        function onRunStatusChanged() { root._refresh(true) }
        // 5WHY (复核 2026-08-18 语义信号): 换 scheme 不重跑只发
        // filteredDataChanged（旧接 targetChanged+stateVersionChanged 双发
        // 双刷、误触发）；语义信号单次驱动。
        function onFilteredDataChanged() { root._refresh(false) }
    }
    Component.onCompleted: _refresh(false)
}
