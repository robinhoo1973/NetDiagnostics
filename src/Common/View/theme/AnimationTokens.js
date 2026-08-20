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
    checkDrawDuration: 420,  // 盾牌打勾：两笔显现总时长（一笔 45% / 二笔 55%）
    checkHoldDuration: 420,  // 勾完全显现后的保持时长
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
    // 单一来源：QML 默认值直接读 tokens，C++ 经 QQmlJSEngine 解析同文件
    // （AppState::diagAnimationAnchor），母版再生成位移仅改此处一处。
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

    // 5WHY (复核 2026-08-19 单一来源): 详情页有界回放窗口曾是 DiagAnimator
    // 硬编码 2400——与各循环周期的手算关系写在注释里（wifiWave 1920、
    // geoRadar 900+2×300=1500、jiggle 540）。窗口与周期同为动画时序事实，收敛进
    // tokens 同源维护；新增动画周期超过窗口时由 250ms 淡出兜底截断。
    replayWindowMs: 2400,

    // ── Settle / transition (used by DiagBlock.qml Behavior) ─────────────
    settleDuration: 300,     // Done settle pop (OutBack)
    transitionDuration: 200, // color / border transitions
};
