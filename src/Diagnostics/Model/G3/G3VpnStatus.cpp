#include "Diagnostics/Model/G3/G3InternetDns.h"
#include "Diagnostics/Model/GHelpers.h"

namespace G1G2G3Native {

// ── VPN Status Detection — country-agnostic ──────────────────────────
// 5WHY: VPN detection was CN-only (hardcoded CN site probes and CN server
// latency checks).  A user in Japan behind a US VPN would be misclassified.
// Now probes servers from the GeoIP country + 2 reference regions (CN, US)
// and compares latencies: if GeoIP country servers have high latency but
// another region has low latency → VPN detected.
//
// Classification:
//   No VPN       — GeoIP country servers reachable with low latency
//   VPN detected — GeoIP country servers high/unreachable, other region low
//   Uncertain    — insufficient data to determine

DiagnosticResult vpnStatus(DiagId id) {
    DiagnosticResult r;
    r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;
    out.append(QStringLiteral("VPN Status Detection"));
    out.append(QStringLiteral("Cross-references GeoIP country with actual server reachability:"));
    out.append(QStringLiteral("  - Multi-region connectivity check"));
    out.append(QStringLiteral("  - GeoIP country detection"));
    out.append(QStringLiteral("  - TCP latency probes from GeoIP country + reference regions"));
    out.append(QString());

    // ── Step 1: Multi-region connectivity check ───────────────────
    struct { const char* host; int port; const char* name; const char* region; } sites[] = {
        {"223.5.5.5", 53, "Alibaba DNS", "CN"},
        {"baidu.com", 443, "Baidu", "CN"},
        {"119.29.29.29", 53, "DNSPod DNS", "CN"},
        {"8.8.8.8", 53, "Google DNS", "US"},
        {"1.1.1.1", 53, "Cloudflare DNS", "US"},
        {"dns.google", 53, "Google DNS2", "US"},
        {"9.9.9.9", 53, "Quad9 DNS", "EU"},
        {"208.67.222.222", 53, "OpenDNS", "US"},
    };
    QMap<QString,int> regionReachable;
    out.append(QStringLiteral("--- Connectivity Check -------------------------------------------------"));
    for (auto& s : sites) {
        int lat = tcpPingMs(s.host, s.port);
        bool ok = (lat >= 0);
        if (ok) regionReachable[QString::fromUtf8(s.region)]++;
        out.append(QStringLiteral("  %1  %2:%3  [%4]  %5  %6 ms")
            .arg(QString::fromUtf8(s.name).leftJustified(14, ' '))
            .arg(s.host).arg(s.port)
            .arg(QString::fromUtf8(s.region).leftJustified(2, ' '))
            .arg(ok ? QStringLiteral("OK") : QStringLiteral("--"))
            .arg(ok ? QString::number(lat) : QStringLiteral("-")));
    }

    // ── Step 2: GeoIP detection ──────────────────────────────────
    QString geoCountry = SpeedTest::detectCountry(3000);
    out.append(QString());
    out.append(QStringLiteral("GeoIP country: %1").arg(geoCountry == "XX" ? "Unknown" : geoCountry));

    // ── Step 3: Probe speed-test servers from GeoIP country + reference regions ──
    // Map country codes to probe servers.  Falls back to CN/US if unknown.
    struct RegionProbe { const char* label; const char* host; int port; };
    QMap<QString, QVector<RegionProbe>> probes;
    // Primary: GeoIP country's servers
    probes[geoCountry].append({geoCountry.toUtf8().constData(),
        "speedtest.tele2.net", 8080}); // generic fallback always included
    // Reference: CN (always, largest pool)
    probes[QStringLiteral("CN")] = {
        {"China Mobile GD", "speedtest1.gd.chinamobile.com", 8080},
        {"China Mobile BJ", "speedtest.bj.chinamobile.com", 8080},
        {"China Telecom SH", "speedtest1.online.sh.cn", 8080},
    };
    // Reference: US
    probes[QStringLiteral("US")] = {
        {"US Xfinity", "speedtest.xfinity.com", 8080},
    };

    struct RegionResult { QString code; double latencyMs; int reachable; };
    QVector<RegionResult> regionResults;
    out.append(QString());
    out.append(QStringLiteral("--- Regional Latency Probes ---------------------------------------------"));

    for (auto it = probes.begin(); it != probes.end(); ++it) {
        double bestLat = -1;
        int reachable = 0;
        for (auto& p : it.value()) {
            int lat = tcpPingMs(p.host, p.port);
            if (lat >= 0) {
                reachable++;
                if (bestLat < 0 || lat < bestLat) bestLat = lat;
            }
        }
        if (reachable > 0) {
            regionResults.append({it.key(), bestLat, reachable});
            out.append(QStringLiteral("  [%1] %2/%3 reachable, best %4ms")
                .arg(it.key(), 2)
                .arg(reachable).arg(it.value().size())
                .arg(QString::number((int)bestLat)));
        }
    }

    // ── Step 4: Classify (country-agnostic) ─────────────────────────
    QString scenario;
    QString details;
    DiagStatus status = DiagStatus::Pass;

    // Sort by latency ascending
    std::sort(regionResults.begin(), regionResults.end(),
              [](const RegionResult& a, const RegionResult& b) { return a.latencyMs < b.latencyMs; });

    if (regionResults.isEmpty()) {
        scenario = QStringLiteral("No connectivity detected");
        details = QStringLiteral("All probes failed — cannot determine VPN status");
        status = DiagStatus::Fail;
    } else {
        QString bestRegion = regionResults[0].code;
        double bestLat = regionResults[0].latencyMs;

        // Find GeoIP country's result
        const RegionResult* geoResult = nullptr;
        for (auto& rr : regionResults) {
            if (rr.code == geoCountry) { geoResult = &rr; break; }
        }

        bool geoIpEmpty = (geoCountry == QStringLiteral("XX"));
        if (geoIpEmpty) {
            // GeoIP failed — use connectivity alone
            scenario = QStringLiteral("Approximate location: %1").arg(bestRegion);
            details = QStringLiteral("GeoIP failed. Lowest latency region: %1 (%2ms, %3 servers)")
                .arg(bestRegion).arg((int)bestLat).arg(regionResults[0].reachable);
            status = DiagStatus::Info;
        } else if (geoResult) {
            double geoLat = geoResult->latencyMs;
            // If GeoIP country servers have low latency → no VPN
            if (geoLat < 50 && geoLat <= bestLat * 1.5) {
                scenario = QStringLiteral("No VPN detected (%1)").arg(geoCountry);
                details = QStringLiteral("GeoIP=%1 matches server latency (%2ms). %3/%4 reachable.")
                    .arg(geoCountry).arg((int)geoLat).arg(geoResult->reachable)
                    .arg(probes[geoCountry].size());
            }
            // GeoIP country servers high latency, but other region low → VPN
            else if (geoLat > 100 || (probes[geoCountry].size() > 0 && geoResult->reachable < probes[geoCountry].size())) {
                QString likelyRegion = (regionResults.size() > 1 && regionResults[0].code != geoCountry)
                    ? regionResults[0].code : QStringLiteral("unknown");
                scenario = QStringLiteral("VPN detected (GeoIP=%1, likely in %2)").arg(geoCountry).arg(likelyRegion);
                details = QStringLiteral("GeoIP=%1 (%2ms latency) but %3 servers have %4ms latency. "
                    "Mismatch indicates VPN: user appears in %1 but connectivity suggests %3.")
                    .arg(geoCountry).arg((int)geoLat).arg(likelyRegion).arg((int)bestLat);
                status = DiagStatus::Warning;
            } else {
                scenario = QStringLiteral("Uncertain (%1)").arg(geoCountry);
                details = QStringLiteral("GeoIP=%1, latency %2ms. Insufficient data to confirm/deny VPN.")
                    .arg(geoCountry).arg((int)geoLat);
                status = DiagStatus::Info;
            }
        } else {
            // GeoIP country has no probe data — use connectivity
            int geoCon = regionReachable.value(geoCountry, 0);
            if (geoCon > 0 && bestLat < 50) {
                scenario = QStringLiteral("No VPN detected (%1)").arg(geoCountry);
                details = QStringLiteral("GeoIP=%1, %2 sites reachable, best latency %3ms")
                    .arg(geoCountry).arg(geoCon).arg((int)bestLat);
            } else {
                scenario = QStringLiteral("Uncertain — no probe data for %1").arg(geoCountry);
                details = QStringLiteral("GeoIP=%1 but no speed-test servers available for this country.")
                    .arg(geoCountry);
                status = DiagStatus::Info;
            }
        }
    }

    out.append(QString());
    out.append(QStringLiteral("--- Result -----------------------------------------------------------------"));
    out.append(QStringLiteral("  %1").arg(scenario));
    out.append(QStringLiteral("  %1").arg(details));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;

    // User-friendly summary label
    if (scenario.startsWith(QStringLiteral("No VPN")))
        r.summary = QStringLiteral("No VPN");
    else if (scenario.startsWith(QStringLiteral("VPN detected")))
        r.summary = QStringLiteral("VPN detected");
    else if (scenario.startsWith(QStringLiteral("Approximate")))
        r.summary = QStringLiteral("Location est.");
    else
        r.summary = QStringLiteral("Uncertain");
    r.status = status;
    r.durationMs = t.elapsed();
    return r;
}

} // namespace G1G2G3Native
