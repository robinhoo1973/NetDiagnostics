// =============================================================================
// G3InternetConnectivity.cpp — Internet Connectivity diagnostic (G3)
//
// Uses GeoProbe for TTFB-based server probing + speed test on best server.
// Wired to DiagId::G3InternetSpeedTest via TaskFactory.
// =============================================================================
#include "Diagnostics/Model/GeoProbe.h"
#include "Diagnostics/Model/GHelpers.h"

namespace G1G2G3Native {

DiagnosticResult internetConnectivity(DiagId id) {
    DiagnosticResult r;
    r.id = id; r.group = diagGroup(id);
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();

    GeoProbe gp;

    ProbeConfig cfg;
    cfg.scope = ProbeConfig::Global;
    cfg.rounds = 3;
    cfg.aggregation = ProbeConfig::ByCountry;

    gp.probe(cfg);
    ProbeResult result = gp.getFeedback(cfg);

    QStringList out;
    out.append(QStringLiteral("Internet Connectivity"));
    out.append(QStringLiteral("Method: TTFB global probe → 3-round → HL aggregation → speed test"));
    out.append(QString());

    // ── Phase 1: Location ──
    out.append(QStringLiteral("Physical location: %1").arg(result.physicalCountry));
    out.append(QStringLiteral("Probed %1 servers, %2 countries reachable")
        .arg(result.servers.size()).arg(result.countries.size()));
    out.append(QString());

    // ── Phase 2: Top 5 servers ──
    int shown = 0;
    for (auto& cr : result.countries) {
        for (auto& sr : cr.servers) {
            if (shown >= 5) break;
            out.append(QStringLiteral("  %1. %2 (%3) — %4ms")
                .arg(shown + 1).arg(sr.host).arg(cr.code).arg(sr.ttfbMs, 0, 'f', 1));
            shown++;
        }
        if (shown >= 5) break;
    }
    out.append(QString());

    // ── Phase 3: Best server (first server in first country = lowest latency) ──
    if (result.servers.isEmpty()) {
        out.append(QStringLiteral("No reachable server found"));
        r.summary = QStringLiteral("No internet connectivity");
        r.status = DiagStatus::Fail;
        r.rawOutput = out.join('\n'); r.details = r.rawOutput;
        r.durationMs = t.elapsed(); return r;
    }

    // Pick best server: first in servers list (sorted by HL latency in country order)
    const ServerResult& best = result.servers[0];
    out.append(QStringLiteral("Best server (%1): %2:%3 — %4ms (95%CI ±%5ms, %6 rounds)")
        .arg(best.country, best.host).arg(best.port)
        .arg(best.ttfbMs, 0, 'f', 1).arg(best.ciHalf, 0, 'f', 1).arg(best.rounds));
    out.append(QString());

    // ── Phase 4: Speed test on best server ──
    out.append(QStringLiteral("Running speed test on %1:%2...").arg(best.host).arg(best.port));
    QString dlUrl = QStringLiteral("http://%1:%2/download?size=250000")
        .arg(best.host).arg(best.port);
    SpeedResult dl = httpDownload(dlUrl, 250000, 15000);
    if (dl.ok && dl.mbps > 0.01) {
        out.append(QStringLiteral("  Download: %1 Mbps (%2 bytes in %3ms)")
            .arg(dl.mbps, 0, 'f', 1).arg(dl.bytes).arg(dl.durationMs));
        r.summary = QStringLiteral("Connected — %1 (%2ms, %3 Mbps)")
            .arg(result.physicalCountry).arg(best.ttfbMs, 0, 'f', 0)
            .arg(dl.mbps, 0, 'f', 1);
        r.status = DiagStatus::Pass;
    } else {
        out.append(QStringLiteral("  Speed test failed — server unreachable for download"));
        r.summary = QStringLiteral("Connected — %1 (%2ms, speed N/A)")
            .arg(result.physicalCountry).arg(best.ttfbMs, 0, 'f', 0);
        r.status = DiagStatus::Warning;
    }

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.durationMs = t.elapsed();
    return r;
}

} // namespace G1G2G3Native
