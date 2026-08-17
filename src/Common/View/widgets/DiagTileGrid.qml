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
        // 5WHY (review round 4): 列数按 w 计算但网格两侧各留 1×gap——边界
        // 宽度下多算一列导致末列越出网格。以可用宽（w-2×gap）迭代 2 次收敛。
        var cols = Math.max(1, Math.floor((w / _minTile - _k) / (1.0 + _k)))
        var tile = _minTile
        for (var i = 0; i < 2; ++i) {
            var gap = Math.max(4, Math.round(tile * _k))
            var avail = w - 2 * gap
            cols = Math.max(1, Math.floor((avail / _minTile - _k) / (1.0 + _k)))
            var denom = cols + (cols + 1) * _k
            tile = Math.min(_maxTile, Math.max(_minTile, Math.floor(avail / denom)))
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
                compact: root.compact
                itemData: modelData
                testRunning: root.usePerItemRunning
                             ? (root.groupRunning && modelData.isPending === true && !modelData.isDisabled)
                             : false
                onClicked: function(data) { root.tileClicked(data) }
            }
        }
    }
}
