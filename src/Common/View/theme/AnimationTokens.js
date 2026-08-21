// Living Diagnostics L4 animation timing tokens.
// Kept as a JS object (not QML properties) to avoid QML engine property
// bloat on iOS static Qt — 5WHY #3: ThemeEngine must not bloat.
// Easing strings removed — QML Easing.* enum values can't be resolved from JS strings.
.pragma library

var tokens = {
    // ── Busy-loop cycles (milliseconds) ──────────────────────────────────
    jigglePeriod: 100,       // half-cycle of ±2.5° oscillation
    bouncePeriod: 500,       // one-way travel time for bounce
    pathNodeDelay: 200,      // delay between hop-node appearances
    pulsePeriod: 800,        // full opacity pulse cycle
    typeCharDelay: 40,       // per-character delay for typing effect
    lockDropDuration: 400,   // stamp/lock fall duration

    // ── v5 (2026-08-18): 新动画类型令牌 ────────────────────────────────
    checkHideDuration: 220,  // 盾牌打勾：盖片渐入遮没勾的时长（勾消失）
    checkDrawDuration: 420,  // 盾牌打勾：盖片渐出时长（勾逐渐重现）
    checkHoldDuration: 420,  // 勾消失/重现后的保持时长
    wifiWaveFade: 220,       // 信号弧单条淡入/淡出时长（逐条明灭）
    wifiWaveHold: 400,       // 三道弧全部显现后的保持时长
    wifiWaveGap: 200,        // 全部熄灭后到下一轮的休整时长
    convergeTravel: 380,     // 网关箭头单程（聚拢/回退）时长
    convergeStagger: 120,    // 四箭头有序启动间隔
    convergeHold: 260,       // 全部聚拢到位后的保持时长
    geoRadarPeriod: 900,     // 定位雷达波单圈扩散时长（GeoLocateAnimation）
    geoRadarStagger: 300,    // 三圈雷达波有序启动间隔

    // ── v6 (2026-08-20): 动画锚点几何（母版 SVG 图形事实，归一化到
    // 图标框宽度）。曾双份存于 C++ AppState 硬编码 + 各动画 QML 默认值
    // ——同值双份靠注释"两处同改"维系，改一处即静默错位。收敛为本文件
    // 单一来源：QML 默认值直接读 tokens，母版再生成位移仅改此处一处。
    // ── GeoIP 定位针头中心 + 到最近边缘的半径（QSvgRenderer 逐通道
    // 实测 viewBox ≈(0.71, 0.30)；右缘距 0.29 为约束紧侧）──
    geoRadarAnchorCx: 0.71,
    geoRadarAnchorCy: 0.30,
    geoRadarAnchorMaxR: 0.29,
    // ── internet 母版右下三道信号弧（350 系 y≈216/236/243 弧组）
    // 焦点≈(296,244)、外弧半径≈52/350 → 归一化 (0.85, 0.70)、0.155 ──
    wifiWaveAnchorCx: 0.85,
    wifiWaveAnchorCy: 0.70,
    wifiWaveAnchorMaxR: 0.155,
    // 5WHY (复核 2026-08-20 几何双份残留): 弧角跨度与半径收缩系数同为母版
    // 图形事实——曾与焦点/半径分家留在动画 QML 字面量（"改一处"承诺
    // 落空）。全部收敛于此，母版再生成时只改本文件。
    wifiWaveArcA0: -78,              // 起角（右上，度）
    wifiWaveArcA1: -168,             // 终角（左侧，度）
    wifiWaveArcRadii: [1.0, 0.74, 0.48],   // 外→内三道弧半径收缩系数
    // 5WHY (复核 2026-08-21 用户诉求 "WiFi 信息动画同款弧线"): WiFi 信息
    // 图标（nd-diag-g1-wifi-info，Lucide wifi）的三弧几何与 internet 右下
    // 弧组不同——同心弧，圆心=(12,20)（dot 即圆心）、半径 15/10/5
    // （24 viewBox 归一化）。锚点曾只有 internet 一套，WiFi 信息无法复用
    // WifiWave。键 = 母版图标名；缺省回退上方 internet 平坦令牌。
    // 起/终角按 QML 位置式 (cos,sin) 换算（y 向下）：外弧端点
    // (2,8.82)/(22,8.82) → -131.81°/-48.19°；中/内弧同角跨 -134.43°/
    // -45.57°。片厚占比 = 母版线宽 1.6/1.2/1.2 ÷ 24。
    wifiWaveAnchorSets: {
        "nd-diag-g1-wifi-info": {
            cx: 0.5,
            cy: 0.8333,
            maxR: 0.625,
            a0: [-131.81, -134.43, -134.43],   // 外→内起角（左端，度）
            a1: [-48.19, -45.57, -45.57],      // 外→内终角（右端，度）
            radii: [1.0, 0.6667, 0.3333],      // 15/10/5 半径收缩系数
            dashTh: [0.0667, 0.05, 0.05],      // 母版线宽 1.6/1.2/1.2 → 宽度占比
            // 5WHY (复核 2026-08-21 徽章过绘): 母版徽章 circle-i（(17.2,7.6)
            // r2.9+线1.3/2 ≈ 3.55/24）叠于弧线上——动画层在图标层之上，
            // 外弧在 θ∈[-80°,-60°] 段穿越徽章圆盘（距徽章心 1.55~3.52）、
            // 中弧贴边（3.45），灯亮相位弧线片过绘徽章高亮。排除盘：
            // 片中心落入盘内（含片厚/余量 ≈0.19）即隐藏——保持母版
            // "徽章压弧线"合成次序。
            exclude: { cx: 0.7167, cy: 0.3167, r: 0.19 }
        }
    },

    // 5WHY (复核 2026-08-19 单一来源): 详情页有界回放窗口曾是 DiagAnimator
    // 硬编码 2400——与各循环周期的手算关系写在注释里（wifiWave 1920、
    // geoRadar 900+2×300=1500、jiggle 540）。窗口与周期同为动画时序事实，收敛进
    // tokens 同源维护；新增动画周期超过窗口时由 250ms 淡出兜底截断。
    replayWindowMs: 2400,

    // ── Settle / transition (used by DiagBlock.qml Behavior) ─────────────
    settleDuration: 300,     // Done settle pop (OutBack)
    transitionDuration: 200, // color / border transitions
};
