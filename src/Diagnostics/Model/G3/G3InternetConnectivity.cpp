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

    GeoProbe probe;
    GeoProbe::Result result = probe.probe(45);

    QStringList out;
    out.append(QStringLiteral("Internet Connectivity"));
    out.append(QStringLiteral("Method: TTFB global probe → top 5 → 3-round CI → speed test"));
    out.append(QString());

    // ── Phase 1-2: Location ──
    out.append(QStringLiteral("Physical location: %1").arg(result.physicalCountry));
    out.append(QStringLiteral("Probed %1 servers, %2 reachable (%3s)")
        .arg(result.totalServers).arg(result.totalOk).arg(result.durationSec, 0, 'f', 1));
    out.append(QString());

    // ── Phase 3: Top 5 servers ──
    int shown = 0;
    for (auto& cr : result.countries) {
        for (auto& sr : cr.servers) {
            if (shown >= 5) break;
            out.append(QStringLiteral("  %1. %2 (%3) — %4ms")
                .arg(shown + 1).arg(sr.sponsor).arg(cr.code).arg(sr.ttfbMs, 0, 'f', 1));
            shown++;
        }
        if (shown >= 5) break;
    }
    out.append(QString());

    // ── Phase 4: Best server with CI ──
    if (result.bestServer.valid) {
        out.append(QStringLiteral("Best server (%1, in %2): %3 — %4ms (95%CI ±%5ms, %6 rounds)")
            .arg(result.physicalCountry, result.bestServer.country)
            .arg(result.bestServer.sponsor)
            .arg(result.bestServer.ttfbMs, 0, 'f', 1)
            .arg(result.bestServer.ttfbCI, 0, 'f', 1)
            .arg(result.bestServer.rounds));
    } else {
        out.append(QStringLiteral("No reachable server in %1").arg(result.physicalCountry));
        r.summary = QStringLiteral("Location: %1").arg(result.physicalCountry);
        r.status = DiagStatus::Warning;
        r.rawOutput = out.join('\n'); r.details = r.rawOutput;
        r.durationMs = t.elapsed(); return r;
    }

    // ── Phase 5: Speed test on best server ──
    out.append(QString());
    out.append(QStringLiteral("Running speed test on %1...").arg(result.bestServer.sponsor));
    QString dlUrl = QStringLiteral("http://%1:%2/download?size=250000")
        .arg(result.bestServer.host).arg(result.bestServer.port);
    SpeedResult dl = httpDownload(dlUrl, 250000, 15000);
    if (dl.ok && dl.mbps > 0.01) {
        out.append(QStringLiteral("  Download: %1 Mbps (%2 bytes in %3ms)")
            .arg(dl.mbps, 0, 'f', 1).arg(dl.bytes).arg(dl.durationMs));
        r.summary = QStringLiteral("Connected — %1 (%2ms, %3 Mbps)")
            .arg(result.physicalCountry).arg(result.bestServer.ttfbMs, 0, 'f', 0)
            .arg(dl.mbps, 0, 'f', 1);
        r.status = DiagStatus::Pass;
    } else {
        out.append(QStringLiteral("  Speed test failed — server unreachable for download"));
        r.summary = QStringLiteral("Connected — %1 (%2ms, speed N/A)")
            .arg(result.physicalCountry).arg(result.bestServer.ttfbMs, 0, 'f', 0);
        r.status = DiagStatus::Warning;
    }

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.durationMs = t.elapsed();
    return r;
}

} // namespace G1G2G3Native
