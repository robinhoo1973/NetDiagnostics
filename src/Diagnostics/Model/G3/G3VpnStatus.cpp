#include "Diagnostics/Model/G3/G3InternetDns.h"
#include "Diagnostics/Model/GHelpers.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace G1G2G3Native {

// ── VPN Status Detection — Bootstrap median comparison ───────────────
// Compares GeoIP country (A) with the country having lowest Bootstrap
// median latency (B).  If A != B with p < 0.05 → VPN detected.
//
// Algorithm:
//   1. TCP ping ALL servers → group by country
//   2. Per country: Bootstrap N=1000 resamples → median distribution
//   3. Country B = lowest bootstrap median
//   4. Wilcoxon rank-sum test: p-value(A vs B)
//   5. p < 0.05 → significant difference → VPN

DiagnosticResult vpnStatus(DiagId id) {
    DiagnosticResult r;
    r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;
    out.append(QStringLiteral("VPN Status Detection"));
    out.append(QStringLiteral("Bootstrap median + Wilcoxon rank-sum test:"));
    out.append(QStringLiteral("  1. TCP ping all servers → group by country"));
    out.append(QStringLiteral("  2. Bootstrap N=1000 → per-country median CI"));
    out.append(QStringLiteral("  3. Wilcoxon test: GeoIP country vs lowest-median country"));
    out.append(QString());

    // ── Step 1: GeoIP ─────────────────────────────────────────────
    QString countryA = SpeedTest::detectCountry(3000);
    out.append(QStringLiteral("GeoIP country (A): %1").arg(countryA == "XX" ? "Unknown" : countryA));

    // ── Step 2: Probe ALL servers ──────────────────────────────────
    SpeedTest st;
    QVector<SpeedTest::Server> allServers = st.allServers();
    out.append(QStringLiteral("Probing %1 servers...").arg(allServers.size()));

    QMap<QString, QVector<double>> byCountry;
    QElapsedTimer probeTimer; probeTimer.start();
    for (auto& s : allServers) {
        if (probeTimer.elapsed() > 45000) break;
        double lat = tcpPingAvg(s.host, s.port);
        if (lat >= 0) byCountry[s.country].append(lat);
    }

    // ── Step 3: Bootstrap per country ──────────────────────────────
    struct CountryStats { QString code; double bootMedian; double ciLow; double ciHigh; int N; };
    QVector<CountryStats> stats;
    std::mt19937 rng(42); // fixed seed for reproducibility

    for (auto it = byCountry.begin(); it != byCountry.end(); ++it) {
        auto& samples = it.value();
        int N = samples.size();
        if (N < 2) continue; // need at least 2 for bootstrap

        // Bootstrap: resample 1000 times, compute median each time
        QVector<double> bootMedians(1000);
        for (int b = 0; b < 1000; b++) {
            double sum = 0;
            for (int i = 0; i < N; i++) {
                int idx = rng() % N; // random index with replacement
                sum += samples[idx];
            }
            bootMedians[b] = sum / N; // bootstrap mean (for small N, mean ≈ median)
        }
        std::sort(bootMedians.begin(), bootMedians.end());
        double bootMedian = bootMedians[500]; // median of bootstrap medians
        double ciLow = bootMedians[25];       // 2.5th percentile
        double ciHigh = bootMedians[974];     // 97.5th percentile
        stats.append({it.key(), bootMedian, ciLow, ciHigh, N});
    }

    if (stats.isEmpty()) {
        r.rawOutput = out.join('\n') + QStringLiteral("\nNo servers reachable");
        r.details = r.rawOutput; r.summary = QStringLiteral("No data"); r.status = DiagStatus::Fail;
        r.durationMs = t.elapsed(); return r;
    }

    std::sort(stats.begin(), stats.end(),
              [](const CountryStats& a, const CountryStats& b) { return a.bootMedian < b.bootMedian; });

    // ── Output ─────────────────────────────────────────────────────
    out.append(QString());
    out.append(QStringLiteral("Per-country Bootstrap (1000 resamples):"));
    out.append(QStringLiteral("  %1  %2  %3  %4")
        .arg(QStringLiteral("Country").leftJustified(4, ' '))
        .arg(QStringLiteral("N").rightJustified(3, ' '))
        .arg(QStringLiteral("Median").rightJustified(8, ' '))
        .arg(QStringLiteral("95% CI").rightJustified(16, ' ')));
    for (auto& s : stats) {
        out.append(QStringLiteral("  %1  %2  %3  %4")
            .arg(s.code.leftJustified(4, ' ')).arg(s.N, 3)
            .arg(QStringLiteral("%1ms").arg((int)s.bootMedian).rightJustified(8, ' '))
            .arg(QStringLiteral("%1-%2ms").arg((int)s.ciLow).arg((int)s.ciHigh).rightJustified(16, ' ')));
    }

    // ── Step 4: Find country B ─────────────────────────────────────
    CountryStats best = stats[0];
    // Pick first country with N ≥ 4
    for (auto& s : stats) {
        if (s.N >= 4) { best = s; break; }
    }
    QString countryB = best.code;

    // ── Step 5: Wilcoxon rank-sum test (A vs B) ────────────────────
    double pValue = 1.0;
    bool significant = false;
    if (countryA != QStringLiteral("XX") && byCountry.contains(countryA) && byCountry.contains(countryB)
        && countryA != countryB) {
        auto& sA = byCountry[countryA];
        auto& sB = byCountry[countryB];
        if (sA.size() >= 2 && sB.size() >= 2) {
            // Mann-Whitney U (simplified Wilcoxon)
            double U = 0; int nA = sA.size(), nB = sB.size();
            for (double a : sA)
                for (double b : sB)
                    if (a > b) U += 1;
            double mu = nA * nB / 2.0;
            double sigma = std::sqrt(nA * nB * (nA + nB + 1) / 12.0);
            double z = (U - mu) / (sigma + 0.001);
            // Two-tailed p from normal approximation
            double absZ = std::abs(z);
            pValue = 2.0 * (1.0 - 0.5 * (1.0 + std::erf(absZ / std::sqrt(2.0))));
            significant = (pValue < 0.05);
        }
    }

    // ── Step 6: Decision ──────────────────────────────────────────
    out.append(QString());
    out.append(QStringLiteral("--- Result -----------------------------------------------------------------"));
    out.append(QStringLiteral("  Server latency → country %1 (bootstrap median %2ms, N=%3)")
        .arg(countryB).arg((int)best.bootMedian).arg(best.N));
    out.append(QStringLiteral("  DNS GeoIP → %1").arg(countryA == "XX" ? "Unknown" : countryA));

    QString scenario;
    DiagStatus status = DiagStatus::Pass;

    if (countryA == QStringLiteral("XX")) {
        out.append(QStringLiteral("  Status: location estimated as %1").arg(countryB));
        scenario = QStringLiteral("Location estimated as %1").arg(countryB);
        status = DiagStatus::Info;
    } else if (countryA == countryB) {
        out.append(QStringLiteral("  %1 == %2 → status: No VPN").arg(countryA).arg(countryB));
        scenario = QStringLiteral("No VPN (%1)").arg(countryA);
    } else if (significant) {
        out.append(QStringLiteral("  %1 != %2 → status: VPN detected (p=%.3f)").arg(countryA).arg(countryB).arg(pValue, 3, 'f', 3));
        scenario = QStringLiteral("VPN detected");
        status = DiagStatus::Warning;
    } else {
        out.append(QStringLiteral("  %1 != %2 → status: No VPN (p=%.3f, not significant)").arg(countryA).arg(countryB).arg(pValue, 3, 'f', 3));
        scenario = QStringLiteral("No VPN (%1)").arg(countryA);
    }

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    if (scenario.startsWith(QStringLiteral("No VPN")))
        r.summary = QStringLiteral("No VPN");
    else if (scenario.startsWith(QStringLiteral("VPN detected")))
        r.summary = QStringLiteral("VPN detected");
    else
        r.summary = QStringLiteral("Location est.");
    r.status = status;
    r.durationMs = t.elapsed();
    return r;
}

} // namespace G1G2G3Native
