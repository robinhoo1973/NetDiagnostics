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
    meterSweepDuration: 600, // 表针从左到右单次扫掠时长
    convergeTravel: 380,     // 网关箭头单程（聚拢/回退）时长
    convergeStagger: 120,    // 四箭头有序启动间隔
    convergeHold: 260,       // 全部聚拢到位后的保持时长
    geoRadarPeriod: 900,     // 定位雷达波单圈扩散时长（GeoLocateAnimation）
    geoRadarStagger: 300,    // 三圈雷达波有序启动间隔

    // 5WHY (复核 2026-08-19 单一来源): 详情页有界回放窗口曾是 DiagAnimator
    // 硬编码 2400——与各循环周期的手算关系写在注释里（meter 600、geoRadar
    // 900+2×300=1500、jiggle 540）。窗口与周期同为动画时序事实，收敛进
    // tokens 同源维护；新增动画周期超过窗口时由 250ms 淡出兜底截断。
    replayWindowMs: 2400,

    // ── Settle / transition (used by DiagBlock.qml Behavior) ─────────────
    settleDuration: 300,     // Done settle pop (OutBack)
    transitionDuration: 200, // color / border transitions
};
