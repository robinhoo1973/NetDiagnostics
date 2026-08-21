import QtQuick

// ── JiggleAnimation.qml — iOS-style icon jiggle (default busy state) ──
// The DEFAULT animation (~20 diagnostics).  Rotates the icon well itself
// (±2.5°, random per-block phase) so the ICON visibly wiggles like iOS
// home-screen icons in edit mode.  A faint accent ring pulses as a
// secondary affordance.
//
// 5WHY (复核 2026-08-21 相位契约统一): 曾两段声明式 running 绑定——Qt 的
// stop() 保留 currentTime，重跑从中途续播（与 GeoLocate/WifiWave 同款
// 相位错乱，彼处已改 RestartController 此处漏改）。改用共享命令式契约：
// restart 从 0 相位开始；停止即经 onStopped 复位旋转与光环。
//
// Usage: JiggleAnimation { anchors.fill: parent; running: testRunning; targetItem: iconWell }

AnimationBase {
    id: root
    // 5WHY (review B5): the original implementation only pulsed a faint
    // transparent ring (opacity ≤0.22) — the icon itself never moved, so
    // the "iOS jiggle" busy state was nearly invisible.  targetItem（基类
    // 统一声明——DiagBlock 传入的图标井）now actually rotates.
    property real phaseOffset: Math.random() * 200  // ms, random per block

    // ── Real icon jiggle ────────────────────────────────────────────────
    SequentialAnimation {
        id: jiggleSeq
        loops: Animation.Infinite
        // Random phase so multiple blocks don't jiggle in sync
        PauseAnimation { duration: root.phaseOffset }
        NumberAnimation { target: root.targetItem; property: "rotation"; from: 0;    to: 2.5; duration: 90;  easing.type: Easing.InOutQuad }
        NumberAnimation { target: root.targetItem; property: "rotation"; from: 2.5;  to: -2.5; duration: 180; easing.type: Easing.InOutQuad }
        NumberAnimation { target: root.targetItem; property: "rotation"; from: -2.5; to: 2.5;  duration: 180; easing.type: Easing.InOutQuad }
        NumberAnimation { target: root.targetItem; property: "rotation"; from: 2.5;  to: 0;    duration: 90;  easing.type: Easing.InOutQuad }
    }
    // 5WHY (复核 2026-08-21 复位多路径): 曾 onStopped 再复位旋转——基类
    // onRunningChanged(!running) 的 resetVisuals 已含旋转+光环复位
    // （targetItem 置空过渡时 onStopped 复位目标已空，恒 no-op），
    // 双路径漂移风险。停止复位单走基类钩子。
    RestartController {
        running: root.running && root.targetItem !== null
        target: jiggleSeq
    }

    function resetVisuals() {
        if (root.targetItem)
            root.targetItem.rotation = 0
        // 5WHY: ring.opacity is driven by target-based SequentialAnimation
        // which destroys the declarative binding.  Mid-cycle stop leaves
        // a ghost ring.  Explicit reset on stop.
        ring.opacity = 0.0
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
        SequentialAnimation {
            id: ringSeq
            loops: Animation.Infinite
            NumberAnimation { target: ring; property: "opacity"; from: 0; to: 0.5; duration: 300; easing.type: Easing.InOutQuad }
            NumberAnimation { target: ring; property: "opacity"; from: 0.5; to: 0; duration: 300; easing.type: Easing.InOutQuad }
        }
        RestartController {
            running: root.running
            target: ringSeq
        }
    }
}
