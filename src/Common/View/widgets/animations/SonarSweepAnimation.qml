import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── SonarSweepAnimation.qml — Ping 声呐雷达扫描 ──────────────────────────
// 5WHY (2026-08-23 设计文档 review/refactor/ui/anim/04-g4-route-animations.md):
// Ping 曾复用通用 Bounce（两点往返小球）——与 2026-08-23 新母版
// nd-diag-g4-ping（Lucide radar：中心主机环 + 外弧组）无语义锚点。
// 雷达语义即"波束扫过 → 回波扩散"：细波束绕中心主机环匀速整圈旋转，
// 每圈自中心扩散一枚回波环（与 GeoRadar 的纯辐射波不同——那里无波束，
// 这里扫掠是主视觉，互斥成立）。
//
// 几何锚定母版（归一化，母版位移改此处）：中心主机环 circle(12,12,r2)
// → cx=cy=0.5、内半径 2/24≈0.083；外弧 r10 → 最大半径 10/24≈0.417
// （波束/回波止于外弧，不溢出图标框）。
// 纯 Rectangle + rotation/scale/opacity —— 无 Canvas/ShapePath/ShaderEffect
// （iOS 静态 Qt 安全，同 GeoLocate 契约）。
//
// Usage: 经 DiagAnimator 装载——
//   DiagAnimator { anchors.fill: parent; diagId: ...; running: testRunning }

AnimationBase {
    id: root

    // DIAG_ACCENT(g4-ping) = success 绿（探测脉冲语义，与母版 accent 同源）
    accentColor: T.ThemeEngine.colors.success

    readonly property real _cx: parent.width * 0.5
    readonly property real _cy: parent.height * 0.5
    // 半径上限 = 中心到最近边缘距离（方垫下即 h/2），环不越框
    readonly property real _maxR: Math.min(parent.width * 0.417,
                                           parent.height / 2)
    readonly property real _innerR: Math.min(parent.width, parent.height) * 0.083
    readonly property int _period: Tokens.tokens.sonarSweepMs

    // ── 扫描波束：自主机环边缘到外弧的细条，绕中心匀速整圈 ──────────────
    Rectangle {
        id: beam
        // 5WHY (复核 2026-08-23): x 曾取 _cx——波束左端落在圆心而非主机环
        // 边缘，扫掠横穿主机点，与注释"自主机环边缘"不符。内半径偏移补上。
        x: root._cx + root._innerR
        y: root._cy - height / 2
        width: root._maxR - root._innerR
        height: Math.max(1.5, root._maxR * 0.055)
        radius: height / 2
        color: root.accentColor
        opacity: 0.85
        transformOrigin: Item.Left
        rotation: 0

        SequentialAnimation {
            id: beamSeq
            loops: Animation.Infinite
            // 5WHY (GeoLocate 同契约): 不用声明式 running 绑定——stop() 保留
            // currentTime 续播错相位；RestartController 命令式 restart 从 0 起。
            NumberAnimation {
                target: beam; property: "rotation"
                from: 0; to: 360
                duration: root._period
                easing.type: Easing.Linear
            }
        }
        RestartController {
            running: root.running
            target: beamSeq
            onStopped: beam.rotation = 0
        }
    }

    // ── 回波环：每圈自主机环扩散一枚，与波束同周期同步 ───────────────────
    Rectangle {
        id: echo
        x: root._cx - width / 2
        y: root._cy - height / 2
        width: root._maxR * 2
        height: root._maxR * 2
        radius: width / 2
        color: "transparent"
        border {
            width: Math.max(1.5, root._maxR * 1.5 / 24)
            color: root.accentColor
        }
        opacity: 0
        scale: 0.2

        SequentialAnimation {
            id: echoSeq
            loops: Animation.Infinite
            ParallelAnimation {
                NumberAnimation {
                    target: echo; property: "scale"
                    from: 0.2; to: 1.0
                    duration: root._period
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    target: echo; property: "opacity"
                    from: 0.65; to: 0.0
                    duration: root._period
                    easing.type: Easing.Linear
                }
            }
            PropertyAction { target: echo; property: "opacity"; value: 0 }
            PropertyAction { target: echo; property: "scale"; value: 0.2 }
        }
        RestartController {
            running: root.running
            target: echoSeq
            onStopped: {
                echo.opacity = 0
                echo.scale = 0.2
            }
        }
    }

    function resetVisuals() {
        beam.rotation = 0
        echo.opacity = 0
        echo.scale = 0.2
    }
}
