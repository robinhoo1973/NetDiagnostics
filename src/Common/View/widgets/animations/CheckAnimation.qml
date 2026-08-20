import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── CheckAnimation.qml — 盾牌内打勾消失/重现（DNS Integrity）────────────
// 用两根纯 Rectangle 线段复刻 shield-check 母版的勾（24 viewBox：
// (9,12)→(11,14)→(15,10)）+ 一块盖片（cover）实现"勾消失 → 逐渐重现"
// 循环。
//
// 5WHY (复核 2026-08-21 用户诉求 "DNS Integrity 动画是消失当中的 tick，
// 然后再逐渐显示，如此往复"): 曾实现为"逐笔显现 → 保持 → 整体淡出"——
// 但母版盾牌里已有一枚静态勾，动画勾与静态勾同位同形，淡出后静态勾
// 原样可见：整个动画在视觉上近乎不可见，且勾从未真正消失。修正：动画
// 勾（success 绿，凸显"检测通过"语义）常显绘制；盖片以图标垫底色
// （surfaceContainerLow + 运行态 primary 0.10 光晕合成）渐入遮住勾
// （消失），保持后渐出（勾逐渐重现），循环。纯 Rectangle —— 无
// Canvas/ShapePath/ShaderEffect（iOS 静态 Qt 安全）。
//
// 5WHY (复核 2026-08-21 相位错乱): 曾用声明式 running 绑定——Qt 的
// stop() 保留 currentTime，重新 start() 会从中途续播（与 GeoLocate/
// WifiWave 同款缺陷，彼处已修此处未修）。改用 RestartController 共享
// 命令式契约（restart 从 0 相位开始；停止即复位视觉）。
//
// Usage: CheckAnimation { anchors.fill: parent; running: testRunning }

AnimationBase {
    id: root
    // Check 成功语义：accent 覆盖为 success 令牌
    property color accentColor: T.ThemeEngine.colors.success
    // 消失渐入时长（勾被遮住）
    property int hideDuration: 220
    // 勾保持消失/重现后保持的时长（Tokens 单一来源）
    property int holdDuration: Tokens.tokens.checkHoldDuration
    // 盖片渐出（勾逐渐重现）时长
    property int revealDuration: Tokens.tokens.checkDrawDuration

    // 勾线段几何（24 viewBox → 父级像素；与盾牌母版勾位一致）
    readonly property real _s: parent.width / 24
    readonly property real _stroke: Math.max(1.5, 2.2 * _s)
    readonly property real _len1: Math.sqrt(8) * _s      // (9,12)→(11,14) ≈2.83u
    readonly property real _len2: Math.sqrt(32) * _s     // (11,14)→(15,10) ≈5.66u
    readonly property real _p1x: 9 * _s
    readonly property real _p1y: 12 * _s
    readonly property real _p2x: 11 * _s
    readonly property real _p2y: 14 * _s

    // 5WHY (复核 2026-08-21 盖片底色): 母版盾牌仅描边无填充——勾位于图标
    // 透明区内，其下是瓦片光晕垫（运行态 primary 0.10 叠加 surfaceContainerLow
    // 渐变卡）。盖片以同合成色近似（Qt.tint 线性混合），hero/瓦片两消费
    // 方皆近隐形（两处光晕 alpha 仅 0.10/0.12，差异不可感知）。
    readonly property color _coverColor: Qt.tint(T.ThemeEngine.colors.surfaceContainerLow,
        Qt.rgba(T.ThemeEngine.colors.primary.r, T.ThemeEngine.colors.primary.g,
                T.ThemeEngine.colors.primary.b, 0.10))

    Item {
        id: checkGroup
        anchors.fill: parent
        z: 1

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

    // 盖片：遮住勾区（含笔画线宽余量），渐入=消失、渐出=逐渐重现
    Rectangle {
        id: cover
        z: 2
        x: 8.2 * root._s
        y: 9.4 * root._s
        width: 8.0 * root._s
        height: 5.6 * root._s
        radius: 2.0 * root._s
        color: root._coverColor
        opacity: 0
    }

    // 循环：勾全显（PropertyAction 两笔全宽）→ 盖片渐入（勾消失）→
    // 保持 → 盖片渐出（勾逐渐重现）→ 保持。周期 = 220+420+420+420
    // = 1480ms，落在 replayWindowMs=2400 的有界重放窗口内。
    SequentialAnimation {
        id: seq
        loops: Animation.Infinite
        PropertyAction { target: seg1; property: "width"; value: root._len1 }
        PropertyAction { target: seg2; property: "width"; value: root._len2 }
        NumberAnimation {
            target: cover; property: "opacity"
            from: 0; to: 1; duration: root.hideDuration
            easing.type: Easing.OutCubic
        }
        PauseAnimation { duration: root.holdDuration }
        NumberAnimation {
            target: cover; property: "opacity"
            from: 1; to: 0; duration: root.revealDuration
            easing.type: Easing.InOutQuad
        }
        PauseAnimation { duration: root.holdDuration }
    }
    RestartController {
        running: root.running
        target: seq
        onStopped: root.resetVisuals()
    }

    function resetVisuals() {
        cover.opacity = 0
        seg1.width = 0
        seg2.width = 0
    }
}
