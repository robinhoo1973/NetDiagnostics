import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── HopSampleAnimation.qml — PathPing 逐跳统计采样 ───────────────────────
// 5WHY (2026-08-23 设计文档 review/refactor/ui/anim/04-g4-route-animations.md):
// PathPing 曾与 Traceroute 共用通用 Path（同构剪影 + 同款动画，双重趋同）。
// pathping 的语义差 = "逐跳采样统计"：沿母版 nd-diag-g4-pathping
// （Lucide waypoints 菱形四节点链）节点序 顶→左→右→底 逐点打采样环，
// 连接段随下游节点点亮同步闪光；末拍右节点（母版 accent 内心 = 统计目标）
// 补一枚双拍强调环——"统计锁定"。
//
// 几何锚定母版（归一化，母版位移改此处）：四节点 circle r2.5 →
// T(12,4.5) L(4.5,12) R(19.5,12) B(12,19.5)；连接段 m10.2 6.3-3.9 3.9 /
// M7 12h10 / m13.8 17.7 3.9-3.9。纯 Rectangle + scale/opacity（iOS 静态
// Qt 安全，同 GeoLocate 契约）。
//
// 时序契约（GeoLocate 同款）：每委托自带全组等长 SequentialAnimation——
// 前置 Pause 错峰 + 尾部 Pause 补齐周期，无缝循环；RestartController
// 命令式 restart/stop，停止时逐项复位。
//
// Usage: 经 DiagAnimator 装载——
//   DiagAnimator { anchors.fill: parent; diagId: ...; running: testRunning }

AnimationBase {
    id: root

    // DIAG_ACCENT(g4-pathping) = warning 琥珀（逐跳统计语义，与母版 accent 同源）
    accentColor: T.ThemeEngine.colors.warning

    readonly property int _stagger: Tokens.tokens.hopSampleStagger
    readonly property int _pulseMs: Tokens.tokens.hopSamplePulseMs
    readonly property int _holdMs: Tokens.tokens.hopSampleHoldMs

    // 节点序（顶→左→右→底）；末项 = 右节点双拍强调（delay 在末拍之后）
    // 5WHY (单一来源): 周期数学只认本表长度——加节点即自动扩周期。
    readonly property var _samples: [
        { nx: 0.5,    ny: 0.1875 },
        { nx: 0.1875, ny: 0.5    },
        { nx: 0.8125, ny: 0.5    },
        { nx: 0.5,    ny: 0.8125 },
        // 双拍强调：右节点（统计目标），末节点起拍后再错半步
        { nx: 0.8125, ny: 0.5, delayExtra: _pulseMs }
    ]
    readonly property int _period: (_samples.length - 1) * _stagger + _pulseMs + _holdMs

    // 连接段闪光：段 j 的远端节点 = samples[j+1]——随其采样同步亮起
    readonly property var _segments: [
        // 顶→左 对角（m10.2 6.3 -3.9 3.9 段）
        { x1: 0.425, y1: 0.2625, x2: 0.2625, y2: 0.425, far: 1 },
        // 左→右 水平（M7 12h10）
        { x1: 0.292, y1: 0.5, x2: 0.708, y2: 0.5, far: 2 },
        // 右→底 对角（m13.8 17.7 3.9 -3.9 段）
        { x1: 0.575, y1: 0.7375, x2: 0.7375, y2: 0.575, far: 3 }
    ]

    Repeater {
        id: segsRep
        model: root._segments.length
        Rectangle {
            id: seg
            property var cfg: root._segments[index]
            property real _x1: cfg.x1 * root.width
            property real _y1: cfg.y1 * root.height
            property real _x2: cfg.x2 * root.width
            property real _y2: cfg.y2 * root.height
            // 中点定位 + 绕中心旋转到段向
            x: (_x1 + _x2) / 2 - width / 2
            y: (_y1 + _y2) / 2 - height / 2
            width: Math.sqrt((_x2 - _x1) * (_x2 - _x1) + (_y2 - _y1) * (_y2 - _y1))
            height: Math.max(1.5, root.width * 0.05)
            radius: height / 2
            color: root.accentColor
            opacity: 0.18

            SequentialAnimation {
                id: segSeq
                loops: Animation.Infinite
                // 远端节点起拍前 80ms 抢入闪光，覆盖其采样窗口
                PauseAnimation {
                    duration: Math.max(0, seg.cfg.far * root._stagger - 80)
                }
                NumberAnimation {
                    target: seg; property: "opacity"
                    from: 0.18; to: 0.95
                    duration: root._pulseMs / 2
                    easing.type: Easing.OutQuad
                }
                NumberAnimation {
                    target: seg; property: "opacity"
                    from: 0.95; to: 0.18
                    duration: root._pulseMs / 2
                    easing.type: Easing.InQuad
                }
                PauseAnimation {
                    duration: Math.max(0, root._period
                        - (seg.cfg.far * root._stagger - 80) - root._pulseMs)
                }
            }
            RestartController {
                running: root.running
                target: segSeq
                onStopped: seg.opacity = 0.18
            }
        }
    }

    Repeater {
        id: samplesRep
        model: root._samples.length
        Rectangle {
            id: ring
            property var cfg: root._samples[index]
            // 环基径 = 母版节点直径 (r2.5×2=5)/24；强调环略大（0.23：
            // ×1.6 峰值半宽 0.184S，距右缘 0.8125 不越框——0.26 曾溢框
            // 2%，GeoLocate「环画到垫外」同类）
            readonly property real base: root.width * (index === root._samples.length - 1 ? 0.23 : 0.21)
            x: cfg.nx * root.width - width / 2
            y: cfg.ny * root.height - height / 2
            width: base
            height: base
            radius: width / 2
            color: "transparent"
            border {
                width: Math.max(1.5, root.width * 0.065)
                color: root.accentColor
            }
            opacity: 0
            scale: 0.45

            SequentialAnimation {
                id: sampleSeq
                loops: Animation.Infinite
                PauseAnimation {
                    duration: index * root._stagger + (cfg.delayExtra || 0)
                }
                ParallelAnimation {
                    NumberAnimation {
                        target: ring; property: "scale"
                        from: 0.45; to: 1.6
                        duration: root._pulseMs
                        easing.type: Easing.OutCubic
                    }
                    NumberAnimation {
                        target: ring; property: "opacity"
                        from: 0.75; to: 0.0
                        duration: root._pulseMs
                        easing.type: Easing.Linear
                    }
                }
                PropertyAction { target: ring; property: "opacity"; value: 0 }
                PropertyAction { target: ring; property: "scale"; value: 0.45 }
                // 尾部补齐全组周期（含 delayExtra 占用的额外拍）
                PauseAnimation {
                    duration: Math.max(0, root._period
                        - index * root._stagger - (cfg.delayExtra || 0) - root._pulseMs)
                }
            }
            RestartController {
                running: root.running
                target: sampleSeq
                onStopped: {
                    ring.opacity = 0
                    ring.scale = 0.45
                }
            }
        }
    }

    function resetVisuals() {
        for (var i = 0; i < samplesRep.count; ++i) {
            var d = samplesRep.itemAt(i)
            if (d) { d.opacity = 0; d.scale = 0.45 }
        }
    }
}
