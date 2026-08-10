// =============================================================================
// DiagTileGrid.qml — shared responsive diagnostic-tile wall
//
// Single source of truth for the tile grid used by the Diagnostic screen
// (via DiagGroupPanel) and the Dashboard per-group cards.  Encapsulates:
//   • Flow-based auto-fit columns with the DYNAMIC gap algorithm
//   • DiagBlock delegate wiring (blockSize / compact / per-item running)
//   • click → tileClicked(data)
//
// 5WHY: DiagGroupPanel and DashboardScreen each owned a near-identical
// Flow + DiagBlock block; the Dashboard kept a FIXED spacing: 8 that left a
// ragged right edge (the exact problem DiagGroupPanel's 5WHY documents),
// while the Diagnostic screen had the dynamic algorithm — the two drifted.
// One component = one algorithm = no drift and no wasted space.
//
// Usage:
//   DiagTileGrid {
//       Layout.fillWidth: true
//       model: someModel
//       blockSize: 108
//       compact: false
//       usePerItemRunning: true
//       onTileClicked: function(data) { ... }
//   }
// =============================================================================
import QtQuick

Item {
    id: root

    // ── Public API ────────────────────────────────────────────────────────
    property var model: []            // list of {diagId,status,isRunning,isDisabled,...}
    property int blockSize: 108
    property bool compact: false
    // true → DiagBlock gets testRunning from modelData.isRunning (live group);
    // false → static tiles (Dashboard snapshot view).
    property bool usePerItemRunning: false
    signal tileClicked(var data)

    // Height follows the wrapped tile wall (Flow's own content height).
    implicitHeight: flow.implicitHeight

    // ── Tile wall ─────────────────────────────────────────────────────────
    Flow {
        id: flow
        anchors { left: parent.left; right: parent.right; top: parent.top }

        // Dynamic spacing: leftover width distributed evenly BETWEEN tiles
        // (not at edges).  Minimum 8px, maximum 24px.
        property real _cols: Math.max(1, Math.floor((width + 8) / (root.blockSize + 8)))
        property real _gap: {
            if (_cols <= 1) return 8
            var leftover = width - _cols * root.blockSize
            return Math.max(8, Math.min(24, Math.floor(leftover / (_cols - 1))))
        }
        spacing: _gap

        Repeater {
            model: root.model
            delegate: DiagBlock {
                blockSize: root.blockSize
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
