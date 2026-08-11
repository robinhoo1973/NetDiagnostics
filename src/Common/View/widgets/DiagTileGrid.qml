// =============================================================================
// DiagTileGrid.qml — shared responsive diagnostic-tile wall (v4)
//
// Two-phase gap-ratio grid algorithm with comfort-aware column selection:
//
//   Phase 1 ("comfort-first density"):
//     Walk down from max-feasible n until tile ≥ comfortMin, where
//       comfortMin = minTile + max(8px, 12% × (maxTile - minTile))
//     This prevents "临界方块" — tiles that are mathematically valid but
//     visually cramped (e.g. 80px on a 430px phone barely meets the 44pt
//     touch target; an 82px tile on a 31-column dashboard is illegible).
//
//   Phase 2 ("fallback"):
//     If no n satisfies the comfort constraint, fall back to max-feasible n
//     (same as v3 behaviour).  This guarantees the algorithm never breaks.
//
//   Max-column cap: prevents ultra-wide screen degeneration
//     (compact: 15 cols, full: 12 cols — ~5-9 items/row optimal for scan).
//
// Core formula (unchanged from v3):
//   block_width = n × tile_width + (n+1) × gap_width
//   gap_width = k × tile_width        (k = 0.08 full, 0.06 compact)
//   → tile_width = block_width / (n + (n+1) × k)
//
// Edge gaps (n+1, not n-1) follow Apple HIG and M3: tiles are centered
// in the row with equal breathing space on both ends.
//
// Usage: DiagTileGrid { Layout.fillWidth: true; model: ...; compact: false }
// =============================================================================
import QtQuick

Item {
    id: root

    // ── Public API ────────────────────────────────────────────────────────
    property var model: []
    property bool compact: false
    property bool usePerItemRunning: false
    signal tileClicked(var data)

    // ── Design parameters ──────────────────────────────────────────────────
    // gap-to-tile ratio k (M3: ~8%, Apple HIG: ~10%, compact: slightly tighter)
    readonly property real _k: compact ? 0.06 : 0.08
    readonly property int _minTile: compact ? 80 : 100
    readonly property int _maxTile: 160

    // ── Comfort threshold ──────────────────────────────────────────────────
    // Tiles at exactly minTile feel cramped — enforce a soft margin so the
    // grid breathes.  Margin = max(8px absolute, 12% of [min,max] range).
    //   Full:    comfortMin = 100 + max(8, 60×0.12) = 108
    //   Compact: comfortMin =  80 + max(8, 80×0.12) =  90
    readonly property int _comfortMin:
        _minTile + Math.max(8, Math.round((_maxTile - _minTile) * 0.12))

    // Max columns — safety net for ultra-wide screens (4K+).
    // Set high enough that it only triggers on genuinely degenerate widths;
    // the comfort walk (Phase 1) is the primary mechanism for normal screens.
    //   Compact: natural max is ~22 at 1920px → cap at 20 (kicks in at ≈2100px)
    //   Full:    natural max is ~17 at 1920px → cap at 18 (kicks in at ≈2050px)
    readonly property int _maxColumns: compact ? 20 : 18

    // ── Column count n (two-phase) ────────────────────────────────────────
    // Phase 1: from max-feasible n, walk down to the first n whose exact
    //          tile meets the comfort threshold.
    // Phase 2: if no n passes, fall back to max-feasible n (never break).
    readonly property int _columns: {
        var w = width
        if (w <= 0) return 1

        // Max feasible n (density ceiling, same as v3)
        var maxN = Math.floor((w / _minTile - _k) / (1.0 + _k))
        maxN = Math.max(1, Math.min(_maxColumns, maxN))

        // Walk down to find a comfortable n
        for (var n = maxN; n >= 1; n--) {
            var tileExact = w / (n + (n + 1) * _k)
            if (tileExact >= _comfortMin && tileExact <= _maxTile) {
                return n
            }
        }

        // Fallback: comfort not achievable, use max density
        return maxN
    }

    // ── Tile size ──────────────────────────────────────────────────────────
    // tile = block / (n + (n+1)×k), clamped to [min, max]
    // 5WHY (width=0 during reloadModel): when the Repeater rebuilds
    // delegates, Grid briefly has zero width → tile=0 clamped to _minTile.
    // On the next frame width restores but _tileSize already evaluated
    // with stale _columns=1 → wrong layout.  Guard: keep last valid size.
    property int _lastTileSize: Math.round((_minTile + _maxTile) / 2)
    readonly property int _tileSize: {
        var n = _columns
        if (width <= 0 || n <= 0) return _lastTileSize
        var denom = n + (n + 1) * _k
        // Math.round centres the rounding error (±0.5px) instead of
        // Math.floor's systematic negative bias (tiles always undersized).
        var tile = Math.round(width / denom)
        var result = Math.min(_maxTile, Math.max(_minTile, tile))
        // Only cache plausible sizes to guard against transient mid-animation
        // widths corrupting the fallback value.
        if (result >= _minTile && result <= _maxTile) {
            root._lastTileSize = result
        }
        return result
    }

    // ── Gap width ──────────────────────────────────────────────────────────
    // Minimum gap raised from 4→5: below 5px the grid reads as one
    // undifferentiated block; 5px+ restores visual grouping per Gestalt
    // proximity principle.
    readonly property int _gapWidth: Math.max(5, Math.round(_tileSize * _k))

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
