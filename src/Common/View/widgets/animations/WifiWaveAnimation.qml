import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── WifiWaveAnimation.qml — WiFi 信号弧逐条亮起 + 高亮色轮换（SVG 复绘）──
// Internet Connectivity & Speed / WiFi Information 专用（替代 MeterAnimation
// 表针摆动）。
//
// 5WHY (2026-08-20 用户诉求 "取消表针动画，改为右下角 wifi 信号弧线
// 逐条显示再逐条消失"): 表针动画历经三轮修调（圆心锚定/双向往复/起始
// 瞬跳）仍与图标语义脱节——internet 母版右下侧是三道红色信号弧，表针
// 扫掠在图形上没有对应元素。业界惯例（WiFi 强度指示器）：信号弧逐条
// 明灭表达 "正在采样/检测"。
//
// 5WHY (复核 2026-08-21 用户诉求 "从最短的弧线开始逐层显示，再从最长的
// 弧线开始逐层抹去"): 曾以 Repeater index 平移相位（i×arcFade）——index 0
// 是外弧（最长），先亮的是最长弧，与诉求方向相反。改为 (2−i)×arcFade
// 起显（最短弧先亮），熄灭相位 i×arcFade（最长弧先灭）；周期保持
// 6×arcFade+arcHold+arcGap（默认 1920ms，落在 replayWindowMs=2400 内）。
//
// 5WHY (复核 2026-08-22 用户三次诉求 "弧线用不同的高亮颜色呈现并不停
// 轮换" + "是否可以通过不断的重绘 SVG 来实现"): 曾以 QML Rectangle
// 虚线片 + 三角函数推导近似母版弧线——几何是"算出来的"而非"母版的"，
// 且颜色轮换落在短暂点亮窗口内难以察觉。按用户方案改为 SVG 复绘：
// 弧线路径与 ffffff 母版 SVG 逐字一致（AnimationTokens.js
// wifiWaveArcSets，几何=母版事实、零漂移），每次点亮前重新生成
// data-URI SVG 并注入下一档高亮色（"重绘时对 WiFi 弧线做不同色彩
// 设定"）——每次出现的弧线都是新颜色；三弧起色按 index 错位，任一
// 时刻三弧各持不同高亮色。纯 Image + data-URI + 属性动画（无 Canvas/
// ShapePath/ShaderEffect——iOS 静态 Qt 安全；QSvgPlugin 已随管线 v4
// 显式导入）。
//
// Usage: WifiWaveAnimation { anchors.fill: parent; running: testRunning }

AnimationBase {
    id: root
    property int arcFade: Tokens.tokens.wifiWaveFade
    property int arcHold: Tokens.tokens.wifiWaveHold
    property int arcGap:  Tokens.tokens.wifiWaveGap

    // 图标名（AnimationBase 统一声明，DiagAnimator 装载时绑定下发）→
    // 每图标弧线母版路径集。缺省回退 internet 右下弧组。
    readonly property var _arcSet: root.iconName !== ""
        ? (Tokens.tokens.wifiWaveArcSets[root.iconName] || null)
        : null
    readonly property var _arcs: root._arcSet || Tokens.tokens.wifiWaveArcSets["nd-diag-g3-internet"]

    // 四色高亮盘（primary/tertiary/warning/success）——双主题经
    // ThemeEngine 角色即时解析（_arcUri 绑定内读取，主题切换即重估）。
    readonly property var _palette: [T.ThemeEngine.colors.primary,
        T.ThemeEngine.colors.tertiary, T.ThemeEngine.colors.warning,
        T.ThemeEngine.colors.success]

    // QML color.toString() 可能是 #RRGGBB 或 #AARRGGBB——统一取后 6 位。
    // 5WHY (Reuse 2026-09-05): 收敛至 ThemeEngine.colorToHex（通道取整版，
    // 不依赖 toString 格式；与 AppIcon 请求色同一实现）。
    function _hexOf(c) {
        return T.ThemeEngine.colorToHex(c)
    }
    // 弧线 SVG 复绘：stroke 弧（wifi-info）与 fill 弧（internet，含母版
    // scale(0.06857) 变换）两种包络；每次调用重绘一张注入指定色的 SVG。
    function _arcUri(i, palIdx) {
        var a = root._arcs[i]
        var hex = root._hexOf(root._palette[palIdx % 4])
        if (a.stroke !== undefined)
            return "data:image/svg+xml;base64," + Qt.btoa(
                '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none" stroke-linecap="round" stroke-linejoin="round">'
                + '<path d="' + a.stroke + '" stroke="#' + hex + '" stroke-width="' + a.sw + '" fill="none"/>'
                + '</svg>')
        return "data:image/svg+xml;base64," + Qt.btoa(
            '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none" fill-opacity="0">'
            + '<g transform="scale(0.06857)">'
            + '<path d="' + a.fill + '" fill="#' + hex + '" fill-opacity="1"/>'
            + '</g></svg>')
    }

    Repeater {
        model: 3
        delegate: Item {
            id: arcItem
            width: parent.width
            height: parent.height
            // index = 外层 Repeater 序号（0=外弧/最长，2=内弧/最短）
            // 5WHY (复核 2026-08-22 用户方案 "重绘 SVG 时对弧线做不同
            // 色彩设定"): _palIdx 即调色盘相位——点亮前推进（见 seq 的
            // ScriptAction），source 绑定随相位/主题重估 → 新 data-URI
            // → 重绘母版弧线并注入新色。三弧起色按 index 错位：任一
            // 时刻三弧各持不同高亮色。
            property int _palIdx: index
            Image {
                anchors.fill: parent
                // 96px 固定源尺寸：≤56px 图标井内 SVG 放大渲染不糊
                sourceSize.width: 96
                sourceSize.height: 96
                smooth: true
                mipmap: true
                source: root._arcUri(index, arcItem._palIdx)
            }
            opacity: 0

            // 相位（5WHY 复核 2026-08-21 用户诉求 "最短弧先亮、最长弧先灭"）:
            // 起显相位 (2−index)×arcFade → 内弧（index 2）最先亮、外弧
            // （index 0）最后亮；熄灭相位 index×arcFade → 外弧最先灭、
            // 内弧最后灭。周期 = 6×arcFade + arcHold + arcGap（默认 1920ms，
            // 落在 replayWindowMs=2400 的有界重放窗口内，三弧同周期无缝循环）。
            // 5WHY (复核 2026-08-21 gap 语义): arcGap 曾并入保持段——全亮
            // 保持 600ms（hold+gap）而循环边界零休整（tokens 注释"全部熄灭
            // 后到下一轮的休整时长"落空）。gap 移至尾部休整段：全亮保持
            // 恰为 arcHold，全熄休整恰为 arcGap。
            SequentialAnimation {
                id: seq
                loops: Animation.Infinite
                PauseAnimation { duration: (2 - index) * root.arcFade }
                // 点亮前推进调色盘：同 index 弧每次出现皆换新色（重绘 SVG）
                ScriptAction { script: { arcItem._palIdx = (arcItem._palIdx + 1) % 4 } }
                NumberAnimation {
                    target: arcItem; property: "opacity"
                    from: 0; to: 1; duration: root.arcFade
                    easing.type: Easing.OutCubic
                }
                PauseAnimation { duration: root.arcHold + 2 * index * root.arcFade }
                NumberAnimation {
                    target: arcItem; property: "opacity"
                    from: 1; to: 0; duration: root.arcFade
                    easing.type: Easing.InCubic
                }
                PauseAnimation { duration: (2 - index) * root.arcFade + root.arcGap }
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
