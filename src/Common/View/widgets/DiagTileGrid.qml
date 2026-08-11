// =============================================================================
// DiagTileGrid.qml — shared responsive diagnostic-tile wall (v3)
//
// Industry-standard gap-ratio grid algorithm:
//   block_width = n × tile_width + (n+1) × gap_width
//   gap_width = k × tile_width        (k = 0.08 full, 0.06 compact)
//   → tile_width = block_width / (n + (n+1) × k)
//
// Edge gaps (n+1, not n-1) follow Apple HIG and M3: tiles are centered
// in the row with equal breathing space on both ends.  This is the iOS
// Home Screen / App Store / Files grid pattern.
//
// Tile size clamped to [minTile, 160] for usability across all screen sizes.
//
// Usage: DiagTileGrid { Layout.fillWidth: true; model: ...; compact: false }
// =============================================================================
import QtQuick

Item {
    id: root

    // ── Public API ────────────────────────────────────────────────────────
    // model items: {diagId,status,isRunning,isDisabled,label,...} — passed to DiagBlock
    property var model: []
    property bool compact: false
    property bool usePerItemRunning: false
    signal tileClicked(var data)

    // ── Design parameters ──────────────────────────────────────────────────
    // gap-to-tile ratio k (M3: ~8%, Apple HIG: ~10%, compact: slightly tighter)
    readonly property real _k: compact ? 0.06 : 0.08
    readonly property int _minTile: compact ? 80 : 100
    readonly property int _maxTile: 160

    // ── Column count n ─────────────────────────────────────────────────────
    // Find maximum n where tile_width >= minTile.
    // From: block / (n + (n+1)×k) >= minTile
    //   →  n <= (block/minTile - k) / (1 + k)
    readonly property int _columns: {
        var w = width
        if (w <= 0) return 1
        var n = Math.floor((w / _minTile - _k) / (1.0 + _k))
        return Math.max(1, n)
    }

    // ── Tile size ──────────────────────────────────────────────────────────
    // tile = block / (n + (n+1)×k), clamped to [min, max]
    readonly property int _tileSize: {
        if (_columns <= 1) return Math.min(_maxTile, Math.max(_minTile, width))
        var denom = _columns + (_columns + 1) * _k
        var tile = Math.floor(width / denom)
        return Math.min(_maxTile, Math.max(_minTile, tile))
    }

    // ── Gap width ──────────────────────────────────────────────────────────
    readonly property int _gapWidth: Math.max(4, Math.round(_tileSize * _k))

    // ── Layout metrics ─────────────────────────────────────────────────────
    readonly property int _rowCount: model && model.length
        ? Math.ceil(model.length / _columns) : 0
    implicitHeight: _rowCount > 0
        ? _rowCount * _tileSize + (_rowCount - 1) * _gapWidth : 0

    // ── Tile wall ─────────────────────────────────────────────────────────
    // Edge gaps via left/right anchors.margins = _gapWidth
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
                             ? (modelData.isRunning === true && !modelData.isDisabled)
                             : false
                onClicked: function(data) { root.tileClicked(data) }
            }
        }
    }
}
