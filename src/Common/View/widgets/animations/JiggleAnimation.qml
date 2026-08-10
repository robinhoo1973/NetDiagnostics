import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── JiggleAnimation.qml — iOS-style icon jiggle (default busy state) ──
// The DEFAULT animation (~20 diagnostics).  Rotates the icon well itself
// (±2.5°, random per-block phase) so the ICON visibly wiggles like iOS
// home-screen icons in edit mode.  A faint accent ring pulses as a
// secondary affordance.
//
// Usage: JiggleAnimation { anchors.fill: parent; running: testRunning; targetItem: iconWell }

Item {
    id: root
    property bool running: false
    // 5WHY (review B5): the original implementation only pulsed a faint
    // transparent ring (opacity ≤0.22) — the icon itself never moved, so
    // the "iOS jiggle" busy state was nearly invisible.  targetItem (the
    // icon well passed by DiagBlock) is now what actually rotates.
    property var targetItem: null
    property real phaseOffset: Math.random() * 200  // ms, random per block
    property color accentColor: T.ThemeEngine ? T.ThemeEngine.colors.primary : "#60C8F8"

    // ── Real icon jiggle ────────────────────────────────────────────────
    SequentialAnimation {
        running: root.running && root.targetItem !== null
        loops: Animation.Infinite
        // Random phase so multiple blocks don't jiggle in sync
        PauseAnimation { duration: root.phaseOffset }
        NumberAnimation { target: root.targetItem; property: "rotation"; from: 0;    to: 2.5; duration: 90;  easing.type: Easing.InOutQuad }
        NumberAnimation { target: root.targetItem; property: "rotation"; from: 2.5;  to: -2.5; duration: 180; easing.type: Easing.InOutQuad }
        NumberAnimation { target: root.targetItem; property: "rotation"; from: -2.5; to: 2.5;  duration: 180; easing.type: Easing.InOutQuad }
        NumberAnimation { target: root.targetItem; property: "rotation"; from: 2.5;  to: 0;    duration: 90;  easing.type: Easing.InOutQuad }
    }

    onRunningChanged: {
        if (!running) {
            if (root.targetItem)
                root.targetItem.rotation = 0
            // 5WHY: ring.opacity is driven by SequentialAnimation on opacity
            // which destroys the declarative binding.  Mid-cycle stop leaves
            // a ghost ring.  Explicit reset on stop.
            ring.opacity = 0.0
        }
    }

    // ── Subtle accent ring (secondary affordance) ────────────────────────
    Rectangle {
        id: ring
        anchors.centerIn: parent
        width: Math.min(parent.width, parent.height) * 0.92
        height: width
        radius: width / 2
        color: "transparent"
        border { width: 1.5; color: Qt.alpha(root.accentColor, 0.4) }
        opacity: 0.0
        SequentialAnimation on opacity {
            running: root.running
            loops: Animation.Infinite
            NumberAnimation { from: 0; to: 0.5; duration: 300; easing.type: Easing.InOutQuad }
            NumberAnimation { from: 0.5; to: 0; duration: 300; easing.type: Easing.InOutQuad }
        }
    }
}
