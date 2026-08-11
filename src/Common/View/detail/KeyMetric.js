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
// metric/chart can be derived without duck-typing):
//   Ping(0)      rttAvgMs, lossPercent, individualRtts[]
//   Path(1)      hopCount, hops[].rttMs
//   Handshake(2) daysLeft (chart: Gauge)
//   Request(3)   dnsMs, connectMs, sslMs, firstByteMs, totalMs (all CUMULATIVE)
//   Query(4)     latencyMs, connected
//   System(5)    (no chart; properties table only)
// Other keys mapped here (durationMs fallback, queryTimeMs, downloadMbpsBest,
// overallScorePercent, tcpPingMs, rowCount, connectedCount, responseTimeMs)
// are additive conveniences — they must never conflict with the contract keys.
// =============================================================================
.pragma library

// Compact duration formatting for meta lines / fallback display.
// <1s → "342ms"; <60s → "1.2s"; >=60s → "1:23" (m:ss).
function formatDuration(ms) {
    var d = Number(ms) || 0
    if (d <= 0) return "--"
    if (d < 1000) return Math.round(d) + "ms"
    if (d < 60000) return (d / 1000).toFixed(1) + "s"
    var min = Math.floor(d / 60000)
    var sec = Math.round((d % 60000) / 1000)
    return min + ":" + (sec < 10 ? "0" : "") + sec
}

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
// data: the enriched data map (templateType injected by C++) or null/undefined.
// durationMs: fallback metric used when no structured key is present.
function keyMetric(data, durationMs) {
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

    // ── Fallback: execution duration (with or without structured data) ──
    var dur = Number(durationMs) || 0
    if (dur <= 0) return empty
    if (dur < 1000)
        return { ok: true, value: dur, unitKey: "unitMs", labelKey: "metricDuration",
                 precision: 0, format: "num", trailing: "" }
    if (dur < 60000)
        return { ok: true, value: dur / 1000, unitKey: "unitSec", labelKey: "metricDuration",
                 precision: 1, format: "num", trailing: "" }
    // >= 60s: min:sec — MetricCard formats from total seconds (no string parsing)
    return { ok: true, value: Math.round(dur / 1000), unitKey: "", labelKey: "metricDuration",
             precision: 0, format: "minsec", trailing: "" }
}
