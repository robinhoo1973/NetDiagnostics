// =============================================================================
// KeyMetric.js — single source of truth for the "key metric" headline number
//
// Consumed by DetailPage.qml (MetricCard) and DiagBlock.qml (tile metric line).
// 5WHY: DetailPage._keyMetricValue/_keyMetricLabel/_keyMetricUnit and
// DiagBlock._keyMetric were TWO independent implementations with different
// key priorities and formats — adding a new metric only updated one side,
// and DetailPage did `parseFloat(_keyMetricValue)` on a string that could be
// "1:23" (m:ss), silently corrupting sub-minute durations into "1".
// Fix: one stateless module both files import.  Because it is a
// `.pragma library` module it cannot access context properties (T/appState),
// so it returns raw values plus TRANSLATION KEYS; callers translate with T.tr().
//
// Return contract:
//   { ok: bool, value: number, unitKey: string, labelKey: string,
//     precision: int, format: "num" | "minsec", trailing: string }
//
// R8 — per-template key contract (the keys each template MUST emit so the
// metric/chart can be derived without duck-typing). 模板标识用 chartKey
// 令牌（diagTemplateKey()，DiagNames.h 单一映射）——曾列枚举序值，序值
// 不跨语言（枚举重排曾灭掉全部详情图表）。
//   ping          rttAvgMs, lossPercent, individualRtts[]
//   path          hopCount, hops[].rttMs
//   handshake     daysLeft (chart: Gauge)
//   request       dnsMs, connectMs, sslMs, firstByteMs, totalMs (all CUMULATIVE)
//   query         latencyMs, connected
//   system        (no chart; properties table only)
// Other keys mapped here (queryTimeMs, downloadMbpsBest, overallScorePercent,
// tcpPingMs, rowCount, connectedCount, responseTimeMs) are additive
// conveniences — they must never conflict with the contract keys.
// (durationMs is not a metric here: the hero shows it; 5WHY 2026-08-19.)
// =============================================================================
.pragma library

// Pure number formatting shared by MetricCard (animated) and DiagBlock
// (static tile text): respects precision and the "minsec" format.
// 5WHY: MetricCard._displayText and DiagBlock._keyMetric each implemented
// the same toFixed/min:sec logic — one helper removes the duplication.
function secondsToMinSec(totalSec) {
    var s = Math.max(0, Math.round(totalSec))
    var mm = Math.floor(s / 60)
    var ss = s % 60
    return mm + ":" + (ss < 10 ? "0" : "") + ss
}
function formatNumber(value, precision, format) {
    if (format === "minsec") return secondsToMinSec(value)
    return Number(value).toFixed(precision)
}

