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

    // ── v8 (2026-08-23): G4 远端主机三动画令牌 ──────────────────────────
    // 5WHY (设计文档 review/refactor/ui/anim/04-g4-route-animations.md):
    // Ping/Traceroute/PathPing 曾共用通用 Bounce/Path——与新母版
    // （Lucide radar/route/waypoints）无语义锚点，观者无法建立"探测中"
    // 联想。三动画几何锚定各自母版（归一化常量在动画 QML 本地，v7 教义：
    // 几何事实不进 tokens）；本表只收时序。
    sonarSweepMs: 1100,      // 声呐波束整圈扫描时长（SonarSweepAnimation）
    routeTravelMs: 1400,     // 探测包沿 route S 形单程时长（RouteTraceAnimation）
    routeArrivalMs: 380,     // 终点命中脉冲扩散时长
    routeHoldMs: 420,        // 命中后到下一轮的休整时长
    routeFadeMs: 150,        // 探测包终点淡出（防止环首瞬移回起点）
    hopSampleStagger: 280,   // waypoints 四节点逐跳采样启动间隔（HopSampleAnimation）
    hopSamplePulseMs: 320,   // 单节点采样环扩散时长
    hopSampleHoldMs: 480,    // 末节点采样后休整时长

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
    // 蜂窝四信号柱弧（BarsCycleAnimation SVG 复绘，与 WifiWave 同机制）：
    // 路径 = ffffff 母版 nd-diag-g1-cellular.svg 逐字事实（24 系 stroke）。
    barsCycleSets: {
        "nd-diag-g1-cellular": [
            { stroke: "M6.4 18 V15", sw: 1.6 },
            { stroke: "M10.1 18 V13", sw: 1.6 },
            { stroke: "M13.8 18 V11", sw: 1.6 },
            { stroke: "M17.5 18 V9", sw: 1.6 }
        ]
    },

    // 5WHY (复核 2026-08-19 单一来源): 详情页有界回放窗口曾是 DiagAnimator
    // 硬编码 2400——与各循环周期的手算关系写在注释里（wifiWave 1920、
    // geoRadar 900+2×300=1500、jiggle 540）。窗口与周期同为动画时序事实，收敛进
    // tokens 同源维护；新增动画周期超过窗口时由 250ms 淡出兜底截断。

    // ── v9 (2026-08-23): 文字打字/闪烁 + HTTP 内容闪烁动画集 ──────────
    // 5WHY (用户裁定): IP Configuration 打字机逐字点亮「1.1.1.1」、
    // DNS Servers 闪烁「DNS」三字符、HTTP 三图标（curl/http-headers/
    // security-headers）仅闪烁文件图形内部（HTML 字符保持静止）。
    // 字形/内容 = 母版 SVG 逐字事实（同 wifiWaveArcSets 单一来源约定），
    // 运行时经 data-URI SVG 复绘着色。
    typeTextStepMs: 240,     // 打字单字符步进（TypeTextAnimation）
    typeTextHoldMs: 520,     // 全部出现后保持
    typeTextClearMs: 200,    // 清屏休整（ip-config 周期=7×240+520+200=2400）
    blinkTextOnMs: 420,      // 「DNS」亮相时长（BlinkTextAnimation）
    blinkTextOffMs: 380,     // 熄灭时长
    flashOnMs: 460,          // 内容亮相时长（FlashContentAnimation）
    flashOffMs: 340,         // 熄灭时长（HTML 文本不参与，恒显）
    glyphTypeSets: {
        "nd-diag-g1-ip-config": { tf: "", sw: 0.8, chars: [
            { d: "M 6.31 8.99 L 6.66 8.81 L 7.17 8.30 L 7.17 11.90" },
            { d: "M 9.02 11.56 L 8.85 11.73 L 9.02 11.90 L 9.19 11.73 L 9.02 11.56" },
            { d: "M 9.83 8.99 L 10.18 8.81 L 10.69 8.30 L 10.69 11.90" },
            { d: "M 12.54 11.56 L 12.37 11.73 L 12.54 11.90 L 12.71 11.73 L 12.54 11.56" },
            { d: "M 13.35 8.99 L 13.70 8.81 L 14.21 8.30 L 14.21 11.90" },
            { d: "M 16.06 11.56 L 15.89 11.73 L 16.06 11.90 L 16.23 11.73 L 16.06 11.56" },
            { d: "M 16.87 8.99 L 17.22 8.81 L 17.73 8.30 L 17.73 11.90" },
        ] },
    },
    glyphBlinkSets: {
        "nd-diag-g3-dns-servers": { tf: "", sw: 0.6, letters: [
            { d: "M 10.30 10.00 L 10.30 13.00 M 10.30 10.00 L 11.10 10.00 L 11.44 10.14 L 11.67 10.43 L 11.79 10.71 L 11.90 11.14 L 11.90 11.86 L 11.79 12.29 L 11.67 12.57 L 11.44 12.86 L 11.10 13.00 L 10.30 13.00" },
            { d: "M 12.74 10.00 L 12.74 13.00 M 12.74 10.00 L 14.34 13.00 M 14.34 10.00 L 14.34 13.00" },
            { d: "M 16.78 10.43 L 16.55 10.14 L 16.21 10.00 L 15.75 10.00 L 15.41 10.14 L 15.18 10.43 L 15.18 10.71 L 15.29 11.00 L 15.41 11.14 L 15.64 11.29 L 16.32 11.57 L 16.55 11.71 L 16.67 11.86 L 16.78 12.14 L 16.78 12.57 L 16.55 12.86 L 16.21 13.00 L 15.75 13.00 L 15.41 12.86 L 15.18 12.57" },
        ] },
    },
    flashContentSets: {
        "nd-diag-g5-curl-verbose": { tf: "translate(5.4 3.16) scale(0.55)", body: "<!-- Browser requests page, server responds --> <rect x=\"3.4\" y=\"7\" width=\"6\" height=\"10\" rx=\"1.4\" stroke=\"url(#ngnddiagg5curlverbose)\" stroke-width=\"1.6\" /> <path d=\"M3.4 9.6 H9.4\" stroke=\"#AAAAAA\" stroke-width=\"1.2\" /> <circle cx=\"6.4\" cy=\"13\" r=\"0.9\" fill=\"#000000\" fill-opacity=\"1\" /> <rect x=\"14.6\" y=\"7\" width=\"6\" height=\"10\" rx=\"1.4\" stroke=\"url(#ngnddiagg5curlverbose)\" stroke-width=\"1.6\" /> <circle cx=\"17\" cy=\"10.4\" r=\"0.7\" fill=\"#777777\" fill-opacity=\"1\" /> <circle cx=\"17\" cy=\"13.2\" r=\"0.7\" fill=\"#777777\" fill-opacity=\"1\" /> <path d=\"M9.6 9.4 H14.4 M14.4 9.4 l-2.2 -1.4 M14.4 9.4 l-2.2 1.4\" stroke=\"#000000\" stroke-width=\"1.4\" /> <path d=\"M14.4 14.6 H9.6 M9.6 14.6 l2.2 -1.4 M9.6 14.6 l2.2 1.4\" stroke=\"#AAAAAA\" stroke-width=\"1.2\" />" },
        "nd-diag-g5-http-headers": { tf: "translate(4.86 1.89) scale(0.6)", body: "<path d=\"M5.4 5.8 V18.2 M9.6 5.8 V18.2 M5.4 12 H9.6\" stroke=\"url(#ngnddiagg5httpheaders)\" stroke-width=\"1.6\" /> <path d=\"M12.6 9.8 H18.4 M12.6 13.2 H18.4 M12.6 16.6 H16\" stroke=\"#000000\" stroke-width=\"1.3\" />" },
        "nd-diag-g5-security-headers": { tf: "translate(6.0 3.4) scale(0.5)", body: "<path d=\"M12 3.4 L18.6 6 V12.4 C18.6 16.6 15.7 19.6 12 21.4 C8.3 19.6 5.4 16.6 5.4 12.4 V6 Z\" stroke=\"url(#ngnddiagg5securityheaders)\" stroke-width=\"1.6\" /> <path d=\"M8.6 10.6 H15.4 M8.6 13.2 H15.4 M8.6 15.8 H12.6\" stroke=\"#000000\" stroke-width=\"1.3\" />" },
    },


    // ── v10 (2026-08-23): 终端三协议打字动画 ─────────────────────────────
    // 5WHY (用户裁定): SSH/FTP/TELNET 的动画 = 命令行光标（下划线）不停
    // 闪烁，每闪烁两次以打字机方式打印出协议名的一个字符，直到全部打印。
    // 字形 = Hershey Roman Simplex 真字体（gen-hershey-text 同源解析），
    // 起笔 x10.6（静态 _ 之后）、基线 y12.1、字帽 2.0；cx = 该字符落笔后
    // 光标左缘。光标条 1.4×0.9u 圆角，primary 色。
    termBlinkHalfMs: 220,    // 光标半周期（亮/灭各一拍，两拍=一字）
    termHoldMs: 1000,        // 全词打印完的保持时长
    termClearMs: 120,        // 清屏休整
    termTypeSets: {
        "nd-diag-g5-ftp": { chars: [
            { d: "M 10.98 10.10 L 10.98 12.10 M 10.98 10.10 L 12.22 10.10 M 10.98 11.05 L 11.74 11.05", cx: 12.56 },
            { d: "M 13.33 10.10 L 13.33 12.10 M 12.66 10.10 L 13.99 10.10", cx: 14.34 },
            { d: "M 14.72 10.10 L 14.72 12.10 M 14.72 10.10 L 15.58 10.10 L 15.86 10.20 L 15.96 10.29 L 16.05 10.48 L 16.05 10.77 L 15.96 10.96 L 15.86 11.05 L 15.58 11.15 L 14.72 11.15", cx: 16.59 },
        ] },
        "nd-diag-g5-ssh": { chars: [
            { d: "M 12.22 10.39 L 12.03 10.20 L 11.74 10.10 L 11.36 10.10 L 11.08 10.20 L 10.89 10.39 L 10.89 10.58 L 10.98 10.77 L 11.08 10.86 L 11.27 10.96 L 11.84 11.15 L 12.03 11.24 L 12.12 11.34 L 12.22 11.53 L 12.22 11.81 L 12.03 12.00 L 11.74 12.10 L 11.36 12.10 L 11.08 12.00 L 10.89 11.81", cx: 12.75 },
            { d: "M 14.37 10.39 L 14.18 10.20 L 13.90 10.10 L 13.52 10.10 L 13.23 10.20 L 13.04 10.39 L 13.04 10.58 L 13.14 10.77 L 13.23 10.86 L 13.42 10.96 L 13.99 11.15 L 14.18 11.24 L 14.28 11.34 L 14.37 11.53 L 14.37 11.81 L 14.18 12.00 L 13.90 12.10 L 13.52 12.10 L 13.23 12.00 L 13.04 11.81", cx: 14.91 },
            { d: "M 15.29 10.10 L 15.29 12.10 M 16.62 10.10 L 16.62 12.10 M 15.29 11.05 L 16.62 11.05", cx: 17.25 },
        ] },
        "nd-diag-g5-telnet": { chars: [
            { d: "M 11.36 10.10 L 11.36 12.10 M 10.70 10.10 L 12.03 10.10", cx: 12.37 },
            { d: "M 12.75 10.10 L 12.75 12.10 M 12.75 10.10 L 13.99 10.10 M 12.75 11.05 L 13.52 11.05 M 12.75 12.10 L 13.99 12.10", cx: 14.43 },
            { d: "M 14.81 10.10 L 14.81 12.10 M 14.81 12.10 L 15.96 12.10", cx: 16.3 },
            { d: "M 16.68 10.10 L 16.68 12.10 M 16.68 10.10 L 18.02 12.10 M 18.02 10.10 L 18.02 12.10", cx: 18.65 },
            { d: "M 19.03 10.10 L 19.03 12.10 M 19.03 10.10 L 20.27 10.10 M 19.03 11.05 L 19.79 11.05 M 19.03 12.10 L 20.27 12.10", cx: 20.71 },
            { d: "M 21.47 10.10 L 21.47 12.10 M 20.80 10.10 L 22.14 10.10", cx: 22.48 },
        ] },
    },
    replayWindowMs: 2400,

    // ── Settle / transition (used by DiagBlock.qml Behavior) ─────────────
    settleDuration: 300,     // Done settle pop (OutBack)
    transitionDuration: 200, // color / border transitions
};
