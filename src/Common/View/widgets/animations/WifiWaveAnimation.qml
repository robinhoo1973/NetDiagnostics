import QtQuick
import "../../theme" as T
import "../../theme/AnimationTokens.js" as Tokens

// ── WifiWaveAnimation.qml — WiFi 信号弧逐条亮起再逐条熄灭 ──────────────
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
// 5WHY (复核 2026-08-21 用户诉求 "弧线用不同的高亮颜色呈现"): 曾三弧同色
// （root.accentColor）——外→内三弧各取主题强调色（primary/tertiary/
// warning），双主题经 ThemeEngine 角色解析即时切换（见 delegate _arcColor）。
//
// 锚点几何单一来源 = AnimationTokens.js（每图标锚点集 wifiWaveAnchorSets
// 键控于母版图标名；internet 默认平坦令牌为回退）。DiagAnimator 装载时
// 下发 iconName（与 running/targetItem 同层绑定）。纯 Rectangle 虚线弧
// （无 Canvas/ShapePath/ShaderEffect——iOS 静态 Qt 安全）。
//
// Usage: WifiWaveAnimation { anchors.fill: parent; running: testRunning }

AnimationBase {
    id: root
    property int arcFade: Tokens.tokens.wifiWaveFade
    property int arcHold: Tokens.tokens.wifiWaveHold
    property int arcGap:  Tokens.tokens.wifiWaveGap
    property int colorStep: Tokens.tokens.wifiWaveColorStep

    // 图标名（AnimationBase 统一声明，DiagAnimator 装载时绑定下发）→
    // 每图标锚点集。WiFi 信息图标（nd-diag-g1-wifi-info）的三弧几何
    // （同心弧，焦点=(12,20)/24）与 internet 右下角弧组不同——曾只有
    // 一套锚点，第二消费方无法复用。
    readonly property var _anchorSet: root.iconName !== ""
        ? (Tokens.tokens.wifiWaveAnchorSets[root.iconName] || null)
        : null
    // 5WHY (复核 2026-08-21 归一化锚点集): 曾 delegate 内四份三元重复测
    // _anchorSet——root 层解析为恒有定义的数组（缺省 = internet 平坦令牌
    // 逐弧广播），delegate 直接下标，无逐弧守卫重复。
    readonly property var _a0s: root._anchorSet && root._anchorSet.a0
        ? root._anchorSet.a0
        : [Tokens.tokens.wifiWaveArcA0, Tokens.tokens.wifiWaveArcA0, Tokens.tokens.wifiWaveArcA0]
    readonly property var _a1s: root._anchorSet && root._anchorSet.a1
        ? root._anchorSet.a1
        : [Tokens.tokens.wifiWaveArcA1, Tokens.tokens.wifiWaveArcA1, Tokens.tokens.wifiWaveArcA1]
    readonly property var _radii: root._anchorSet && root._anchorSet.radii
        ? root._anchorSet.radii : Tokens.tokens.wifiWaveArcRadii
    // 片厚占比（缺省 0.025 与旧默认分支同值；delegate 内统一加 0.9px 地板）
    readonly property var _thW: root._anchorSet && root._anchorSet.dashTh
        ? root._anchorSet.dashTh : [0.025, 0.025, 0.025]
    // 徽章排除盘（5WHY 复核 2026-08-21 徽章过绘，见 tokens 注释）：动画层
    // 高于图标层，wifi-info 徽章 circle-i 被外弧灯亮相位过绘——片中心落入
    // 排除盘即隐藏，保持母版"徽章压弧线"合成次序。
    readonly property var _excl: root._anchorSet && root._anchorSet.exclude
        ? root._anchorSet.exclude : null
    function excludedAt(px, py) {
        if (!root._excl) return false
        const dx = px - root._excl.cx * root.width
        const dy = py - root._excl.cy * root.height
        const r = root._excl.r * root.width
        return dx * dx + dy * dy <= r * r
    }
    property real anchorCx: root._anchorSet ? root._anchorSet.cx : Tokens.tokens.wifiWaveAnchorCx
    property real anchorCy: root._anchorSet ? root._anchorSet.cy : Tokens.tokens.wifiWaveAnchorCy
    property real anchorMaxR: root._anchorSet ? root._anchorSet.maxR : Tokens.tokens.wifiWaveAnchorMaxR

    // 弧组几何（与母版三道信号弧同焦点、同角跨度）——角跨度与半径
    // 系数同读 AnimationTokens.js（几何事实单一来源，见 tokens v6）
    readonly property real _fx: parent.width * root.anchorCx
    readonly property real _fy: parent.height * root.anchorCy
    // 5WHY (复核 2026-08-20 虚线不可见): 片段数/片长曾为固定 9 片 ×
    // 0.045w——默认 40px 图标上外弧弧长仅 ≈9.7px，9 片 1.8px 互相重叠
    // 且片长（1.8px）短于片厚（2.4px），"虚线弧"实际渲染成实心圆角带
    // （设计意图逐条明灭的虚线不可见）。改为按真实弧长推导：片数 =
    // floor(弧长/(3×片厚))（钳 [1,9]，片长恒 ≥1.5×片厚；片数 1 = 整弧
    // 实心，片宽取弦长 2r·sin(Δθ/2) 而非弧长——弧长直条径向越弧 27%），
    // 片长 = 弧长/片数×0.5（占空比 ≈50%）——业界惯例：虚线段长不短于
    // 线宽，否则视觉上为点而非线。细节见 delegate 注释。

    Repeater {
        model: 3
        delegate: Item {
            id: arcItem
            // index = 外层 Repeater 序号（0=外弧/最长，2=内弧/最短）
            property real _a0: root._a0s[index]
            property real _a1: root._a1s[index]
            property real _r: root._radii[index]
            // 片厚：母版线宽事实（wifi-info 1.6/1.2 线宽占比；internet
            // 默认 0.025w）。0.9px 地板与旧默认分支一致，且防
            // parent.width==0 时 0 除 → NaN 段计数（装载首帧瞬态）。
            property real _dashTh: Math.max(0.9, parent.width * root._thW[index])
            // 外→内三道弧半径（相对 anchorMaxR 收缩）
            property real radius: parent.width * root.anchorMaxR * arcItem._r
            // 5WHY (复核 2026-08-20 负弧长): (_a1 - _a0) 恒负——
            // 曾直接相乘致 arcLen<0 → segCount 钳死在最小值、dashLen<0 →
            // Rectangle 负宽不渲染（虚线弧全平台不可见，正是本 commit
            // 声称修复的缺陷）。取绝对值（角跨度与方向无关）。
            property real arcLen: Math.abs(arcItem._a1 - arcItem._a0) * Math.PI / 180 * radius
            // 5WHY (复核 2026-08-21 点非线): 曾 segCount 下限钳 3——内弧
            // arcLen/th≈4.7 时 3 片 → 片长 0.78×片厚，"片长 ≥ 片厚"的
            // 虚线不变式（本文件自声明的修复目标）在 3 弧中 2 弧不可达，
            // 渲染成点而非线。floor(arcLen/(3×th))（下限 1）：片长恒
            // ≥1.5×片厚；片数 1 时整弧实心。
            property int segCount: Math.max(1, Math.min(9, Math.floor(arcLen / (3 * arcItem._dashTh))))
            // 5WHY (复核 2026-08-21 越弧直条): 曾 segCount==1 时片宽取弧长
            // ——弧长直条两端径向越出圆弧 27%（internet 内弧恒单段），
            // "实心与母版一致"落空。单段宽 = 弦长 2r·sin(Δθ/2)：切线
            // 摆放于中点角时两端恰落弧端点，与母版弧线重合。
            property real segW: arcItem.segCount <= 1
                ? 2 * arcItem.radius * Math.sin(Math.abs(arcItem._a1 - arcItem._a0) * Math.PI / 360)
                : arcLen / segCount * 0.5
            // 5WHY (复核 2026-08-22 用户诉求 "弧线用不同的高亮颜色呈现，
            // 并不停的变化"): 上轮实现只做到静态分色（三弧各固定一色，
            // 仅透明度明灭）——"颜色不断变化"的时间轮换维度缺失。改为
            // 每弧沿 4 色高亮盘（primary/tertiary/warning/success）连续
            // 轮换，步长 colorStep=500ms；三弧起色按 index 错位——任一
            // 时刻三弧各持不同颜色且各自流动（业界"分色流光"惯例）。
            // ColorAnimation 序列与明灭序列独立循环（颜色流动不因弧
            // 熄灭暂停）；RestartController 重启时 from/to 重读主题色。
            readonly property var _palette: [T.ThemeEngine.colors.primary,
                T.ThemeEngine.colors.tertiary, T.ThemeEngine.colors.warning,
                T.ThemeEngine.colors.success]
            property color _arcColor: arcItem._palette[index % 4]
            opacity: 0
            SequentialAnimation on _arcColor {
                id: colorSeq
                loops: Animation.Infinite
                ColorAnimation { from: arcItem._palette[(index + 0) % 4]; to: arcItem._palette[(index + 1) % 4]; duration: root.colorStep }
                ColorAnimation { from: arcItem._palette[(index + 1) % 4]; to: arcItem._palette[(index + 2) % 4]; duration: root.colorStep }
                ColorAnimation { from: arcItem._palette[(index + 2) % 4]; to: arcItem._palette[(index + 3) % 4]; duration: root.colorStep }
                ColorAnimation { from: arcItem._palette[(index + 3) % 4]; to: arcItem._palette[(index + 0) % 4]; duration: root.colorStep }
            }
            RestartController {
                running: root.running
                target: colorSeq
                onStopped: arcItem._arcColor = arcItem._palette[index % 4]
            }

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
                    // 5WHY (复核 2026-08-21 NaN 除零): segCount=1（内弧
                    // 实心整弧）时 index/(segCount-1) = 0/0 = NaN——单段
                    // 位置/旋转全 NaN 不可渲染。单段取中点角，整弧直线
                    // 近似圆心角平分线（微小弧上直线≈弧，且居中摆放）。
                    property real ang: arcItem.segCount <= 1
                        ? (arcItem._a0 + arcItem._a1) / 2
                        : arcItem._a0 + (arcItem._a1 - arcItem._a0) * (index / (arcItem.segCount - 1))
                    width: arcItem.segW
                    height: arcItem._dashTh
                    radius: height / 2
                    color: arcItem._arcColor
                    visible: !root.excludedAt(x + width / 2, y + height / 2)
                    x: root._fx + arcItem.radius * Math.cos(ang * Math.PI / 180) - width / 2
                    y: root._fy + arcItem.radius * Math.sin(ang * Math.PI / 180) - height / 2
                    rotation: ang + 90
                }
            }

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
