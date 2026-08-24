import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── ConvergeAnimation.qml — 四箭头有序聚拢圆心（Default Gateway）──────
// 四根箭头（杆 + 菱形箭头）从四角沿径向指向圆心，按 1→2→3→4 有序启动
// 依次聚拢，全部到位保持后按 1→2→3→4 顺序回退，循环。
// 纯 Rectangle + Rotation + NumberAnimation —— 无 Canvas/ShapePath/
// ShaderEffect（iOS 静态 Qt 安全）。箭头白色，与 gateway 母版白箭头一致
// （深/浅主题的蓝圆上均清晰）。
//
// Usage: ConvergeAnimation { anchors.fill: parent; running: testRunning }

AnimationBase {
    id: root
    // 5WHY (复核 2026-08-19 视觉决策): 白色箭头盖深色主色垫——onPrimary
    // 暗主题为深墨不适用。经 onAccentPad 令牌（两主题一致白，Palette.js
    // 单一事实源），不再携带裸字面量。
    property color accentColor: T.ThemeEngine.colors.onAccentPad
    property int travel: Tokens.tokens.convergeTravel
    property int stagger: Tokens.tokens.convergeStagger
    property int hold: Tokens.tokens.convergeHold

    readonly property real _cx: parent.width / 2
    readonly property real _cy: parent.height / 2
    // 5WHY (2026-08-24 用户复核 "移动箭头与图上箭头位置偏离"): 母版四根
    // 斜箭头实测占径 4.24..8.85u（外端箭头翼至 9.96）——旧参数 .21/.055
    // 让移动箭头在 1.3..5u 内圈飞行，与静态白箭头完全脱位。改为沿母版
    // 斜杆往复（头端 8.85u ↔ 4.7u，杆即母版斜杆段 4.6u，厚 2.0u 对齐
    // 母版 sw2.3），运动轨迹与静态箭头重合。
    readonly property real _r1: parent.width * 0.369   // 头端远位（=母版外端 8.85u）
    readonly property real _r2: parent.width * 0.196   // 头端近位（≈母版内端 4.7u）
    readonly property real _shaft: Math.max(1.5, parent.width * 2.0 / 24)
    readonly property real _len: parent.width * 4.6 / 24
    readonly property real _head: parent.width * 2.9 / 24

    Component {
        id: arrowComp
        Item {
            id: arrow
            property real angle: 45 + 90 * index
            property real radius: root._r1

            // 杆：径向指向圆心（中心随半径移动）
            Rectangle {
                width: root._len
                height: root._shaft
                radius: root._shaft / 2
                color: root.accentColor
                x: root._cx + Math.cos(arrow.angle * Math.PI / 180)
                   * (arrow.radius + root._len / 2) - width / 2
                y: root._cy + Math.sin(arrow.angle * Math.PI / 180)
                   * (arrow.radius + root._len / 2) - height / 2
                transform: Rotation {
                    origin.x: width / 2
                    origin.y: height / 2
                    angle: arrow.angle
                }
            }
            // 箭头：菱形（一角指向圆心）
            Rectangle {
                width: root._head; height: root._head
                radius: root._head * 0.22
                color: root.accentColor
                x: root._cx + Math.cos(arrow.angle * Math.PI / 180)
                   * arrow.radius - width / 2
                y: root._cy + Math.sin(arrow.angle * Math.PI / 180)
                   * arrow.radius - height / 2
                transform: Rotation {
                    origin.x: width / 2
                    origin.y: height / 2
                    angle: arrow.angle - 45
                }
            }

            // 每箭头独立循环；总时长恒定（2×travel + 3×stagger + hold），
            // 全组同周期 → 有序聚拢后一起保持、依次回退（见计划注释）。
            SequentialAnimation {
                running: root.running
                loops: Animation.Infinite

                // 有序启动：第 i 个箭头延迟 i×stagger
                PauseAnimation { duration: index * root.stagger }
                // 聚拢（外→内）
                NumberAnimation {
                    target: arrow; property: "radius"
                    from: root._r1; to: root._r2
                    duration: root.travel
                    easing.type: Easing.InOutQuad
                }
                // 等待其余箭头聚拢 + 保持
                PauseAnimation { duration: (3 - index) * root.stagger + root.hold }
                // 回退（内→外）
                NumberAnimation {
                    target: arrow; property: "radius"
                    from: root._r2; to: root._r1
                    duration: root.travel
                    easing.type: Easing.InOutQuad
                }
            }
        }
    }

    Repeater {
        id: repeater
        model: 4
        delegate: arrowComp
    }

    function resetVisuals() {
        for (var i = 0; i < 4; ++i) {
            var d = repeater.itemAt(i)
            if (d)
                d.radius = root._r1
        }
    }
}
