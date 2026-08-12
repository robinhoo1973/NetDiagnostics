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
    // 5WHY (tile width drift during run): both _columns and _tileSize were
    // readonly bindings; _tileSize returned a MIDPOINT fallback (_lastTileSize)
    // whenever width was <=0 (a transient during Repeater rebuild / panel
    // recreation at run start).  If the grid was sized in that width=0 window
    // and the recompute lagged, tiles rendered at the WRONG width (midpoint
    // 130) for the whole run, then snapped to the computed size (e.g. 101)
    // after the run — running vs finished states disagreed.  Now both are
    // recomputed EXPLICITLY from width on every width change, so the tile
    // size always reflects the current grid width in BOTH states.
    property int _columns: 1

    // ── Tile size ──────────────────────────────────────────────────────────
    // tile = block / (n + (n+1)×k), clamped to [min, max]
    property int _tileSize: _minTile
    onWidthChanged: _recomputeSize()
    Component.onCompleted: _recomputeSize()
    function _recomputeSize() {
        var w = width
        if (w <= 0) return   // width unknown yet — keep _minTile floor
        _columns = Math.max(1, Math.floor((w / _minTile - _k) / (1.0 + _k)))
        var denom = _columns + (_columns + 1) * _k
        _tileSize = Math.min(_maxTile, Math.max(_minTile, Math.floor(w / denom)))
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
