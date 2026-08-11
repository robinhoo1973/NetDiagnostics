// =============================================================================
// DiagTileGrid.qml — shared responsive diagnostic-tile wall
//
// Single source of truth for the tile grid used by the Diagnostic screen
// (via DiagGroupPanel) and the Dashboard per-group cards.
//
// 5WHY (v2, 2026-08-11): the original algorithm used Flow + FIXED blockSize
// (108px) + dynamic gap (8-24px).  On a 1000px-wide container, 7 tiles at
// 108px + 6 gaps at max 24px = 900px used, 100px wasted — the gap cap
// prevented filling the available space.  Switched to Grid + computed
// columns + responsive tile size that fills the container width exactly.
//
//   columns = Math.floor((width + minGap) / (minTile + minGap))
//   tileSize = (width - (columns - 1) * spacing) / columns
//
// This is the iOS Home Screen / App Store grid pattern: tiles scale to fill
// the row exactly.  Clamped to [80, 160] so tiles never become unusably
// small or cartoonishly large.
//
// Usage:
//   DiagTileGrid {
//       Layout.fillWidth: true
//       model: someModel
//       compact: false
//       usePerItemRunning: true
//       onTileClicked: function(data) { ... }
//   }
// =============================================================================
import QtQuick

Item {
    id: root

    // ── Public API ────────────────────────────────────────────────────────
    property var model: []
    property bool compact: false
    // true → DiagBlock gets testRunning from modelData.isRunning (live group);
    // false → static tiles (Dashboard snapshot view).
    property bool usePerItemRunning: false
    signal tileClicked(var data)

    // ── Responsive sizing ──────────────────────────────────────────────────
    // Minimum tile size (compact 80px, full 100px) & gap
    readonly property int _minTile: compact ? 80 : 100
    readonly property int _gap: 8

    // Columns: how many tiles fit per row at minimum size
    readonly property int _columns: Math.max(1,
        Math.floor((width + _gap) / (_minTile + _gap)))

    // Tile size: fill the row exactly (spacing eaten by columnSpacing)
    readonly property int _tileSize: {
        if (_columns <= 1) return Math.min(140, Math.max(_minTile, width))
        var avail = width - (_columns - 1) * _gap
        var size = Math.floor(avail / _columns)
        return Math.min(160, Math.max(_minTile, size))
    }

    // Height follows the grid
    readonly property int _rowCount: model && model.length
        ? Math.ceil(model.length / _columns) : 0
    implicitHeight: _rowCount > 0
        ? _rowCount * _tileSize + (_rowCount - 1) * _gap : 0

    // ── Tile wall ─────────────────────────────────────────────────────────
    Grid {
        id: grid
        anchors { left: parent.left; right: parent.right; top: parent.top }
        columns: root._columns
        columnSpacing: root._gap
        rowSpacing: root._gap

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
