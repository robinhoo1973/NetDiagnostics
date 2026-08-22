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
    barsCycleColorStep: 500, // 蜂窝信号柱高亮色轮换单步时长
    convergeTravel: 380,     // 网关箭头单程（聚拢/回退）时长
    convergeStagger: 120,    // 四箭头有序启动间隔
    convergeHold: 260,       // 全部聚拢到位后的保持时长
    geoRadarPeriod: 900,     // 定位雷达波单圈扩散时长（GeoLocateAnimation）
    geoRadarStagger: 300,    // 三圈雷达波有序启动间隔

    // ── v7 (2026-08-22): 用户诉求 "不断重绘 SVG，重绘时对 WiFi 弧线做
    // 不同色彩设定"——弧线几何单一来源改为母版 SVG 路径逐字复刻：动画层
    // 以 data-URI SVG 复绘母版弧线（路径 = ffffff 母版事实，零几何漂移），
    // 每轮点亮注入不同高亮色。键 = 母版图标名；缺省回退 internet 弧组。
    // internet：gstatic 徽章右下 #B00002 信号弧（350 系 4 路径；内档两路径
    // 合为一子路径）；wifi-info：Lucide wifi 三弧 stroke（24 系）。
    wifiWaveArcSets: {
        "nd-diag-g3-internet": [
            { fill: "M298,171c54.88,39.65,22.84,126.03-45.52,121.17c-11.23-.8-24.41-3.85-33.24-11.28c-38.44-32.38-37.14-90.67,9.02-114.86c10.35-5.42,21.98-8.78,33.7-8.9c13.67-.15,26.52,3.88,36.04,13.87Zm13,53c-4.72-71-103.73-65.3-104.4-1.06c-.29,28.85,20.28,54.47,50.33,55.09c30.69,.64,53.68-24.1,54.07-54.03Z" },
            { fill: "M299,216c-1.54,3.31-2.98,7.17-7.16,5.27c-4.86-2.2-8.71-6.28-13.98-8.43c-18.2-7.46-38.05-3.51-52.86,9.16c-3.36-1.34-4.65-2.64-6-6c22.11-21.58,57.58-20.6,80,0Z" },
            { fill: "M283,236c0-.66,0-1.32,0-2c-4.85-1.88-9.59-4.55-14.53-6.07c-12.71-3.92-25.19,.36-35.47,8.07c-1.65-1.32-3.3-2.64-5-4c2.36-5.74,7.75-8.49,13.2-11.02c14.44-5.6,29.62-3.84,42.8,4.02c1.71,1.28,3.39,2.59,5,4c-.56,3.24-1.41,4.93-4,7c-.66,0-1.32,0-2,0Z M280,243c-.78,2.75-1.32,5.38-4.53,5.91c-3.65,.59-5.74-2.2-8.83-3.57c-3.07-1.36-6.35-1.88-9.69-1.68c-5.12,.31-12.15,7.21-16.44,4.43c-1.13-.73-1.95-1.74-2.7-2.85c-.28-.41-.54-.82-.81-1.24c12.66-12.32,26.4-10.3,41-3c0,.66,0,1.32,0,2c.66,0,1.32,0,2,0Z" }
        ],
        "nd-diag-g1-wifi-info": [
            { stroke: "M2 8.82 a15 15 0 0 1 20 0", sw: 1.6 },
            { stroke: "M5 12.859 a10 10 0 0 1 14 0", sw: 1.2 },
            { stroke: "M8.5 16.429 a5 5 0 0 1 7 0", sw: 1.2 }
        ]
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
