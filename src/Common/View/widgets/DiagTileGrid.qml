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
        _columns = Math.max(1, Math.floor((w / _minTile - _k) / (1.0 + _k)))
        var denom = _columns + (_columns + 1) * _k
        _tileSize = Math.min(_maxTile, Math.max(_minTile, Math.floor(w / denom)))
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
