import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── CheckAnimation.qml — 盾牌内打勾显现（DNS Integrity）────────────────
// 用两根纯 Rectangle 线段复刻 shield-check 母版的勾（24 viewBox：
// (9,12)→(11,14)→(15,10)），沿各自轴向 0→全长 逐笔显现，保持后淡出——
// 呈现"在盾牌里打勾"的动画。
// 纯 Rectangle + Rotation + NumberAnimation —— 无 Canvas/ShapePath/
// ShaderEffect（iOS 静态 Qt 安全，见 Gauge.qml 5WHY）。
//
// Usage: CheckAnimation { anchors.fill: parent; running: testRunning }

Item {
    id: root
    property bool running: false
    // Unused by Check (draws own content) — see BounceAnimation 5WHY.
    property var targetItem: null
    property color accentColor: T.ThemeEngine ? T.ThemeEngine.colors.success : "#4ADE80"
    property int drawDuration: Tokens.tokens.checkDrawDuration
    property int holdDuration: Tokens.tokens.checkHoldDuration

    // 勾线段几何（24 viewBox → 父级像素；与盾牌母版勾位一致）
    readonly property real _s: parent.width / 24
    readonly property real _stroke: Math.max(1.5, 2.2 * _s)
    readonly property real _len1: Math.sqrt(8) * _s      // (9,12)→(11,14) ≈2.83u
    readonly property real _len2: Math.sqrt(32) * _s     // (11,14)→(15,10) ≈5.66u
    readonly property real _p1x: 9 * _s
    readonly property real _p1y: 12 * _s
    readonly property real _p2x: 11 * _s
    readonly property real _p2y: 14 * _s

    Item {
        id: checkGroup
        anchors.fill: parent
        opacity: 0

        // 第一笔：自 (9,12) 沿 45° 向下右展开
        Rectangle {
            id: seg1
            width: 0; height: root._stroke
            radius: root._stroke / 2
            color: root.accentColor
            x: root._p1x
            y: root._p1y - height / 2
            transform: Rotation {
                origin.x: 0
                origin.y: root._stroke / 2
                angle: 45
            }
        }
        // 第二笔：自 (11,14) 沿 -45° 向上右展开
        Rectangle {
            id: seg2
            width: 0; height: root._stroke
            radius: root._stroke / 2
            color: root.accentColor
            x: root._p2x
            y: root._p2y - height / 2
            transform: Rotation {
                origin.x: 0
                origin.y: root._stroke / 2
                angle: -45
            }
        }
    }

    // 5WHY (DiagAnimator onLoaded race): 动画组件在 onCompleted 里读 running
    // 会看到 false —— 这里用 running 驱动的 SequentialAnimation 反应式启动。
    SequentialAnimation {
        running: root.running
        loops: Animation.Infinite

        // 重置：淡入 + 两笔归零
        PropertyAction { target: checkGroup; property: "opacity"; value: 1 }
        PropertyAction { target: seg1; property: "width"; value: 0 }
        PropertyAction { target: seg2; property: "width"; value: 0 }
        // 第一笔显现（短斜笔）
        NumberAnimation {
            target: seg1; property: "width"
            from: 0; to: root._len1
            duration: root.drawDuration * 0.45
            easing.type: Easing.InOutQuad
        }
        // 第二笔显现（长提笔，衔接）
        NumberAnimation {
            target: seg2; property: "width"
            from: 0; to: root._len2
            duration: root.drawDuration * 0.55
            easing.type: Easing.InOutQuad
        }
        // 保持（勾完整显现）
        PauseAnimation { duration: root.holdDuration }
        // 淡出，露出母版静态勾
        NumberAnimation {
            target: checkGroup; property: "opacity"
            from: 1; to: 0; duration: 260
        }
    }

    onRunningChanged: {
        if (!running) {
            checkGroup.opacity = 0
            seg1.width = 0
            seg2.width = 0
        }
    }
}
