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
// （i×arcFade）。纯 Rectangle 虚线弧（无 Canvas/ShapePath/ShaderEffect
// ——iOS 静态 Qt 安全）。
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

    // 锚点（C++ 经 DiagAnimator 下发覆盖；默认读 AnimationTokens.js 单一
    // 来源——与 C++ AppState 解析同文件，母版再生成位移仅改 tokens 一处）
    property real anchorCx: Tokens.tokens.wifiWaveAnchorCx
    property real anchorCy: Tokens.tokens.wifiWaveAnchorCy
    property real anchorMaxR: Tokens.tokens.wifiWaveAnchorMaxR

    // 弧组几何（与母版三道信号弧同焦点、同角跨度）——角跨度与半径
    // 系数同读 AnimationTokens.js（几何事实单一来源，见 tokens v6）
    readonly property real _fx: parent.width * root.anchorCx
    readonly property real _fy: parent.height * root.anchorCy
    readonly property real _a0: Tokens.tokens.wifiWaveArcA0
    readonly property real _a1: Tokens.tokens.wifiWaveArcA1
    // 5WHY (复核 2026-08-20 虚线不可见): 片段数/片长曾为固定 9 片 ×
    // 0.045w——默认 40px 图标上外弧弧长仅 ≈9.7px，9 片 1.8px 互相重叠
    // 且片长（1.8px）短于片厚（2.4px），"虚线弧"实际渲染成实心圆角带
    // （设计意图逐条明灭的虚线不可见）。改为按真实弧长推导：片数 =
    // 弧长/(3×片厚)（钳 [3,9]），片长 = 间距 = 弧长/片数×0.5——任意
    // 尺寸下虚线占空比 ≈50%，片长恒 ≥1.5×片厚（业界惯例：虚线段长
    // 不短于线宽，否则视觉上为点而非线）。
    readonly property real _dashTh: Math.max(0.9, parent.width * 0.025)

    Repeater {
        model: 3
        delegate: Item {
            id: arcItem
            // 外→内三道弧半径（相对 anchorMaxR 收缩；index = 外层 Repeater 序号）
            property real radius: parent.width * root.anchorMaxR * Tokens.tokens.wifiWaveArcRadii[index]
            // 5WHY (复核 2026-08-20 负弧长): (_a1 - _a0) = -90° 恒负——
            // 曾直接相乘致 arcLen<0 → segCount 钳死在最小值、dashLen<0 →
            // Rectangle 负宽不渲染（虚线弧全平台不可见，正是本 commit
            // 声称修复的缺陷）。取绝对值（角跨度与方向无关）。
            property real arcLen: Math.abs(root._a1 - root._a0) * Math.PI / 180 * radius
            property int segCount: Math.max(3, Math.min(9, Math.round(arcLen / (3 * root._dashTh))))
            property real dashLen: arcLen / segCount * 0.5
            opacity: 0

            Repeater {
                model: arcItem.segCount
                delegate: Rectangle {
                    // 虚线弧：每片段沿圆弧切线摆放（index = 内层片段序号，
                    // 角度在起角-终角间线性插值）。
                    // 5WHY (复核 2026-08-20 切线镜像): 曾写 rotation:
                    // -(ang+90)——QML 坐标系 y 向下、旋转顺时针为正；由
                    // (cos,sin) 定位的弧上点，其切线方向即 ang+90（有限
                    // 差分可证；同 ConvergeAnimation 径向惯例）。负号把
                    // 每段虚线绕水平轴镜像（弧缘呈斜肋，非顺滑切线弧）。
                    property real ang: root._a0 + (root._a1 - root._a0) * (index / (arcItem.segCount - 1))
                    width: arcItem.dashLen
                    height: root._dashTh
                    radius: height / 2
                    color: root.accentColor
                    x: root._fx + arcItem.radius * Math.cos(ang * Math.PI / 180) - width / 2
                    y: root._fy + arcItem.radius * Math.sin(ang * Math.PI / 180) - height / 2
                    rotation: ang + 90
                }
            }

            // 相位 = index×arcFade：同构序列整体平移 → 逐条亮、逐条灭。
            // 周期 = 3×arcFade + arcHold + arcGap + 3×arcFade（默认 1920ms，
            // 落在 replayWindowMs=2400 的有界重放窗口内）。
            SequentialAnimation {
                id: seq
                loops: Animation.Infinite
                PauseAnimation { duration: index * root.arcFade }
                NumberAnimation {
                    target: arcItem; property: "opacity"
                    from: 0; to: 1; duration: root.arcFade
                    easing.type: Easing.OutCubic
                }
                PauseAnimation { duration: 2 * root.arcFade + root.arcHold + root.arcGap }
                NumberAnimation {
                    target: arcItem; property: "opacity"
                    from: 1; to: 0; duration: root.arcFade
                    easing.type: Easing.InCubic
                }
                PauseAnimation { duration: (2 - index) * root.arcFade }
            }
            // 5WHY (复核 2026-08-20 复位契约): 曾以 onStopped 挂在 delegate
            // Item 上——Item 无 stopped 信号，组件编译失败（Loader 加载
            // 错误，动画整体不出现）。且曾用声明式 running 绑定——stop()
            // 保留 currentTime，重跑从中间相位续播（相位错乱）。改用
            // RestartController 共享命令式契约（restart 从 0 相位开始；
            // 停止即复位本弧 opacity——目标式动画写入断绑，必须显式复位）。
            RestartController {
                running: root.running
                target: seq
                onStopped: arcItem.opacity = 0
            }
        }
    }
}
