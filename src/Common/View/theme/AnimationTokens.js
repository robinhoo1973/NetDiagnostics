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
        ],
        "nd-diag-g1-nic-advanced": [
            { stroke: "M9 9 H15 V15 H9 Z", sw: 1.2 },
            { stroke: "M15 2v2 M9 2v2", sw: 1.2 },
            { stroke: "M15 20v2 M9 20v2", sw: 1.2 },
            { stroke: "M2 15h2 M2 9h2", sw: 1.2 },
            { stroke: "M20 15h2 M20 9h2", sw: 1.2 }
        ],
        "nd-diag-g1-wired": [
            { stroke: "M6 8v1", sw: 1.4 },
            { stroke: "M10 8v1", sw: 1.4 },
            { stroke: "M14 8v1", sw: 1.4 },
            { stroke: "M18 8v1", sw: 1.4 }
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
        "nd-diag-g5-ssl-certificate": { sw: 0.6, letters: [
            { d: "M 10.05 10.99 L 9.84 10.73 L 9.55 10.60 L 9.17 10.60 L 8.88 10.73 L 8.68 10.99 L 8.68 11.24 L 8.78 11.50 L 8.88 11.63 L 9.07 11.76 L 9.65 12.01 L 9.84 12.14 L 9.94 12.27 L 10.05 12.53 L 10.05 12.91 L 9.84 13.17 L 9.55 13.30 L 9.17 13.30 L 8.88 13.17 L 8.68 12.91" },
            { d: "M 12.26 10.99 L 12.06 10.73 L 11.77 10.60 L 11.39 10.60 L 11.10 10.73 L 10.91 10.99 L 10.91 11.24 L 11.00 11.50 L 11.10 11.63 L 11.30 11.76 L 11.88 12.01 L 12.06 12.14 L 12.16 12.27 L 12.26 12.53 L 12.26 12.91 L 12.06 13.17 L 11.77 13.30 L 11.39 13.30 L 11.10 13.17 L 10.91 12.91" },
            { d: "M 13.22 10.60 L 13.22 13.30 M 13.22 13.30 L 14.38 13.30" },
        ] },
        "nd-diag-g2-arp-table": { sw: 1.6, flashRole: "success", letters: [{ d: "M7.2 14.8 H16.8 M7.2 14.8 L8.9 13.1 M7.2 14.8 L8.9 16.5 M16.8 14.8 L15.1 13.1 M16.8 14.8 L15.1 16.5" } ] },
        "nd-diag-g3-dns-cache": { sw: 1.3, flashRole: "warning", letters: [{ d: "M17.8 13.2 A3.6 3.6 0 1 0 21.4 16.8 M17.8 14.9 L17.8 16.8 L19.3 17.8" } ] },
        "nd-diag-g4-dns-resolution": { sw: 0.6, flashRole: "surfaceContainerLow", letters: [ {"d": "M 14.70 1.85 L 14.70 4.92 M 14.70 1.85 L 15.52 1.85 L 15.87 2.00 L 16.10 2.29 L 16.22 2.58 L 16.34 3.02 L 16.34 3.75 L 16.22 4.19 L 16.10 4.48 L 15.87 4.77 L 15.52 4.92 L 14.70 4.92"}, {"d": "M 17.18 1.85 L 17.18 4.92 M 17.18 1.85 L 18.82 4.92 M 18.82 1.85 L 18.82 4.92"}, {"d": "M 21.30 2.29 L 21.07 2.00 L 20.72 1.85 L 20.25 1.85 L 19.90 2.00 L 19.67 2.29 L 19.67 2.58 L 19.78 2.87 L 19.90 3.02 L 20.14 3.17 L 20.84 3.46 L 21.07 3.60 L 21.19 3.75 L 21.30 4.04 L 21.30 4.48 L 21.07 4.77 L 20.72 4.92 L 20.25 4.92 L 19.90 4.77 L 19.67 4.48"} ] },
        "nd-diag-g4-ipv6": { sw: 0.85, flashRole: "success", letters: [ {"d": "M 6.12 8.60 L 6.12 12.20 M 8.04 8.60 L 8.04 12.20 M 8.04 8.60 L 9.58 8.60 L 10.09 8.77 L 10.27 8.94 L 10.44 9.29 L 10.44 9.80 L 10.27 10.14 L 10.09 10.31 L 9.58 10.49 L 8.04 10.49 M 11.67 8.60 L 13.04 12.20 M 14.42 8.60 L 13.04 12.20 M 17.88 9.11 L 17.71 8.77 L 17.19 8.60 L 16.85 8.60 L 16.34 8.77 L 15.99 9.29 L 15.82 10.14 L 15.82 11.00 L 15.99 11.69 L 16.34 12.03 L 16.85 12.20 L 17.02 12.20 L 17.54 12.03 L 17.88 11.69 L 18.05 11.17 L 18.05 11.00 L 17.88 10.49 L 17.54 10.14 L 17.02 9.97 L 16.85 9.97 L 16.34 10.14 L 15.99 10.49 L 15.82 11.00"} ] },
    },
    flashContentSets: {
        "nd-diag-g5-curl-verbose": { tf: "translate(5.4 3.16) scale(0.55)", body: "<!-- Browser requests page, server responds --> <rect x=\"3.4\" y=\"7\" width=\"6\" height=\"10\" rx=\"1.4\" stroke=\"url(#ngnddiagg5curlverbose)\" stroke-width=\"1.6\" /> <path d=\"M3.4 9.6 H9.4\" stroke=\"#AAAAAA\" stroke-width=\"1.2\" /> <circle cx=\"6.4\" cy=\"13\" r=\"0.9\" fill=\"#000000\" fill-opacity=\"1\" /> <rect x=\"14.6\" y=\"7\" width=\"6\" height=\"10\" rx=\"1.4\" stroke=\"url(#ngnddiagg5curlverbose)\" stroke-width=\"1.6\" /> <circle cx=\"17\" cy=\"10.4\" r=\"0.7\" fill=\"#777777\" fill-opacity=\"1\" /> <circle cx=\"17\" cy=\"13.2\" r=\"0.7\" fill=\"#777777\" fill-opacity=\"1\" /> <path d=\"M9.6 9.4 H14.4 M14.4 9.4 l-2.2 -1.4 M14.4 9.4 l-2.2 1.4\" stroke=\"#000000\" stroke-width=\"1.4\" /> <path d=\"M14.4 14.6 H9.6 M9.6 14.6 l2.2 -1.4 M9.6 14.6 l2.2 1.4\" stroke=\"#AAAAAA\" stroke-width=\"1.2\" />" },
        "nd-diag-g5-http-headers": { tf: "translate(4.86 1.89) scale(0.6)", body: "<path d=\"M5.4 5.8 V18.2 M9.6 5.8 V18.2 M5.4 12 H9.6\" stroke=\"url(#ngnddiagg5httpheaders)\" stroke-width=\"1.6\" /> <path d=\"M12.6 9.8 H18.4 M12.6 13.2 H18.4 M12.6 16.6 H16\" stroke=\"#000000\" stroke-width=\"1.3\" />" },
        "nd-diag-g5-security-headers": { tf: "translate(6.0 3.4) scale(0.5)", body: "<path d=\"M12 3.4 L18.6 6 V12.4 C18.6 16.6 15.7 19.6 12 21.4 C8.3 19.6 5.4 16.6 5.4 12.4 V6 Z\" stroke=\"url(#ngnddiagg5securityheaders)\" stroke-width=\"1.6\" /> <path d=\"M8.6 10.6 H15.4 M8.6 13.2 H15.4 M8.6 15.8 H12.6\" stroke=\"#000000\" stroke-width=\"1.3\" />" },
        "nd-diag-g2-network-profile": { tf: "", body: "<!-- Blank text file with hub-and-spoke network inside --><circle cx=\"12\" cy=\"13.6\" r=\"1.1\" fill=\"#000000\" fill-opacity=\"1\" /> <circle cx=\"7.6\" cy=\"15.9\" r=\"0.9\" stroke=\"#AAAAAA\" stroke-width=\"1.2\" /> <circle cx=\"16.4\" cy=\"15.9\" r=\"0.9\" stroke=\"#AAAAAA\" stroke-width=\"1.2\" /> <circle cx=\"12\" cy=\"9.4\" r=\"0.9\" stroke=\"#AAAAAA\" stroke-width=\"1.2\" /> <path d=\"M10.9 12.7 L8.5 15 M13.1 12.7 L15.5 15 M12 12.5 V10.3\" stroke=\"#AAAAAA\" stroke-width=\"1.2\" />" },
        "nd-diag-g5-http-redirect": { tf: "translate(4.33 3.8) scale(-0.55 0.55) translate(-24 0)", body: "<!-- Start dot -> right -> corner -> up -> destination box --> <circle cx=\"5.6\" cy=\"16\" r=\"1.9\" stroke=\"__C__\" stroke-width=\"1.6\" /> <circle cx=\"5.6\" cy=\"16\" r=\"0.8\" fill=\"__C__\" fill-opacity=\"1\" /> <path d=\"M7.5 16 H14.8 V8.6\" stroke=\"__C__\" stroke-width=\"1.6\" /> <path d=\"M14.8 16 l-2.5 -1.5 M14.8 16 l-2.5 1.5 M14.8 8.6 l-1.4 2.6 M14.8 8.6 l1.4 2.6\" stroke=\"__C__\" stroke-width=\"1.2\" /> <rect x=\"13.2\" y=\"5\" width=\"3.2\" height=\"3.2\" stroke=\"__C__\" stroke-width=\"1.6\" />" },
        "nd-diag-g5-http-compression": { tf: "translate(4.08 4.36) scale(0.66)", body: "<!-- VTracer 矢量化 Noun Project 181349（菊花已替换为 HTML 字母，同体色） --> <g transform=\"scale(0.12)\"> <path d=\"M149,48c-3.02,.47-5.98,.7-9.04,.42c-7.14-.66-9.54-10.59-19.3-11.49c-18.08-1.64-41.64-.11-59.99,.88C52.05,38.28,34.09,42.04,26,38c-.06-3.53-1.38-16.86,1.06-19.14c1.39-1.31,99.84-3.33,106.12-1.68C138.97,18.69,143.81,25.34,147,30c.88-1.87,1.88-4.95,3.53-6.18c6.06-4.52,15.12-.87,14.47,7.18c2.96-.06,5.32-.34,8,1c0,5,0,10,0,15c-1.65,.33-3.3,.66-5,1c-.69,11.19-29.69,16.04-19,0Z\" fill=\"__C__\" fill-opacity=\"1\"/> <path d=\"M9,131c-.01-9.56-1.48-82.07,.45-85.09c.11-.13,.22-.25,.33-.38c2.1-1.78,68.07-.66,76.38-.67c13.54,0,27.35-.57,40.86,.01c.83,.04,2.57,.13,2.98,1.13c1.41,3.46,.11,14.47,.1,18.69c-.04,22.1-.07,44.21-.1,66.31c-40.33,0-80.67,0-121,0Zm114-6c0-24.33,0-48.67,0-73c-32.56-5.92-73.88-2.07-107,0c0,24.33,0,48.67,0,73c35.67,0,71.33,0,107,0Z\" fill=\"__C__\" fill-opacity=\"1\"/> <path d=\"M147,72c.01-1.81,.02-5.46,1-7c2.15-3.39,17.83-10.11,20-4c3.12,8.77-14.77,17.23-21,11Z\" fill=\"__C__\" fill-opacity=\"1\"/> <path d=\"M148,89c-.33-2.31-.66-4.62-1-7c3.68-2.36,7.39-4.99,11.32-6.91C174.02,67.41,169.62,94.41,148,89Z\" fill=\"__C__\" fill-opacity=\"1\"/> <path d=\"M168,98c-3.7,3.5-17.55,13.32-21,5c-3.61-8.71,23.22-21.89,21-5Z\" fill=\"__C__\" fill-opacity=\"1\"/> <path d=\"M166,106c7.1,10.66-13.25,19.91-18.28,13.65C141.03,111.32,160.1,104.87,166,106Z\" fill=\"__C__\" fill-opacity=\"1\"/> <path d=\"M147,145c-3.09,3.15-8.08,9.68-12.33,10.78c-6.02,1.56-106.47,.18-106.98-.2C24.38,153.16,25.92,140.25,26,136c4.79-2.39,23.8-2.28,29.25-1c2.4,.56,2.89,2.15,5.99,2.31c19.58,1,41.91,2.4,61.35,1.02c7.89-.56,10.07-8.56,15.77-12.31c.63-.42,26.53-4.4,27.64-4.02c.19,.07,3.77,3.89,7,5c.19,8.63,3.54,17.5-8,17c.2,5.26,.05,10.91,.68,16.12c.47,3.87,2.57,4.17,2.32,9.88c4.79-1.6,5.61-6.29,10.56-7.19c13.57-2.49,15.95,17.64,3.92,19.3c-6.97,.95-8.36-4.2-14.48-5.11c-.23,1.3-.61,5.46-2,6c-5.57,2.17-20,1.98-20-6c-3.78,1.36-7.43,4.86-11.24,5.25c-15.19,1.57-14-22.08,.31-19.72c5.3,.88,6.3,5.93,10.93,7.47c-.18-8.18,1.08-16.52,1-25Z\" fill=\"__C__\" fill-opacity=\"1\"/>" },
        "nd-diag-g5-http-timing": { tf: "", body: "<circle cx=\"11.7\" cy=\"10.9\" r=\"3.5\" stroke=\"__C__\" stroke-width=\"1.4\"/> <path d=\"M11.7 7.4 V6.1 M11.7 10.9 L13.06 9.54\" stroke=\"__C__\" stroke-width=\"1.4\"/> <circle cx=\"11.7\" cy=\"10.9\" r=\"0.6\" fill=\"__C__\" fill-opacity=\"1\"/>" },
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
    // 终端屏幕底色（FIXED_COLORS_LIGHT #B00001 首槽双套）——覆盖静态 _
    // 令其闪烁的覆盖条颜色（TermTypeAnimation 读取）。
    termScreenColors: { dark: "#1E293B", light: "#0F172A" },
    termTypeSets: {
        "nd-diag-g5-ftp": { letters: [
            { d: "M 2.17 16.10 L 2.17 19.00 M 2.17 16.10 L 3.97 16.10 M 2.17 17.48 L 3.28 17.48" },
            { d: "M 5.03 16.10 L 6.96 16.10 M 5.99 16.10 L 5.99 19.00" },
            { d: "M 8.16 16.10 L 8.16 19.00 M 8.16 16.10 L 9.40 16.10 L 9.82 16.24 L 9.95 16.38 L 10.09 16.65 L 10.09 17.07 L 9.95 17.34 L 9.82 17.48 L 9.40 17.62 L 8.16 17.62" },
        ] },
        "nd-diag-g5-ssh": { letters: [
            { d: "M 3.97 16.51 L 3.69 16.24 L 3.28 16.10 L 2.72 16.10 L 2.31 16.24 L 2.03 16.51 L 2.03 16.79 L 2.17 17.07 L 2.31 17.20 L 2.59 17.34 L 3.41 17.62 L 3.69 17.76 L 3.83 17.90 L 3.97 18.17 L 3.97 18.59 L 3.69 18.86 L 3.28 19.00 L 2.72 19.00 L 2.31 18.86 L 2.03 18.59" },
            { d: "M 7.10 16.51 L 6.82 16.24 L 6.41 16.10 L 5.86 16.10 L 5.44 16.24 L 5.16 16.51 L 5.16 16.79 L 5.30 17.07 L 5.44 17.20 L 5.72 17.34 L 6.55 17.62 L 6.82 17.76 L 6.96 17.90 L 7.10 18.17 L 7.10 18.59 L 6.82 18.86 L 6.41 19.00 L 5.86 19.00 L 5.44 18.86 L 5.16 18.59" },
            { d: "M 8.30 16.10 L 8.30 19.00 M 8.30 17.48 L 10.23 17.48 M 10.23 16.10 L 10.23 19.00" },
        ] },
        "nd-diag-g5-telnet": { letters: [
            { d: "M 2.03 16.10 L 3.97 16.10 M 3.00 16.10 L 3.00 19.00" },
            { d: "M 5.30 16.10 L 5.30 19.00 M 5.30 16.10 L 7.10 16.10 M 5.30 17.48 L 6.41 17.48 M 5.30 19.00 L 7.10 19.00" },
            { d: "M 8.30 16.10 L 8.30 19.00 M 8.30 19.00 L 9.95 19.00" },
            { d: "M 11.01 16.10 L 11.01 19.00 M 11.01 16.10 L 12.95 19.00 M 12.95 16.10 L 12.95 19.00" },
            { d: "M 14.28 16.10 L 14.28 19.00 M 14.28 16.10 L 16.08 16.10 M 14.28 17.48 L 15.39 17.48 M 14.28 19.00 L 16.08 19.00" },
            { d: "M 17.14 16.10 L 19.07 16.10 M 18.10 16.10 L 18.10 19.00" },
        ] },
    },
    chevronCycleStepMs: 500, // Proxy 双箭头颜色互换步长（ChevronCycleAnimation）
    chevronCycleSets: {
        "nd-diag-g2-proxy": { sw: 1.6, strokes: [
            { d: "M16.2 8.2 L19.6 12 L16.2 15.8" },
            { d: "M19.2 8.2 L22.6 12 L19.2 15.8" },
        ] },
    },

    plugCycleSets: {
        "nd-diag-g1-active-connections": { groups: [
            { body: '<path d="M17 12l-5 -5l1.5 -1.5a3.536 3.536 0 1 1 5 5l-1.5 1.5z" stroke="#FFFFFF" stroke-width="1.6" /><path d="M18.5 5.5l2.5 -2.5 M13 14l-2 2" stroke="__C__" stroke-width="1.2" />' },
            { body: '<path d="M7 12l5 5l-1.5 1.5a3.536 3.536 0 1 1 -5 -5l1.5 -1.5z" stroke="#FFFFFF" stroke-width="1.6" /><path d="M3 21l2.5 -2.5 M10 11l-2 2" stroke="__C__" stroke-width="1.2" />' }
        ] },
    },

    replayWindowMs: 2400,

    // ── v11 (2026-08-23): DB 名称闪烁 + MTU 箭头伸缩 ───────────────────
    // 5WHY (用户裁定): DB 四图标不用 Pulse 呼吸——闪烁数据库名（页面底部
    // #B00003 名称标签以页面底色覆盖-显隐，同 FlashContent 拍）；MTU 不用
    // Jiggle——底部尺寸双箭头自中线伸缩（卡尺语义）。
    labelFlashOnMs: 460,     // DB 名称亮相时长（与 FlashContent 同拍）
    labelFlashOffMs: 340,    // 熄灭时长
    labelFlashTimes: 5,      // 数据库名称闪烁次数（用户裁定恰 5 次）
    labelFlashRestMs: 1400,  // 5 次后的长休整
    // 名称标签 bbox（chromium 实测 24 系，master #B00003 名称行）+ 覆盖色
    // （= FIXED_COLORS 首槽 #B00001 页面底，dark/light 双套）
    labelFlashSets: {
        "nd-diag-g5-mysql":    { rect: {x: 8.66, y: 17.30, w: 6.68, h: 2.30},
                                  coverDark: "#007CC9", coverLight: "#0094F5" },
        "nd-diag-g5-postgres": { rect: {x: 5.26, y: 17.30, w: 13.49, h: 2.30},
                                  coverDark: "#007CC9", coverLight: "#0094F5" },
        "nd-diag-g5-redis":    { rect: {x: 9.24, y: 17.30, w: 5.53, h: 2.10},
                                  coverDark: "#007CC9", coverLight: "#0094F5" },
        "nd-diag-g5-mongodb":  { rect: {x: 7.20, y: 17.30, w: 9.59, h: 2.10},
                                  coverDark: "#007CC9", coverLight: "#0094F5" },
    },
    measureExtendMs: 500,    // MTU 尺寸箭头外伸时长（MeasureAnimation）
    measureRetractMs: 500,   // 回缩时长

    // ── Settle / transition (used by DiagBlock.qml Behavior) ─────────────
    settleDuration: 300,     // Done settle pop (OutBack)
    transitionDuration: 200, // color / border transitions
};
