// =============================================================================
// DiagTileGrid.qml — 响应式瓦片网格（gap-ratio 算法，归档原样移植）
//   block_width = n × tile + (n+1) × gap；gap = k × tile（k=0.08/0.06）
// =============================================================================
import QtQuick

Item {
    id: root

    property var model: []
    property bool compact: false
    property bool usePerItemRunning: false
    property bool groupRunning: false
    // 5WHY (复核 2026-08-19): 屏幕可见性下传（面板→网格壳→网格→瓦片），
    // 隐藏页瓦片停止运行动画。
    property bool screenVisible: true
    // 5WHY (复核 2026-08-19 viewport 门控): Flickable 下传（同链）。
    property var viewportItem: null
    signal tileClicked(var data)

    readonly property real _k: compact ? 0.06 : 0.08
    readonly property int _minTile: compact ? 80 : 100
    readonly property int _maxTile: 160

    property int _columns: 1
    property int _tileSize: _minTile
    onWidthChanged: _recomputeSize()
    onCompactChanged: _recomputeSize()   // compact 切换会改变 _k/_minTile
    Component.onCompleted: _recomputeSize()
    function _recomputeSize() {
        var w = width
        if (w <= 0) return
        // 5WHY (review round 4): 精确方程 n×tile+(n+1)×gap=w（denom 已含
        // 两侧 gap——勿再减 2×gap，否则每屏系统性欠填）。
        // 5WHY (verify 2026-08-17): gap 四舍五入溢出量至多 (cols+1)×0.5px——
        // 旧版直接降列：w=325 时 2×144+3×12=324 可放下，却塌成 1 列 160px
        // 瓦片 + 172px 死区（60-4000px 间 526 个宽度带少显示 1+ 列）。改
        // 先缩瓦片 1px 吸收溢出（总减量 ≥cols 且 gap 至多反增 cols+1，仍
        // 净减、必然收敛）；仅当瓦片已贴 _minTile 仍越界才降列重算。
        var cols = Math.max(1, Math.floor((w / _minTile - _k) / (1.0 + _k)))
        var denom = cols + (cols + 1) * _k
        var tile = Math.min(_maxTile, Math.max(_minTile, Math.floor(w / denom)))
        for (var i = 0; i < 4; ++i) {
            var gap = Math.max(4, Math.round(tile * _k))
            if (cols * tile + (cols + 1) * gap <= w) break
            if (tile > _minTile) { tile--; continue }
            cols = Math.max(1, cols - 1)
            denom = cols + (cols + 1) * _k
            tile = Math.min(_maxTile, Math.max(_minTile, Math.floor(w / denom)))
        }
        // 防御（共享组件可被 < ~90px 的嵌入方调用；主窗 minWidth 360 不可达）：
        // cols=1 且瓦片已贴 _minTile 仍越界时收缩瓦片直至放下。
        while (tile > 8
               && cols * tile + (cols + 1) * Math.max(4, Math.round(tile * _k)) > w) {
            tile--
        }
        _columns = cols
        _tileSize = tile
    }

    readonly property int _gapWidth: Math.max(4, Math.round(_tileSize * _k))
    implicitHeight: grid.implicitHeight

    Grid {
        id: grid
        anchors {
            left: parent.left; right: parent.right; top: parent.top
            leftMargin: root._gapWidth; rightMargin: root._gapWidth
        }
        columns: root._columns
        columnSpacing: root._gapWidth
        rowSpacing: root._gapWidth

        Repeater {
            model: root.model
            delegate: DiagBlock {
                blockSize: root._tileSize
                itemData: modelData
                // 5WHY (复核 2026-08-18 动画批量抖动): 完成瞬间全部瓦片同帧
                // 触发 settle+光晕动画——按网格序号错峰 30ms/瓦片（封顶
                // 300ms），并行 Suite 结果burst 时动画压力摊平。
                staggerIndex: index
                screenVisible: root.screenVisible
                viewportItem: root.viewportItem
                testRunning: root.usePerItemRunning
                             ? (root.groupRunning && modelData.isPending === true && !modelData.isDisabled)
                             : false
                onClicked: function(data) { root.tileClicked(data) }
            }
        }
    }
}
