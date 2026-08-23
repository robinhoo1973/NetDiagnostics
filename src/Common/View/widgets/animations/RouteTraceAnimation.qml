import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── RouteTraceAnimation.qml — Traceroute 探测包沿径迹行进 ────────────────
// 5WHY (2026-08-23 设计文档 review/refactor/ui/anim/04-g4-route-animations.md):
// Traceroute 曾复用通用 Path（五个方点横排闪现）——与 2026-08-23 新母版
// nd-diag-g4-traceroute（Lucide route：起点环 → S 形路径 → 终点环）无几何
// 关联。语义即"探测包从源主机沿路径行进，命中目标时终点脉冲"：
// 包点沿母版 S 脊线匀速单程（PathInterpolator 精确复刻圆弧），到达后
// 终点环处扩散命中环，休整一轮。
//
// 几何锚定母版（24 系坐标逐字，母版位移改此处）：
//   M9 19 H17.5  A3.5→(17.5,12) 右凸  H6.5  A3.5→(6.5,5) 左凸  H15
// Qt PathArc direction ↔ SVG sweep-flag（y 向下同约定）：sweep=0 =
// Counterclockwise、sweep=1 = Clockwise。PathInterpolator 为纯 QtQuick
// 元素（非 Shape/Canvas——iOS 静态 Qt 安全，同 GeoLocate 契约）。
//
// Usage: 经 DiagAnimator 装载——
//   DiagAnimator { anchors.fill: parent; diagId: ...; running: testRunning }

AnimationBase {
    id: root

    // DIAG_ACCENT(g4-traceroute) = info 蓝（命中目标语义，与母版 accent 同源）
    accentColor: T.ThemeEngine.colors.info

    readonly property int _travelMs: Tokens.tokens.routeTravelMs
    readonly property int _arrivalMs: Tokens.tokens.routeArrivalMs
    readonly property int _holdMs: Tokens.tokens.routeHoldMs
    readonly property int _fadeMs: Tokens.tokens.routeFadeMs

    // 母版 S 脊线逐字（像素坐标随父项缩放）
    Path {
        id: spine
        startX: root.width * (9 / 24)
        startY: root.height * (19 / 24)
        PathLine { x: root.width * (17.5 / 24); y: root.height * (19 / 24) }
        PathArc {
            x: root.width * (17.5 / 24); y: root.height * (12 / 24)
            radius: root.width * (3.5 / 24)
            direction: PathArc.Counterclockwise   // SVG sweep=0 → 右凸
        }
        PathLine { x: root.width * (6.5 / 24); y: root.height * (12 / 24) }
        PathArc {
            x: root.width * (6.5 / 24); y: root.height * (5 / 24)
            radius: root.width * (3.5 / 24)
            direction: PathArc.Clockwise          // SVG sweep=1 → 左凸
        }
        PathLine { x: root.width * (15 / 24); y: root.height * (5 / 24) }
    }
    PathInterpolator { id: interp; path: spine }

    // ── 探测包 ────────────────────────────────────────────────────────────
    Rectangle {
        id: packet
        width: Math.max(5, root.width * 0.13)
        height: width
        radius: width / 2
        color: root.accentColor
        // progress=0 位于起点环右缘 (9,19)
        x: interp.x - width / 2
        y: interp.y - height / 2
    }

    // ── 终点命中环：终点环 (18,5) 处扩散淡出 ──────────────────────────────
    Rectangle {
        id: hitRing
        property real _cx: root.width * (18 / 24)
        property real _cy: root.height * (5 / 24)
        x: _cx - width / 2
        y: _cy - height / 2
        // 5WHY (复核 2026-08-23 GeoLocate 同类回归): 直径曾取 0.75×宽——
        // 锚点贴右上角 (0.75, 0.208)，满幅半径 0.375S 向右溢出框 12%、向顶
        // 溢出 ~17%（"环画到垫外邻内容上"同类）。直径钳到锚点最近边缘距离
        // ×2：min(1−.75, .208)×2 ≈ 0.416S。
        width: Math.max(12, root.width * 0.42)
        height: width
        radius: width / 2
        color: "transparent"
        border {
            width: Math.max(1.5, root.width * 0.06)
            color: root.accentColor
        }
        opacity: 0
        scale: 0.25
    }

    SequentialAnimation {
        id: seq
        loops: Animation.Infinite

        // 5WHY (复核 2026-08-23 瞬移): progress 1→0 环首重绕时包点从终点
        // 瞬移回起点——加首拍显形 + 末拍淡出（fade-in 0 由 PropertyAction
        // 承担，周期 = travel+arrival+hold+fade = 2350ms）。
        PropertyAction { target: packet; property: "opacity"; value: 1 }
        NumberAnimation {
            target: interp; property: "progress"
            from: 0; to: 1
            duration: root._travelMs
            easing.type: Easing.Linear
        }
        // 命中脉冲：包点驻留终点，环扩散
        ParallelAnimation {
            NumberAnimation {
                target: hitRing; property: "scale"
                from: 0.25; to: 1.0
                duration: root._arrivalMs
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: hitRing; property: "opacity"
                from: 0.7; to: 0.0
                duration: root._arrivalMs
                easing.type: Easing.Linear
            }
        }
        PauseAnimation { duration: root._holdMs }
        // 末拍淡出：包点消隐于终点，下一轮从起点重现
        NumberAnimation {
            target: packet; property: "opacity"
            from: 1; to: 0
            duration: root._fadeMs
            easing.type: Easing.OutQuad
        }
    }

    RestartController {
        running: root.running
        target: seq
        onStopped: {
            interp.progress = 0
            packet.opacity = 1
            hitRing.opacity = 0
            hitRing.scale = 0.25
        }
    }

    function resetVisuals() {
        interp.progress = 0
        packet.opacity = 1
        hitRing.opacity = 0
        hitRing.scale = 0.25
    }
}
