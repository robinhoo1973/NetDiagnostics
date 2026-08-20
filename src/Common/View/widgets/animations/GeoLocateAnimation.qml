import QtQuick
import "../../theme/AnimationTokens.js" as Tokens

// ── GeoLocateAnimation.qml — IP Geolocation 雷达定位 ────────────────────
// 5WHY (2026-08-19 用户诉求 "IP 动画不知道在干什么"): GeoIP 曾复用通用
// Bounce（两点间往返小球）——地球+定位针图形上一条横穿圆点没有语义锚点，
// 观者无法建立"定位"联想。业界惯例（地图定位 ping）：从定位针头辐射同心
// 雷达波，逐圈扩散淡出循环——语义即"正在定位"。
// 锚点对齐 geoip 母版定位针头中心（QSvgRenderer 逐通道实测 viewBox
// ≈(0.71,0.30)），扩散半径 0.29×宽（到右缘的最近距离）覆盖针头至针身。
// 纯 Rectangle + scale/opacity 动画——无 Canvas/ShapePath/ShaderEffect
// （iOS 静态 Qt 安全）。
//
// Usage: 经 DiagAnimator 装载（锚点由 C++ diagAnimationAnchor 下发）——
//   DiagAnimator { anchors.fill: parent; diagId: ...; running: testRunning }

AnimationBase {
    id: root

    // 锚点：geoip 母版定位针头中心（实测 ≈(0.71, 0.30)）。C++ 经
    // DiagAnimator 装载时下发覆盖；默认读 AnimationTokens.js 单一来源
    // （C++ AppState::diagAnimationAnchor 解析同文件——曾 C++/QML 同值
    // 双份靠注释"两处同改"维系，母版位移改一处即静默错位）。
    property real anchorCx: Tokens.tokens.geoRadarAnchorCx
    property real anchorCy: Tokens.tokens.geoRadarAnchorCy
    property real anchorMaxR: Tokens.tokens.geoRadarAnchorMaxR
    readonly property real _cx: parent.width * root.anchorCx
    readonly property real _cy: parent.height * root.anchorCy
    // 5WHY (复核 2026-08-19 边界): 半径在 (cx,cy) 锚点上越界（无裁剪链——
    // 环画到垫外邻内容上）。半径上限=到最近边缘距离（右 1-cx / 顶 cy），
    // 圆完全落回图标框内。
    // 5WHY (复核 2026-08-19 回归): 第二 min 项曾误写 (1.0 - anchorCy)——那是
    // 到【底】边的距离（0.70），顶边距离是 anchorCy 本身（0.30）；方形父项
    // 下该项恒不生效（死算术），非方垫下环溢出顶边。修正为 anchorCy。
    readonly property real _maxR: Math.min(parent.width * root.anchorMaxR,
                                            parent.height * root.anchorCy)
    readonly property int _period: Tokens.tokens.geoRadarPeriod
    readonly property int _stagger: Tokens.tokens.geoRadarStagger

    // 三圈同心雷达波；每圈周期 = period + 2×stagger（错峰启动 + 等其余圈
    // 完成），全组同周期无缝循环。
    // 5WHY (复核 2026-08-19 圈数单一来源): 尾部相位数学曾硬编码 (2 - index)
    // ——加第 4 圈即负相位钳 0、无缝循环破坏。_rings 单一来源供 model 与
    // 相位共用。
    property int _rings: 3
    Repeater {
        model: root._rings
        delegate: Rectangle {
            id: ring
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
            scale: 0.15

            // 5WHY (复核 2026-08-19): 不用声明式 running 绑定——Qt 的
            // stop() 保留 currentTime，重新 start() 会从中途续播（相位
            // 错乱）。委托级 Connections 命令式 restart()/stop()，每次
            // 启动都从 0 相位开始；停止时各圈自复位（根级循环会硬编码
            // 圈数 3——加第 4 圈即漏复位）。
            // 5WHY (复核 2026-08-19 创建即真): 属性变更处理器不响应创建期
            // 初值——以 running:true 直接实例化（文件 Usage 注释的用法）
            // 时 restart 永不触发。onCompleted 兜底补一次启动判定。
            Component.onCompleted: if (root.running) seq.restart()
            Connections {
                target: root
                function onRunningChanged() {
                    if (root.running) seq.restart()
                    else {
                        seq.stop()
                        ring.opacity = 0
                        ring.scale = 0.15
                    }
                }
            }
            SequentialAnimation {
                id: seq
                loops: Animation.Infinite
                PauseAnimation { duration: index * root._stagger }
                ParallelAnimation {
                    NumberAnimation {
                        target: ring; property: "scale"
                        from: 0.15; to: 1.0
                        duration: root._period
                        easing.type: Easing.OutCubic
                    }
                    NumberAnimation {
                        target: ring; property: "opacity"
                        from: 0.75; to: 0.0
                        duration: root._period
                        easing.type: Easing.Linear
                    }
                }
                // 复位 + 等其余圈完成，保持全组同周期
                PropertyAction { target: ring; property: "opacity"; value: 0 }
                PropertyAction { target: ring; property: "scale"; value: 0.15 }
                PauseAnimation { duration: (root._rings - 1 - index) * root._stagger }
            }
        }
    }
}