// Structured key metric for a diagnostic result.
// data: the enriched data map (chartKey injected by C++) or null/undefined.
// 5WHY (2026-08-19): durationMs 参数随时长兜底一并移除（hero 恒显示时长，
// 主指标卡只承载结构化指标）。
function keyMetric(data) {
    var empty = { ok: false, value: 0, unitKey: "", labelKey: "",
                  precision: 0, format: "num", trailing: "" }
    var d = data || {}

    // ── Structured metrics — ordered by priority (first match wins) ────
    if (d.rttAvgMs !== undefined)      // Ping / latency: RTT average
        return { ok: true, value: Number(d.rttAvgMs),      unitKey: "unitMs",
                 labelKey: "metricAvg", precision: 0, format: "num", trailing: "" }
    if (d.hopCount !== undefined)      // Traceroute / PathPing
        return { ok: true, value: Number(d.hopCount),      unitKey: "unitHops",
                 labelKey: "metricHops", precision: 0, format: "num", trailing: "" }
    if (d.totalMs !== undefined)       // HTTP timing waterfall total
        return { ok: true, value: Number(d.totalMs),       unitKey: "unitMs",
                 labelKey: "metricTotal", precision: 0, format: "num", trailing: "" }
    if (d.daysLeft !== undefined)      // TLS certificate validity
        return { ok: true, value: Number(d.daysLeft),      unitKey: "unitDays",
                 labelKey: "metricDaysLeft", precision: 0, format: "num", trailing: "" }
    if (d.lossPercent !== undefined)   // Packet loss (needs 1 decimal)
        return { ok: true, value: Number(d.lossPercent),   unitKey: "unitPercent",
                 labelKey: "metricLoss", precision: 1, format: "num", trailing: "" }
    if (d.latencyMs !== undefined)     // Query / DB connect latency
        return { ok: true, value: Number(d.latencyMs),     unitKey: "unitMs",
                 labelKey: "metricLatency", precision: 0, format: "num", trailing: "" }
    if (d.queryTimeMs !== undefined)   // DNS resolution query time
        return { ok: true, value: Number(d.queryTimeMs),   unitKey: "unitMs",
                 labelKey: "metricQueryTime", precision: 0, format: "num", trailing: "" }
    if (d.downloadMbpsBest !== undefined)  // Speed test: headline is bandwidth
        return { ok: true, value: Number(d.downloadMbpsBest), unitKey: "unitMbps",
                 labelKey: "metricDownload", precision: 1, format: "num", trailing: "" }
    if (d.overallScorePercent !== undefined)  // DNS integrity cleanliness score
        return { ok: true, value: Number(d.overallScorePercent), unitKey: "unitPercent",
                 labelKey: "metricScore", precision: 0, format: "num", trailing: "" }
    // 5WHY: G5SecurityHeaders declares keyMetricField="score" and emits
    // data["score"] (7 - missingHeaders), but no handler existed — the
    // metric fell through to the duration fallback, showing "Duration: 1.2s"
    // instead of the security score.  The Gauge chart (tt=2) already read
    // `score`; the MetricCard now agrees with it.
    if (d.score !== undefined)         // G5SecurityHeaders: security score (/totalRequired)
        return { ok: true, value: Number(d.score), unitKey: "",
                 labelKey: "metricScore", precision: 0, format: "num", trailing: "" }
    if (d.tcpPingMs !== undefined)     // Speed test latency
        return { ok: true, value: Number(d.tcpPingMs),     unitKey: "unitMs",
                 labelKey: "metricLatency", precision: 0, format: "num", trailing: "" }
    if (d.rowCount !== undefined)      // DB rows returned
        return { ok: true, value: Number(d.rowCount),      unitKey: "",
                 labelKey: "metricRows", precision: 0, format: "num", trailing: "" }
    if (d.connectedCount !== undefined) {  // IPv6 port reachability "n/total"
        var m = { ok: true, value: Number(d.connectedCount), unitKey: "",
                  labelKey: "metricConnected", precision: 0, format: "num", trailing: "" }
        if (d.totalPorts !== undefined && Number(d.totalPorts) > 0)
            m.trailing = "/" + Number(d.totalPorts)
        return m
    }
    // 5WHY: 8 count-based metrics declared in DiagnosticMeta keyMetricField
    // were missing from this priority list — tests fell through to the
    // duration-based fallback instead of showing their intended count metric.
    // Order is alphabetized; each test only emits its own field so priority
    // among these does not matter.
    if (d.cacheEntries !== undefined)    // G3DnsCache: cached DNS entry count
        return { ok: true, value: Number(d.cacheEntries),   unitKey: "",
                 labelKey: "metricEntries", precision: 0, format: "num", trailing: "" }
    if (d.entryCount !== undefined)      // G2ArpTable: ARP table entry count
        return { ok: true, value: Number(d.entryCount),     unitKey: "",
                 labelKey: "metricEntries", precision: 0, format: "num", trailing: "" }
    if (d.headerCount !== undefined)     // G5HttpHeaders: response header count
        return { ok: true, value: Number(d.headerCount),    unitKey: "unitHeaders",
                 labelKey: "metricHeaders", precision: 0, format: "num", trailing: "" }
    if (d.leaseCount !== undefined)      // G1DhcpStatus: DHCP lease count
        return { ok: true, value: Number(d.leaseCount),     unitKey: "",
                 labelKey: "metricLeases", precision: 0, format: "num", trailing: "" }
    if (d.redirectCount !== undefined)   // G5HttpRedirect: redirect count
        return { ok: true, value: Number(d.redirectCount),  unitKey: "unitHops",
                 labelKey: "metricHops", precision: 0, format: "num", trailing: "" }
    if (d.routeCount !== undefined)      // G2RoutingTable: route table entry count
        return { ok: true, value: Number(d.routeCount),     unitKey: "",
                 labelKey: "metricRoutes", precision: 0, format: "num", trailing: "" }
    if (d.serverCount !== undefined)     // G3DnsServers: DNS server count
        return { ok: true, value: Number(d.serverCount),    unitKey: "",
                 labelKey: "metricServers", precision: 0, format: "num", trailing: "" }
    if (d.tcpCount !== undefined)        // G1ActiveConnections: TCP connection count
        return { ok: true, value: Number(d.tcpCount),       unitKey: "",
                 labelKey: "metricConnections", precision: 0, format: "num", trailing: "" }
    if (d.responseTimeMs !== undefined)  // misc query tests
        return { ok: true, value: Number(d.responseTimeMs), unitKey: "unitMs",
                 labelKey: "metricLatency", precision: 0, format: "num", trailing: "" }

    // 5WHY (2026-08-19 用户诉求 "详情页不单独列出 Duration 区块"): 曾有
    // 执行时长兜底——结构化键缺失的失败结果（黑洞 Ping、TLS 握手失败等）
    // 以"Duration: 3.2s"占据整个 MetricCard。但 PageHeroSection 恒显示
    // 时长行（detailDurationLabel）——失败详情页上时长被重复呈现两块，
    // 且独立大卡仅承载一个可以从 hero 读到的数字（业界惯例：主指标卡只
    // 承载结构化主指标；时长属元信息，归 hero/summary 区）。失败结果由
    // 错误区块 + 属性卡承载真实诊断数据（Host/Port 等已随属性行恢复）。
    // keyMetricField 注入链随兜底一并移除（AppState.resultFor）。
    return empty
}
