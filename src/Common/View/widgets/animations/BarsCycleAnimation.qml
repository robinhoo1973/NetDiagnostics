import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── BarsCycleAnimation.qml — Cellular 信号柱颜色轮换 ──────────────────
// Cellular Information 专用（2026-08-22 替代 Pulse 泛型呼吸）。
//
// 5WHY (复核 2026-08-22 用户诉求 "那几个信号的柱的颜色不停的变化"):
// Cellular 曾挂 Pulse 泛型动画——整图透明度呼吸，与图标语义（四根
// 信号柱）无对应关系，用户感知不到"在检测"。业界惯例（蜂窝信号指示）：
// 四根柱沿高亮色盘连续轮换、起色按柱位错位——任一时刻四柱各持不同
// 颜色且各自流动（"分色流光"），表达"信号采样中"。
//
// 柱几何 = 母版 nd-diag-g1-cellular.svg 事实（/24 归一化：柱中心
// x=6.4/10.1/13.8/17.5，底 18，柱顶 y=15/13/11/9，线宽 1.6）。纯
// Rectangle 柱（无 Canvas/ShapePath——iOS 静态 Qt 安全）。ColorAnimation
// 序列与 RestartController 共享命令式契约（restart 从 0 相位、停止复位
// 本柱颜色——目标式动画写入断绑，必须显式复位）。
//
// Usage: BarsCycleAnimation { anchors.fill: parent; running: testRunning }

AnimationBase {
    id: root
    property int colorStep: Tokens.tokens.barsCycleColorStep

    // 4 色高亮盘（主题角色即时切换；双主题下随 ThemeEngine 重解析）
    readonly property var _palette: [T.ThemeEngine.colors.primary,
        T.ThemeEngine.colors.tertiary, T.ThemeEngine.colors.warning,
        T.ThemeEngine.colors.success]
    // 母版柱几何（/24 归一化）
    readonly property var _barX:   [6.4, 10.1, 13.8, 17.5]
    readonly property var _barTop: [15, 13, 11, 9]
    readonly property real _barBottom: 18
    readonly property real _bw: Math.max(1.5, parent.width * 1.6 / 24)

    Repeater {
        model: 4
        delegate: Item {
            id: barItem
            // index = 柱位序号（0=最矮柱，3=最高柱）；起色按柱位错位，
            // 相邻柱相差一个 colorStep 相位——任一时刻四柱颜色互异。
            property color _barColor: root._palette[index % 4]

            Rectangle {
                width: root._bw
                height: Math.max(3, parent.height * (root._barBottom - root._barTop[index]) / 24)
                radius: width / 2
                color: barItem._barColor
                x: parent.width * root._barX[index] / 24 - width / 2
                y: parent.height * root._barBottom / 24 - height
            }
            SequentialAnimation on _barColor {
                id: colorSeq
                loops: Animation.Infinite
                ColorAnimation { from: root._palette[(index + 0) % 4]; to: root._palette[(index + 1) % 4]; duration: root.colorStep }
                ColorAnimation { from: root._palette[(index + 1) % 4]; to: root._palette[(index + 2) % 4]; duration: root.colorStep }
                ColorAnimation { from: root._palette[(index + 2) % 4]; to: root._palette[(index + 3) % 4]; duration: root.colorStep }
                ColorAnimation { from: root._palette[(index + 3) % 4]; to: root._palette[(index + 0) % 4]; duration: root.colorStep }
            }
            RestartController {
                running: root.running
                target: colorSeq
                onStopped: barItem._barColor = root._palette[index % 4]
            }
        }
    }
}
