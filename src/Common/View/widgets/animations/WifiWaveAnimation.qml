import QtQuick
import "../../theme/AnimationTokens.js" as Tokens

// ── WifiWaveAnimation.qml — 右下角 WiFi 信号弧逐条亮起再逐条熄灭 ──────
// Internet Connectivity & Speed 专用（替代 MeterAnimation 表针摆动）。
//
// 5WHY (2026-08-20 用户诉求 "取消表针动画，改为右下角 wifi 信号弧线
// 逐条显示再逐条消失"): 表针动画历经三轮修调（圆心锚定/双向往复/起始
// 瞬跳）仍与图标语义脱节——internet 母版右下侧是三道红色信号弧
// （350 系坐标 y≈216/236/243 弧组，焦点≈(296,244)），表针扫掠在图形上
// 没有对应元素。业界惯例（WiFi 强度指示器）：信号弧逐条明灭表达
// "正在采样/检测"——弧 1→2→3 逐条显现、保持片刻、再逐条熄灭，循环。
// 三弧共用同构序列（亮→保持→灭→休整），仅按 Repeater index 平移相位
// （i×arcFade），声明式 SequentialAnimation 零绑定、零随机目标。
// 纯 Rectangle 虚线弧（无 Canvas/ShapePath/ShaderEffect——iOS 静态 Qt 安全）。
//
// 焦点/半径由 C++ AppState.diagAnimationAnchor 单一来源下发（与 GeoRadar
// 同机制，母版再生成位移时仅改 C++ 一处）；保留同值默认供直接实例化回退。
//
// Usage: WifiWaveAnimation { anchors.fill: parent; running: testRunning }

AnimationBase {
    id: root
    property int arcFade: Tokens.tokens.wifiWaveFade
    property int arcHold: Tokens.tokens.wifiWaveHold
    property int arcGap:  Tokens.tokens.wifiWaveGap

    // 锚点（C++ 下发；默认 = 母版右下信号弧组焦点 + 外弧半径，归一化到宽度）
    property real anchorCx: 0.85
    property real anchorCy: 0.70
    property real anchorMaxR: 0.155

    // 弧组几何（与母版三道信号弧同焦点、同角跨度）
    readonly property real _fx: parent.width * root.anchorCx
    readonly property real _fy: parent.height * root.anchorCy
    readonly property real _a0: -78      // 起角（右上，度）
    readonly property real _a1: -168     // 终角（左侧，度）
    readonly property int  _segs: 9      // 每道弧的虚线片段数
    readonly property real _dashLen: parent.width * 0.045
    readonly property real _dashTh: Math.max(1.4, parent.width * 0.06)

    Repeater {
        model: 3
        delegate: Item {
            id: arcItem
            // 外→内三道弧半径（相对 anchorMaxR 收缩；index = 外层 Repeater 序号）
            property real radius: parent.width * root.anchorMaxR * [1.0, 0.74, 0.48][index]
            opacity: 0

            Repeater {
                model: root._segs
                delegate: Rectangle {
                    // 虚线弧：每片段沿圆弧切线摆放（index = 内层片段序号，
                    // 角度在起角-终角间线性插值）
                    property real ang: root._a0 + (root._a1 - root._a0) * (index / (root._segs - 1))
                    width: root._dashLen
                    height: root._dashTh
                    radius: height / 2
                    color: root.accentColor
                    x: root._fx + arcItem.radius * Math.cos(ang * Math.PI / 180) - width / 2
                    y: root._fy + arcItem.radius * Math.sin(ang * Math.PI / 180) - height / 2
                    rotation: -(ang + 90)
                }
            }

            // 相位 = index×arcFade：同构序列整体平移 → 逐条亮、逐条灭。
            // 周期 = 3×arcFade + arcHold + arcGap + 3×arcFade（默认 1920ms，
            // 落在 replayWindowMs=2400 的有界重放窗口内）。
            SequentialAnimation on opacity {
                running: root.running
                loops: Animation.Infinite
                PauseAnimation { duration: index * root.arcFade }
                NumberAnimation { from: 0; to: 1; duration: root.arcFade; easing.type: Easing.OutCubic }
                PauseAnimation { duration: 2 * root.arcFade + root.arcHold + root.arcGap }
                NumberAnimation { from: 1; to: 0; duration: root.arcFade; easing.type: Easing.InCubic }
                PauseAnimation { duration: (2 - index) * root.arcFade }
            }
            // 5WHY (复核 2026-08-20 复位契约): Repeater 子项无法从 root 以 id
            // 引用——基类 resetVisuals() 覆写不可行。running→false 时动画
            // 进入 Stopped 态即触发 onStopped（loops:Infinite 只有外部停止
            // 才触发），在此把本弧 opacity 复位为 0，等价于 resetVisuals。
            onStopped: arcItem.opacity = 0
        }
    }
}
