import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── BarsCycleAnimation.qml — Cellular 信号柱颜色轮换（SVG 复绘）────────
// Cellular Information 专用（2026-08-22 替代 Pulse 泛型呼吸）。
//
// 5WHY (复核 2026-08-22 用户诉求 "那几个信号的柱的颜色不停的变化"):
// Cellular 曾挂 Pulse 泛型动画——整图透明度呼吸，与图标语义（四根
// 信号柱）无对应关系，用户感知不到"在检测"。业界惯例（蜂窝信号指示）：
// 四根柱沿高亮色盘连续轮换、起色按柱位错位——任一时刻四柱各持不同
// 颜色且各自流动（"分色流光"），表达"信号采样中"。
//
// 5WHY (复核 2026-08-22 用户诉求 "Cellular 动画采用 signal bar 的颜色
// 轮换变化的方式，类似与 WIFI 弧线的方式"): 曾以 Rectangle + 手算柱几何
// 复刻母版（x/y/宽全部推导值）且用 ColorAnimation 连续插值换色。改为
// 与 WifiWave 同机制的 SVG 复绘：柱路径 = ffffff 母版
// nd-diag-g1-cellular.svg 逐字事实（AnimationTokens.js barsCycleSets，
// 几何零漂移），每次换色步重新生成 data-URI SVG 注入下一档高亮色。
// 四柱同时推进调色盘（柱位错位起色，任一时刻四柱各持不同高亮色）。
// 纯 Image + data-URI + 属性动画（无 Canvas/ShapePath/ShaderEffect——
// iOS 静态 Qt 安全；QSvgPlugin 已随 WifiWave 静态导入）。
//
// Usage: BarsCycleAnimation { anchors.fill: parent; running: testRunning }

AnimationBase {
    id: root
    property int colorStep: Tokens.tokens.barsCycleColorStep

    // 4 色高亮盘（主题角色即时切换；双主题下随 ThemeEngine 重解析）
    readonly property var _palette: [T.ThemeEngine.colors.primary,
        T.ThemeEngine.colors.tertiary, T.ThemeEngine.colors.warning,
        T.ThemeEngine.colors.success]
    // 母版柱路径（单一来源；缺省回退蜂窝母版柱集）
    readonly property var _barSet: root.iconName !== ""
        ? (Tokens.tokens.barsCycleSets[root.iconName] || null)
        : null
    readonly property var _bars: root._barSet || Tokens.tokens.barsCycleSets["nd-diag-g1-cellular"]

    // QML color.toString() 可能是 #RRGGBB 或 #AARRGGBB——统一取后 6 位。
    function _hexOf(c) {
        var s = c.toString()
        return s.length > 7 ? s.slice(3) : s.slice(1)
    }
    // 单柱 SVG 复绘：注入当前调色盘色（stroke 柱路径）。
    function _barUri(i, palIdx) {
        var b = root._bars[i]
        var hex = root._hexOf(root._palette[palIdx % 4])
        return "data:image/svg+xml;base64," + Qt.btoa(
            '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none" stroke-linecap="round" stroke-linejoin="round">'
            + '<path d="' + b.stroke + '" stroke="#' + hex + '" stroke-width="' + b.sw + '" fill="none"/>'
            + '</svg>')
    }

    Repeater {
        model: 4
        delegate: Item {
            id: barItem
            width: parent.width
            height: parent.height
            // index = 柱位序号（0=最矮柱，3=最高柱）；起色按柱位错位，
            // 每 colorStep 整组推进一格——任一时刻四柱颜色互异且不断流动。
            property int _palIdx: index
            Image {
                anchors.fill: parent
                // 96px 固定源尺寸：≤56px 图标井内 SVG 放大渲染不糊
                sourceSize.width: 96
                sourceSize.height: 96
                smooth: true
                mipmap: true
                source: root._barUri(index, barItem._palIdx)
            }
            // 连续轮换：每 colorStep 推进一格调色盘（无明灭——柱恒可见，
            // 换色即动画主体）。4 步一轮回，起点按柱位错位。
            SequentialAnimation {
                id: seq
                loops: Animation.Infinite
                ScriptAction { script: { barItem._palIdx = (barItem._palIdx + 1) % 4 } }
                PauseAnimation { duration: root.colorStep }
                ScriptAction { script: { barItem._palIdx = (barItem._palIdx + 1) % 4 } }
                PauseAnimation { duration: root.colorStep }
                ScriptAction { script: { barItem._palIdx = (barItem._palIdx + 1) % 4 } }
                PauseAnimation { duration: root.colorStep }
                ScriptAction { script: { barItem._palIdx = (barItem._palIdx + 1) % 4 } }
                PauseAnimation { duration: root.colorStep }
            }
            // 5WHY (复核 2026-08-20 复位契约): 与 GeoLocate/WifiWave 同款
            // 命令式契约——restart 从 0 相位开始；停止即复位柱相位。
            RestartController {
                running: root.running
                target: seq
                onStopped: barItem._palIdx = index
            }
        }
    }
}
