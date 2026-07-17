#include "Diagnostics/Model/G3/G3InternetDns.h"
#include "Diagnostics/Model/GHelpers.h"
#include <algorithm>
#include <cmath>

namespace G1G2G3Native {

// ── VPN Status Detection — statistical country-agnostic ──────────────
// 5WHY: Previous approach only probed 3 hand-picked countries (GeoIP, CN,
// US).  A user in Japan behind Australia VPN would have zero Australian
// probes and be misclassified.  Now probes ALL speed-test servers from
// ALL countries, computes per-country median latency, and compares the
// lowest-median country (B) with GeoIP country (A).  If A ≠ B → VPN.
//
// Statistical rationale: within a country, server latencies form a
// cluster.  The median is robust against outliers (unreachable servers
// or abnormally slow ones).  The country with the lowest median latency
// and sufficient sample size is the most likely physical location.

DiagnosticResult vpnStatus(DiagId id) {
    DiagnosticResult r;
    r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;
    out.append(QStringLiteral("VPN Status Detection"));
    out.append(QStringLiteral("Compares GeoIP country with statistical latency clustering:"));
    out.append(QStringLiteral("  1. GeoIP → apparent country (A)"));
    out.append(QStringLiteral("  2. TCP ping ALL speed-test servers grouped by country"));
    out.append(QStringLiteral("  3. Per-country median latency → connectivity country (B)"));
    out.append(QStringLiteral("  4. A == B → No VPN.  A != B → VPN."));
    out.append(QString());

    // ── Step 1: GeoIP → country A ──────────────────────────────────
    QString countryA = SpeedTest::detectCountry(3000);
    out.append(QStringLiteral("GeoIP country (A): %1").arg(countryA == "XX" ? "Unknown" : countryA));

    // ── Step 2: Probe ALL servers, group by country ─────────────────
    SpeedTest st;
    QVector<SpeedTest::Server> allServers = st.allServers();
    out.append(QString());
    out.append(QStringLiteral("Probing %1 speed-test servers across all countries...").arg(allServers.size()));
    out.append(QString());

    // Per-server: record (country, latency).  Use tcpPingAvg for sub-ms precision.
    struct ProbeResult { QString country; double latencyMs; };
    QVector<ProbeResult> probeResults;
    QElapsedTimer probeTimer; probeTimer.start();

    for (auto& s : allServers) {
        if (probeTimer.elapsed() > 45000) break; // 45s budget for probes
        double lat = tcpPingAvg(s.host, s.port);
        if (lat >= 0) {
            probeResults.append({s.country, lat});
        }
    }

    if (probeResults.isEmpty()) {
        r.rawOutput = out.join('\n') + QStringLiteral("\nAll servers unreachable — cannot determine VPN status");
        r.details = r.rawOutput;
        r.summary = QStringLiteral("No connectivity");
        r.status = DiagStatus::Fail;
        r.durationMs = t.elapsed();
        return r;
    }

    // ── Step 3: Per-country statistics ──────────────────────────────
    // Group by country, compute median, min, count
    struct CountryStats { QString code; double median; double minLat; int count; };
    QMap<QString, QVector<double>> byCountry;
    for (auto& pr : probeResults)
        byCountry[pr.country].append(pr.latencyMs);

    QVector<CountryStats> stats;
    for (auto it = byCountry.begin(); it != byCountry.end(); ++it) {
        auto& lats = it.value();
        std::sort(lats.begin(), lats.end());
        int n = lats.size();
        // Median: middle value (or average of two middle if even)
        double median = (n % 2 == 1) ? lats[n/2] : (lats[n/2-1] + lats[n/2]) / 2.0;
        stats.append({it.key(), median, lats[0], n});
    }

    // Sort by median ascending → lowest-latency country first
    std::sort(stats.begin(), stats.end(),
              [](const CountryStats& a, const CountryStats& b) { return a.median < b.median; });

    // ── Step 4: Determine connectivity country B ────────────────────
    // Pick the first country with ≥2 reachable servers
    CountryStats countryBStats;
    bool foundB = false;
    for (auto& s : stats) {
        if (s.count >= 2) {
            countryBStats = s;
            foundB = true;
            break;
        }
    }
    if (!foundB && !stats.isEmpty()) {
        // Fallback: use any country with at least 1 server
        countryBStats = stats[0];
        foundB = true;
    }
    QString countryB = foundB ? countryBStats.code : QString();

    // ── Output per-country table ────────────────────────────────────
    out.append(QStringLiteral("Per-country latency statistics (sorted by median):"));
    out.append(QStringLiteral("  %1  %2  %3  %4  %5")
        .arg(QStringLiteral("Country").leftJustified(4, ' '))
        .arg(QStringLiteral("Count").rightJustified(5, ' '))
        .arg(QStringLiteral("Median").rightJustified(8, ' '))
        .arg(QStringLiteral("Min").rightJustified(6, ' '))
        .arg(QStringLiteral("Range").rightJustified(10, ' ')));
    for (auto& s : stats) {
        double maxLat = byCountry[s.code].last();
        out.append(QStringLiteral("  %1  %2  %3  %4  %5")
            .arg(s.code.leftJustified(4, ' '))
            .arg(s.count, 5)
            .arg(QStringLiteral("%1ms").arg((int)s.median).rightJustified(8, ' '))
            .arg(QStringLiteral("%1ms").arg((int)s.minLat).rightJustified(6, ' '))
            .arg(QStringLiteral("%1-%2ms").arg((int)s.minLat).arg((int)maxLat).rightJustified(10, ' ')));
    }

    // ── Step 5: Decision ────────────────────────────────────────────
    out.append(QString());
    out.append(QStringLiteral("--- Result -----------------------------------------------------------------"));

    QString scenario;
    DiagStatus status = DiagStatus::Pass;

    if (countryA == QStringLiteral("XX") && foundB) {
        out.append(QStringLiteral("  Server latency → country %1 (median %2ms, %3 servers)")
            .arg(countryB).arg((int)countryBStats.median).arg(countryBStats.count));
        out.append(QStringLiteral("  DNS GeoIP → unavailable"));
        out.append(QStringLiteral("  Status: location estimated as %1").arg(countryB));
        scenario = QStringLiteral("Location estimated as %1").arg(countryB);
        status = DiagStatus::Info;
    } else if (!foundB) {
        out.append(QStringLiteral("  Server latency → no servers reachable"));
        out.append(QStringLiteral("  DNS GeoIP → %1").arg(countryA));
        out.append(QStringLiteral("  Status: insufficient data"));
        scenario = QStringLiteral("Insufficient data");
        status = DiagStatus::Fail;
    } else if (countryA == countryB) {
        out.append(QStringLiteral("  Server latency → country %1 (median %2ms, %3 servers)")
            .arg(countryB).arg((int)countryBStats.median).arg(countryBStats.count));
        out.append(QStringLiteral("  DNS GeoIP → country %1").arg(countryA));
        out.append(QStringLiteral("  %1 == %2 → status: No VPN").arg(countryA).arg(countryB));
        scenario = QStringLiteral("No VPN (%1)").arg(countryA);
    } else {
        out.append(QStringLiteral("  Server latency → country %1 (median %2ms, %3 servers)")
            .arg(countryB).arg((int)countryBStats.median).arg(countryBStats.count));
        out.append(QStringLiteral("  DNS GeoIP → country %1").arg(countryA));
        out.append(QStringLiteral("  %1 != %2 → status: VPN detected").arg(countryA).arg(countryB));
        scenario = QStringLiteral("VPN detected");
        status = DiagStatus::Warning;
    }

    if (countryA != countryB && countryA != QStringLiteral("XX") && foundB) {
        // Add detail line explaining the mismatch
        out.append(QStringLiteral("  (%1 servers in %2 have %3ms median; %4 is the lowest)")
            .arg(countryBStats.count).arg(countryB).arg((int)countryBStats.median).arg(countryB));
    }

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;

    // User-friendly summary
    if (scenario.startsWith(QStringLiteral("No VPN")))
        r.summary = QStringLiteral("No VPN");
    else if (scenario.startsWith(QStringLiteral("VPN detected")))
        r.summary = QStringLiteral("VPN detected");
    else if (scenario.startsWith(QStringLiteral("GeoIP")))
        r.summary = QStringLiteral("Location est.");
    else if (scenario.startsWith(QStringLiteral("Borderline")))
        r.summary = QStringLiteral("Borderline");
    else
        r.summary = QStringLiteral("Uncertain");
    r.status = status;
    r.durationMs = t.elapsed();
    return r;
}

} // namespace G1G2G3Native
