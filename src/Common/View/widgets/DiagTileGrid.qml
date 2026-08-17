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
        // 两侧 gap——勿再减 2×gap，否则每屏系统性欠填）。gap 四舍五入可能
        // 使总量略超 w：按实际总量下调列数直至不越界（cols 单调递减收敛）。
        var cols = Math.max(1, Math.floor((w / _minTile - _k) / (1.0 + _k)))
        var tile = _minTile
        for (var i = 0; i < 3; ++i) {
            var denom = cols + (cols + 1) * _k
            tile = Math.min(_maxTile, Math.max(_minTile, Math.floor(w / denom)))
            var gap = Math.max(4, Math.round(tile * _k))
            if (cols * tile + (cols + 1) * gap <= w) break
            cols = Math.max(1, cols - 1)
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
                testRunning: root.usePerItemRunning
                             ? (root.groupRunning && modelData.isPending === true && !modelData.isDisabled)
                             : false
                onClicked: function(data) { root.tileClicked(data) }
            }
        }
    }
}
