import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── PathAnimation.qml — Hop-by-hop node reveal for ~6 diagnostic tests ──
// Traceroute, PathPing, RoutingTable, TCP Connect, Proxy, FTP, Telnet.
// A series of nodes appear sequentially along a path with connecting lines.
//
// Usage: PathAnimation { anchors.fill: parent; running: testRunning; nodes: 5 }

Item {
    id: root
    property bool running: false
    // Unused by Path (draws own content) — see BounceAnimation 5WHY.
    property var targetItem: null
    property int nodes: 5  // number of hop nodes to animate
    property color accentColor: T.ThemeEngine ? T.ThemeEngine.colors.primary : "#60C8F8"
    property int nodeDelay: Tokens.tokens.pathNodeDelay

    // ── Generate node positions along a zigzag path ──────────────────────
    Repeater {
        model: root.nodes
        Rectangle {
            id: node
            property int index: modelData
            width: 6; height: 6; radius: 3
            color: root.accentColor
            opacity: 0
            // Position: spread horizontally, slight vertical zigzag
            x: 8 + (parent.width - 16) * index / Math.max(root.nodes - 1, 1) - 3
            y: parent.height / 2 + (index % 2 === 0 ? -8 : 8) - 3

            SequentialAnimation {
                running: root.running
                loops: Animation.Infinite
                PauseAnimation { duration: index * root.nodeDelay }
                PropertyAction { target: node; property: "opacity"; value: 0.8 }
                PauseAnimation { duration: root.nodeDelay * root.nodes - index * root.nodeDelay + 200 }
                PropertyAction { target: node; property: "opacity"; value: 0.15 }
            }
        }
    }

    // ── Connecting line ──────────────────────────────────────────────────
    Rectangle {
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
        anchors.leftMargin: 11; anchors.rightMargin: 11
        height: 1; color: root.accentColor
        opacity: root.running ? 0.3 : 0.1
    }

    onRunningChanged: { if (!running) { /* nodes auto-reset via animation stop */ } }
}
