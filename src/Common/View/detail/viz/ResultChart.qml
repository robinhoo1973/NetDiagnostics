// =============================================================================
// ResultChart.qml — template-driven chart (BarChart | Gauge) for the detail page
//
// Owns the L5 chart wiring that previously lived inside DetailPage.qml:
//   • template → source selection (DiagTemplateType: 0=Ping,1=Path,2=Handshake,
//     3=Request,4=Query,5=System)
//   • series construction with TRANSLATED labels (T.tr) and THEME colors
//   • Gauge spec (cert validity / connect latency) with dynamic scale
//   • Loader + bind-on-load + re-bind on language change
//   • per-template height (0 when collapsed)
//
// 5WHY (structure): DetailPage grew to ~700 lines with five chart-related
// properties (_chartSource/_chartSeries/_gaugeSpec/_hasChartData/_bindChart)
// all reading the same data map.  Extracting them into one self-contained
// component makes the chart a single reusable unit: DetailPage just passes
// `data` + `expanded` and reads `hasChart`/`seriesCount`.
//
// Usage:
//   ResultChart { Layout.fillWidth: true; data: page.resultData; expanded: page.chartsExpanded }
// =============================================================================
import QtQuick
import theme

Item {
    id: root

    // ── Public API ────────────────────────────────────────────────────────
    property var data: ({})       // enriched resultData (templateType injected)
    property bool expanded: false
    property int gaugeHeight: 64
    // Has a real visualization wired for this template+data?
    readonly property bool hasChart: _source !== ""
    // Number of series bars (callers use it to pick default expansion).
    readonly property int seriesCount: _series.length
    // Cap for BarChart so long series (50+ RTT samples) scroll internally
    // instead of stretching the card unboundedly.
    readonly property real _maxChartHeight: 300

    // Height: 0 when collapsed; otherwise the loaded chart's NATURAL content
    // height (single source of truth — no duplicated height math to drift).
    implicitHeight: !root.expanded ? 0 : _preferredHeight
    readonly property real _preferredHeight: {
        // 5WHY: reading chartLoader.item.implicitHeight makes the loaded
        // component the single source of truth for chart height.  The old
        // formula (Gauge: 80, BarChart: N*28+40) duplicated the chart internals
        // and drifted from real content (Gauge ≈58px, BarChart ≈N*22+10),
        // leaving 22-30px of dead space inside the Charts card on every
        // chart-bearing detail page (all G3+ Handshake/Query tests).
        if (chartLoader.item && chartLoader.item.implicitHeight > 0) {
            return Math.max(40, Math.min(root._maxChartHeight, chartLoader.item.implicitHeight))
        }
        // Pre-load fallback — replaced on the next frame once the chart loads.
        var tt = root.data.templateType
        if (tt === 2 || tt === 4) return root.gaugeHeight   // Gauge: compact
        return Math.max(40, Math.min(root._maxChartHeight, _series.length * 28 + 40))
    }

    // ── Loader (BarChart | Gauge) ────────────────────────────────────────
    Loader {
        id: chartLoader
        anchors.fill: parent
        active: root.expanded && root._source !== ""
        source: root._source
        visible: active
        onLoaded: { if (item) root._bind(item) }
        // F.1 规则：Loader 错误必须落日志（5WHY review 2026-08-17：图表源
        // URL 指向已不存在的 qrc:/qml/… 前缀时，Loader.Error 静默 → 空白
        // 图表卡无人察觉）
        onStatusChanged: {
            if (status === Loader.Error)
                console.warn("ResultChart: Loader error for source " + root._source)
        }
        Accessible.name: T.tr("detailData") + " " + T.tr("accChart")
        Accessible.role: Accessible.Graphic
    }

    // 5WHY: series labels/units are translated — the BarChart/Gauge received
    // them via one-shot direct assignment, so re-bind on language switch.
    Connections {
        target: T
        function onLangChanged() { if (chartLoader.item) root._bind(chartLoader.item) }
    }

    // ── Source selection ─────────────────────────────────────────────────
    readonly property string _source: {
        var tt = root.data.templateType
        // 5WHY: a failed ping still carries an EMPTY individualRtts list —
        // gating on the key alone would open an empty chart.  Require samples.
        if (tt === 0 && root.data.individualRtts !== undefined
            && root.data.individualRtts.length > 0)       return "qrc:/qt/qml/detail/viz/BarChart.qml"
        if (tt === 1 && root.data.hops !== undefined)     return "qrc:/qt/qml/detail/viz/BarChart.qml"
        // 5WHY: Handshake originally checked only daysLeft (TLS cert expiry).
        // DnsIntegrity emits overallScorePercent, SecurityHeaders emits score,
        // InternetConnectivity emits downloadMbpsBest — none of which write
        // daysLeft, so the Gauge never opened.  Accept any Gauge-compatible key.
        if (tt === 2 && (root.data.daysLeft !== undefined
                      || root.data.overallScorePercent !== undefined
                      || root.data.score !== undefined
                      || root.data.downloadMbpsBest !== undefined))
                                                         return "qrc:/qt/qml/detail/viz/Gauge.qml"
        if (tt === 3 && root.data.dnsMs !== undefined)    return "qrc:/qt/qml/detail/viz/BarChart.qml"
        // Query (tt=4): connect latency gauge
        if (tt === 4 && root.data.latencyMs !== undefined) return "qrc:/qt/qml/detail/viz/Gauge.qml"
        return ""
    }

    // ── Series (translated labels + theme colors) ────────────────────────
    readonly property var _series: {
        var out = []
        var tt = root.data.templateType
        var P = ThemeEngine.colors.primary
        if (tt === 0 && root.data.individualRtts) {
            for (var i = 0; i < root.data.individualRtts.length; i++)
                out.push({ label: "p" + (i + 1), value: Number(root.data.individualRtts[i]), color: P })
        } else if (tt === 1 && root.data.hops) {
            for (var j = 0; j < root.data.hops.length; j++) {
                var h = root.data.hops[j]
                out.push({ label: String(h.ttl !== undefined ? h.ttl : j + 1),
                           value: Number(h.rttMs !== undefined ? h.rttMs : 0), color: P })
            }
        } else if (tt === 3) {
            // 5WHY: curl's connect/ssl/ttfb/total timers are CUMULATIVE from
            // request start — plot per-phase deltas (clamped ≥ 0) so every
            // bar shows the actual phase duration; cached phases render 0.
            var dns = Number(root.data.dnsMs) || 0
            var conn = Number(root.data.connectMs) || 0
            var ssl = Number(root.data.sslMs) || 0
            var fb = Number(root.data.firstByteMs) || 0
            var tot = Number(root.data.totalMs) || 0
            var phases = [
                { label: T.tr("chartDns"),      value: dns,                   color: ThemeEngine.colors.primary },
                { label: T.tr("chartTcp"),      value: Math.max(0, conn-dns), color: ThemeEngine.colors.secondary },
                { label: T.tr("chartTls"),      value: Math.max(0, ssl-conn), color: ThemeEngine.colors.success },
                { label: T.tr("chartTtfb"),     value: Math.max(0, fb-ssl),   color: ThemeEngine.colors.warning },
                { label: T.tr("chartTransfer"), value: Math.max(0, tot-fb),   color: ThemeEngine.colors.accent }
            ]
            for (var k = 0; k < phases.length; k++)
                if (phases[k].value !== undefined) out.push(phases[k])
        } else if (tt === 4 && root.data.latencyMs !== undefined) {
            out.push({ label: T.tr("chartConnect"), value: Number(root.data.latencyMs),
                       color: root.data.connected ? ThemeEngine.colors.success
                                                  : ThemeEngine.colors.fail })
        }
        return out
    }

    // ── Gauge spec ───────────────────────────────────────────────────────
    readonly property var _gaugeSpec: {
        if (root.data.templateType === 2 && root.data.daysLeft !== undefined) {
            var dl = Number(root.data.daysLeft)
            // 5WHY: color follows the same thresholds as the C++ verdict —
            // an EXPIRED cert must render red, not green.
            var col = dl < 0 ? ThemeEngine.colors.fail
                    : dl < 30 ? ThemeEngine.colors.warning
                    : ThemeEngine.colors.success
            return { value: dl, max: 365, unitKey: "unitDays", color: col,
                     emptyLabel: dl < 0 ? T.tr("gaugeExpired") : "" }
        }
        // 5WHY: Handshake template now supports non-TLS data shapes:
        // DnsIntegrity(overallScorePercent 0-100), SecurityHeaders(score/totalRequired),
        // InternetConnectivity(downloadMbpsBest).
        if (root.data.templateType === 2 && root.data.overallScorePercent !== undefined) {
            var sp = Number(root.data.overallScorePercent)
            var sc = sp >= 80 ? ThemeEngine.colors.success
                   : sp >= 50 ? ThemeEngine.colors.warning
                   : ThemeEngine.colors.fail
            return { value: sp, max: 100, unitKey: "unitPercent", color: sc }
        }
        if (root.data.templateType === 2 && root.data.score !== undefined) {
            var sv = Number(root.data.score)
            var mx = Number(root.data.totalRequired) || 7
            var sCol = sv >= Math.ceil(mx * 0.7) ? ThemeEngine.colors.success
                     : sv >= Math.ceil(mx * 0.4) ? ThemeEngine.colors.warning
                     : ThemeEngine.colors.fail
            return { value: sv, max: mx, unitKey: "unitHeaders", color: sCol }
        }
        if (root.data.templateType === 2 && root.data.downloadMbpsBest !== undefined) {
            var bw = Number(root.data.downloadMbpsBest)
            // 5WHY: bandwidth has no "pass/fail" — show info blue.  Scale
            // with headroom (min 10 Mbps so even low speeds fill visibly).
            var bwMax = Math.max(10, Math.ceil(bw * 1.3 / 5) * 5)
            return { value: bw, max: bwMax, unitKey: "unitMbps",
                     color: ThemeEngine.colors.info }
        }
        if (root.data.templateType === 4 && root.data.latencyMs !== undefined) {
            var lat = Number(root.data.latencyMs)
            // 5WHY: a fixed 5000ms scale made real latencies (4-50ms) render
            // as an invisible sliver.  Scale with ~20% headroom (min 100ms).
            var scale = Math.max(100, Math.ceil(lat * 1.2 / 50) * 50)
            return { value: lat, max: scale, unitKey: "unitMs",
                     color: root.data.connected ? ThemeEngine.colors.success
                                                : ThemeEngine.colors.fail }
        }
        return null
    }

    // ── Bind loaded chart component ──────────────────────────────────────
    // 5WHY: dispatch by templateType enum instead of string-matching the
    // source URL — decouples the wiring from QRC path naming.
    function _bind(item) {
        if (!item) return
        var tt = root.data.templateType
        if (tt === 0 || tt === 1 || tt === 3) {
            // Ping / Path / Request → BarChart
            if (_series.length) item.values = _series
        } else if ((tt === 2 || tt === 4) && _gaugeSpec) {
            // Handshake / Query → Gauge (cert validity, connect latency)
            item.value = _gaugeSpec.value
            item.maxValue = _gaugeSpec.max
            item.unit = Qt.binding(function() { return T.tr(_gaugeSpec.unitKey) })
            item.gaugeColor = _gaugeSpec.color
            item.emptyLabel = Qt.binding(function() { return _gaugeSpec.emptyLabel || "" })
        }
    }
}
