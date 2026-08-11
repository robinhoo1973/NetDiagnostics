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
    // 5WHY (width=0 during reloadModel): when the Repeater rebuilds
    // delegates, Grid briefly has zero width → tile=0 clamped to _minTile.
    // On the next frame width restores but _tileSize already evaluated
    // with stale _columns=1 → wrong layout.  Guard: keep last valid size.
    property int _lastTileSize: Math.max(_minTile, Math.min(_maxTile, 108))
    readonly property int _tileSize: {
        var n = _columns
        if (width <= 0 || n <= 0) return _lastTileSize
        var denom = n + (n + 1) * _k
        var tile = Math.floor(width / denom)
        var result = Math.min(_maxTile, Math.max(_minTile, tile))
        root._lastTileSize = result
        return result
    }

    // ── Gap width ──────────────────────────────────────────────────────────
    readonly property int _gapWidth: Math.max(4, Math.round(_tileSize * _k))

    // ── Layout metrics ─────────────────────────────────────────────────────
    // 5WHY (fragile calc): the old manual height formula decoupled from the
    // Grid's actual layout — if DiagBlock ever renders taller than _tileSize,
    // implicitHeight would under-report and the parent ColumnLayout would clip
    // the bottom.  Bind to the Grid's own layout output instead.
    implicitHeight: grid.implicitHeight

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
