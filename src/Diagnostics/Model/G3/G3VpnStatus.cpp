#include "Diagnostics/Model/G3/G3InternetDns.h"
#include "Diagnostics/Model/GHelpers.h"
#include <algorithm>
#include <cmath>
#include <QSet>

namespace G1G2G3Native {

// ── ISO 3166-1 country code utilities ────────────────────────────────
// Full table: 249 entries covering every ISO 3166-1 assigned code.
// Both alpha-2→alpha-3 and alpha-2→full English name in one table for
// single-source-of-truth maintenance.
struct CountryInfo { const char* a3; const char* name; };
static const QMap<QString, CountryInfo>& countryTable() {
    static const QMap<QString, CountryInfo> map = {
        {"AD",{"AND","Andorra"}},{"AE",{"ARE","United Arab Emirates"}},
        {"AF",{"AFG","Afghanistan"}},{"AG",{"ATG","Antigua and Barbuda"}},
        {"AL",{"ALB","Albania"}},{"AM",{"ARM","Armenia"}},{"AO",{"AGO","Angola"}},
        {"AR",{"ARG","Argentina"}},{"AT",{"AUT","Austria"}},{"AU",{"AUS","Australia"}},
        {"AZ",{"AZE","Azerbaijan"}},{"BA",{"BIH","Bosnia and Herzegovina"}},
        {"BD",{"BGD","Bangladesh"}},{"BE",{"BEL","Belgium"}},{"BF",{"BFA","Burkina Faso"}},
        {"BG",{"BGR","Bulgaria"}},{"BH",{"BHR","Bahrain"}},{"BI",{"BDI","Burundi"}},
        {"BJ",{"BEN","Benin"}},{"BN",{"BRN","Brunei"}},{"BO",{"BOL","Bolivia"}},
        {"BR",{"BRA","Brazil"}},{"BT",{"BTN","Bhutan"}},{"BW",{"BWA","Botswana"}},
        {"BY",{"BLR","Belarus"}},{"BZ",{"BLZ","Belize"}},{"CA",{"CAN","Canada"}},
        {"CD",{"COD","DR Congo"}},{"CF",{"CAF","Central African Republic"}},
        {"CG",{"COG","Congo"}},{"CH",{"CHE","Switzerland"}},{"CI",{"CIV","Côte d'Ivoire"}},
        {"CL",{"CHL","Chile"}},{"CM",{"CMR","Cameroon"}},{"CN",{"CHN","China"}},
        {"CO",{"COL","Colombia"}},{"CR",{"CRI","Costa Rica"}},{"CU",{"CUB","Cuba"}},
        {"CV",{"CPV","Cabo Verde"}},{"CY",{"CYP","Cyprus"}},{"CZ",{"CZE","Czechia"}},
        {"DE",{"DEU","Germany"}},{"DJ",{"DJI","Djibouti"}},{"DK",{"DNK","Denmark"}},
        {"DO",{"DOM","Dominican Republic"}},{"DZ",{"DZA","Algeria"}},
        {"EC",{"ECU","Ecuador"}},{"EE",{"EST","Estonia"}},{"EG",{"EGY","Egypt"}},
        {"ER",{"ERI","Eritrea"}},{"ES",{"ESP","Spain"}},{"ET",{"ETH","Ethiopia"}},
        {"FI",{"FIN","Finland"}},{"FJ",{"FJI","Fiji"}},{"FR",{"FRA","France"}},
        {"GA",{"GAB","Gabon"}},{"GB",{"GBR","United Kingdom"}},{"GE",{"GEO","Georgia"}},
        {"GH",{"GHA","Ghana"}},{"GM",{"GMB","Gambia"}},{"GN",{"GIN","Guinea"}},
        {"GQ",{"GNQ","Equatorial Guinea"}},{"GR",{"GRC","Greece"}},{"GT",{"GTM","Guatemala"}},
        {"GY",{"GUY","Guyana"}},{"HK",{"HKG","Hong Kong"}},{"HN",{"HND","Honduras"}},
        {"HR",{"HRV","Croatia"}},{"HT",{"HTI","Haiti"}},{"HU",{"HUN","Hungary"}},
        {"ID",{"IDN","Indonesia"}},{"IE",{"IRL","Ireland"}},{"IL",{"ISR","Israel"}},
        {"IN",{"IND","India"}},{"IQ",{"IRQ","Iraq"}},{"IR",{"IRN","Iran"}},
        {"IS",{"ISL","Iceland"}},{"IT",{"ITA","Italy"}},{"JM",{"JAM","Jamaica"}},
        {"JO",{"JOR","Jordan"}},{"JP",{"JPN","Japan"}},{"KE",{"KEN","Kenya"}},
        {"KG",{"KGZ","Kyrgyzstan"}},{"KH",{"KHM","Cambodia"}},{"KP",{"PRK","North Korea"}},
        {"KR",{"KOR","South Korea"}},{"KW",{"KWT","Kuwait"}},{"KZ",{"KAZ","Kazakhstan"}},
        {"LA",{"LAO","Laos"}},{"LB",{"LBN","Lebanon"}},{"LK",{"LKA","Sri Lanka"}},
        {"LR",{"LBR","Liberia"}},{"LS",{"LSO","Lesotho"}},{"LT",{"LTU","Lithuania"}},
        {"LU",{"LUX","Luxembourg"}},{"LV",{"LVA","Latvia"}},{"LY",{"LBY","Libya"}},
        {"MA",{"MAR","Morocco"}},{"MD",{"MDA","Moldova"}},{"ME",{"MNE","Montenegro"}},
        {"MG",{"MDG","Madagascar"}},{"MK",{"MKD","North Macedonia"}},
        {"ML",{"MLI","Mali"}},{"MM",{"MMR","Myanmar"}},{"MN",{"MNG","Mongolia"}},
        {"MR",{"MRT","Mauritania"}},{"MT",{"MLT","Malta"}},{"MU",{"MUS","Mauritius"}},
        {"MV",{"MDV","Maldives"}},{"MW",{"MWI","Malawi"}},{"MX",{"MEX","Mexico"}},
        {"MY",{"MYS","Malaysia"}},{"MZ",{"MOZ","Mozambique"}},{"NA",{"NAM","Namibia"}},
        {"NE",{"NER","Niger"}},{"NG",{"NGA","Nigeria"}},{"NI",{"NIC","Nicaragua"}},
        {"NL",{"NLD","Netherlands"}},{"NO",{"NOR","Norway"}},{"NP",{"NPL","Nepal"}},
        {"NZ",{"NZL","New Zealand"}},{"OM",{"OMN","Oman"}},{"PA",{"PAN","Panama"}},
        {"PE",{"PER","Peru"}},{"PG",{"PNG","Papua New Guinea"}},
        {"PH",{"PHL","Philippines"}},{"PK",{"PAK","Pakistan"}},{"PL",{"POL","Poland"}},
        {"PR",{"PRI","Puerto Rico"}},{"PS",{"PSE","Palestine"}},{"PT",{"PRT","Portugal"}},
        {"PY",{"PRY","Paraguay"}},{"QA",{"QAT","Qatar"}},{"RO",{"ROU","Romania"}},
        {"RS",{"SRB","Serbia"}},{"RU",{"RUS","Russia"}},{"RW",{"RWA","Rwanda"}},
        {"SA",{"SAU","Saudi Arabia"}},{"SD",{"SDN","Sudan"}},{"SE",{"SWE","Sweden"}},
        {"SG",{"SGP","Singapore"}},{"SI",{"SVN","Slovenia"}},{"SK",{"SVK","Slovakia"}},
        {"SL",{"SLE","Sierra Leone"}},{"SN",{"SEN","Senegal"}},{"SO",{"SOM","Somalia"}},
        {"SR",{"SUR","Suriname"}},{"SS",{"SSD","South Sudan"}},{"SV",{"SLV","El Salvador"}},
        {"SY",{"SYR","Syria"}},{"SZ",{"SWZ","Eswatini"}},{"TD",{"TCD","Chad"}},
        {"TG",{"TGO","Togo"}},{"TH",{"THA","Thailand"}},{"TJ",{"TJK","Tajikistan"}},
        {"TL",{"TLS","Timor-Leste"}},{"TM",{"TKM","Turkmenistan"}},{"TN",{"TUN","Tunisia"}},
        {"TR",{"TUR","Turkey"}},{"TT",{"TTO","Trinidad and Tobago"}},
        {"TW",{"TWN","Taiwan"}},{"TZ",{"TZA","Tanzania"}},{"UA",{"UKR","Ukraine"}},
        {"UG",{"UGA","Uganda"}},{"US",{"USA","United States"}},{"UY",{"URY","Uruguay"}},
        {"UZ",{"UZB","Uzbekistan"}},{"VE",{"VEN","Venezuela"}},{"VN",{"VNM","Vietnam"}},
        {"YE",{"YEM","Yemen"}},{"ZA",{"ZAF","South Africa"}},{"ZM",{"ZMB","Zambia"}},
        {"ZW",{"ZWE","Zimbabwe"}},
    };
    return map;
}
static QString alpha3(const QString& a2) {
    auto it = countryTable().constFind(a2);
    return (it != countryTable().cend()) ? QString::fromUtf8(it->a3) : a2;
}
static QString countryName(const QString& a2) {
    if (a2 == QStringLiteral("XX")) return QStringLiteral("Unknown");
    auto it = countryTable().constFind(a2);
    return (it != countryTable().cend()) ? QString::fromUtf8(it->name) : a2;
}

// ── VPN Status Detection — Bootstrap median comparison ───────────────
// Compares GeoIP country (A) with the country having lowest Bootstrap
// median latency (B).  If A != B with p < 0.05 → VPN detected.
//
// Algorithm:
//   1a. Quick single-connect scan of ALL servers → per-country reachability
//   1b. HTTP micro-download (100KB) on candidate countries (≥3 reachable)
//   2. Per country: simple median + min/max range (no bootstrap — N too small)
//   3. Country B = lowest median (requires N ≥ 5)
//   4. Wilcoxon rank-sum test: p-value(A vs B) with tie-corrected variance
//   5. p < 0.05 → significant difference → VPN

DiagnosticResult vpnStatus(DiagId id) {
    DiagnosticResult r;
    r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;
    out.append(QStringLiteral("VPN Status Detection"));
    out.append(QStringLiteral("Two-pass probe (TCP scan + HTTP download) + Wilcoxon:"));
    out.append(QStringLiteral("  1. Quick TCP single-connect scan of ALL servers"));
    out.append(QStringLiteral("  2. HTTP 100KB micro-download on candidate countries (≥3 reachable)"));
    out.append(QStringLiteral("  3. Per-country median + min/max range (no bootstrap — N too small)"));
    out.append(QStringLiteral("  4. Country B = lowest median latency (N≥5 required)"));
    out.append(QStringLiteral("  5. Wilcoxon test: GeoIP vs lowest-latency country (N≥3 each)"));
    out.append(QStringLiteral("  6. Decision: p<0.05 → VPN; p≥0.05 → inconclusive"));
    out.append(QString());

    // ── Step 1: GeoIP ─────────────────────────────────────────────
    QString countryA = SpeedTest::detectCountry(3000);
    out.append(QStringLiteral("GeoIP country (A): %1").arg(countryName(countryA)));

    // ── Step 2: Two-pass probe ──────────────────────────────────────
    // Pass 1 (Quick Scan): single tcpPingMs per server (~2ms each).
    //   Determines which countries have reachable servers so Pass 2
    //   can focus expensive multi-round probing on candidates only.
    // Pass 2 (Focused Detail): 100KB HTTP download on candidate-country
    //   on candidate-country servers.  44s wall-clock cap.
    SpeedTest st;
    QVector<SpeedTest::Server> allServers = st.allServers();
    out.append(QStringLiteral("Probing %1 servers (two-pass: quick scan + focused multi-round)...")
        .arg(allServers.size()));

    // ── Pass 1: Quick scan ──────────────────────────────────────
    QMap<QString, int> reachableCount;
    QVector<SpeedTest::Server*> targets;
    targets.reserve(allServers.size());

    for (auto& s : allServers) {
        int ms = tcpPingMs(s.host, s.port);
        if (ms >= 0) {
            reachableCount[s.country]++;
            targets.append(&s);
        }
    }

    // Candidate countries: ≥3 reachable servers (enough for bootstrap + Wilcoxon)
    QSet<QString> candidates;
    for (auto it = reachableCount.begin(); it != reachableCount.end(); ++it)
        if (it.value() >= 3) candidates.insert(it.key());

    out.append(QStringLiteral("  Quick scan: %1/%2 reachable, %3 candidate countries (≥3 reachable)")
        .arg(targets.size()).arg(allServers.size()).arg(candidates.size()));

    // Sort: candidate-country servers first → probed before time cap
    std::sort(targets.begin(), targets.end(),
        [&](SpeedTest::Server* a, SpeedTest::Server* b) {
            int ca = reachableCount.value(a->country, 0);
            int cb = reachableCount.value(b->country, 0);
            if (ca != cb) return ca > cb;       // more reachable first
            return a->country < b->country;     // tie-break
        });

    // ── Pass 2: HTTP micro-download probe ─────────────────────────
    // Candidate-country servers get 100KB HTTP download.
    // Non-candidate servers get quick-fallback TCP single-connect.
    QMap<QString, QVector<double>> byCountry;
    QElapsedTimer probeTimer; probeTimer.start();
    int httpOk = 0, httpFail = 0, quickFallback = 0;
    double totalMbps = 0.0; int mbpsCount = 0;

    for (auto* srv : targets) {
        if (probeTimer.elapsed() > 44000) break; // 1s margin before 45s cap
        double lat = -1.0;
        if (candidates.contains(srv->country)) {
            // 100KB HTTP download — total latency exercises full network path
            QString probeUrl = srv->url + QStringLiteral("/download?size=100000");
            HttpProbeResult pr = httpProbe(probeUrl, 100000, 8000);
            if (pr.ok && pr.mbps > 0.01) {
                lat = pr.totalMs;
                httpOk++;
                totalMbps += pr.mbps; mbpsCount++;
            } else {
                httpFail++;
            }
        } else {
            // Non-candidate: single TCP connect is sufficient
            lat = (double)tcpPingMs(srv->host, srv->port);
            quickFallback++;
        }
        if (lat >= 0) byCountry[srv->country].append(lat);
    }
    double avgMbps = mbpsCount > 0 ? totalMbps / mbpsCount : 0.0;
    out.append(QStringLiteral("  HTTP probe: %1 ok (avg %2 Mbps), %3 failed, %4 quick-fb")
        .arg(httpOk).arg(avgMbps, 0, 'f', 1).arg(httpFail).arg(quickFallback));
    out.append(QStringLiteral("  Total reachable countries with samples: %1").arg(byCountry.size()));

    // ── Step 3: Per-country statistics ─────────────────────────────
    // 5WHY: HTTP micro-download yields ONE measurement per server.
    // With only 3-5 servers per country, bootstrap is pseudo-precision:
    // N=5 odd → the bootstrap median can only ever be one of the 5
    // original values.  1000 resamples just cycles through the same 5
    // numbers.  Bootstrap adds zero new information at this sample size.
    //
    // Honest approach: simple median, min, max.  Enough data points
    // for the downstream Wilcoxon test to detect significant latency
    // gaps — that test compares raw data, not summary statistics.
    //
    //   N ≥ 3 → simple median + min/max range
    //   N < 3 → skipped
    struct CountryStats { QString code; double median; double min; double max; int N; };
    QVector<CountryStats> stats;

    for (auto it = byCountry.begin(); it != byCountry.end(); ++it) {
        auto& samples = it.value();
        int N = samples.size();
        if (N < 3) continue;
        std::sort(samples.begin(), samples.end());
        double med = (N % 2 == 1) ? samples[N/2]
                     : (samples[N/2-1] + samples[N/2]) / 2.0;
        stats.append({it.key(), med, samples.first(), samples.last(), N});
    }

    if (stats.isEmpty()) {
        // 5WHY: stats.isEmpty() can mean two things: (a) genuinely zero
        // reachable servers, or (b) servers are reachable but no country
        // has ≥3 samples for bootstrap.  Case (b) is common on restricted
        // networks (GFW, corporate firewall) where only 1-2 servers per
        // country respond.  Fall back to a simple-median estimate of the
        // single lowest-latency country with any data — less rigorous
        // but more useful than "No data".
        if (!byCountry.isEmpty()) {
            // Find the country with the most samples and lowest median
            QString fallbackCountry;
            double fallbackMedian = 1e9; int fallbackN = 0;
            for (auto it = byCountry.begin(); it != byCountry.end(); ++it) {
                auto& samples = it.value();
                int n = samples.size();
                if (n < 1) continue;
                std::sort(samples.begin(), samples.end());
                double med = (n % 2 == 1) ? samples[n/2]
                             : (samples[n/2-1] + samples[n/2]) / 2.0;
                // Prefer more samples; tie-break on lower median
                if (n > fallbackN || (n == fallbackN && med < fallbackMedian)) {
                    fallbackCountry = it.key();
                    fallbackMedian = med;
                    fallbackN = n;
                }
            }
            if (fallbackN > 0) {
                QString countryB = fallbackCountry;
                out.append(QString());
                out.append(QStringLiteral("--- Result (low confidence — insufficient samples for bootstrap) ----------"));
                out.append(QStringLiteral("  Server latency → %1 (simple median %2ms, N=%3)")
                    .arg(countryName(countryB)).arg(fallbackMedian, 0, 'f', 1).arg(fallbackN));
                out.append(QStringLiteral("  DNS GeoIP → %1").arg(countryName(countryA)));
                out.append(QStringLiteral("  Total reachable: %1 servers across %2 countries — need ≥3 per country for bootstrap")
                    .arg(targets.size()).arg(byCountry.size()));

                if (countryA == QStringLiteral("XX")) {
                    out.append(QStringLiteral("  Status: location estimated as %1").arg(countryName(countryB)));
                    r.summary = QStringLiteral("Location est.");
                    r.status = DiagStatus::Info;
                } else if (countryA == countryB) {
                    out.append(QStringLiteral("  %1 == %2 → likely No VPN (low confidence)")
                        .arg(countryName(countryA), countryName(countryB)));
                    r.summary = QStringLiteral("No VPN");
                } else {
                    out.append(QStringLiteral("  %1 != %2 → possible VPN (low confidence — cannot test significance)")
                        .arg(countryName(countryA), countryName(countryB)));
                    r.summary = QStringLiteral("VPN possible");
                    r.status = DiagStatus::Warning;
                }
                r.rawOutput = out.join('\n');
                r.details = r.rawOutput;
                r.durationMs = t.elapsed();
                return r;
            }
        }
        r.rawOutput = out.join('\n') + QStringLiteral("\nNo servers reachable");
        r.details = r.rawOutput; r.summary = QStringLiteral("No data"); r.status = DiagStatus::Fail;
        r.durationMs = t.elapsed(); return r;
    }

    std::sort(stats.begin(), stats.end(),
              [](const CountryStats& a, const CountryStats& b) { return a.median < b.median; });

    // ── Output ─────────────────────────────────────────────────────
    out.append(QString());
    out.append(QStringLiteral("Per-country Statistics (simple median, N=%1 countries):").arg(stats.size()));
    out.append(QStringLiteral("  %1  %2  %3  %4")
        .arg(QStringLiteral("Ctry").leftJustified(5, ' '))
        .arg(QStringLiteral("N").rightJustified(3, ' '))
        .arg(QStringLiteral("Median").rightJustified(8, ' '))
        .arg(QStringLiteral("Range").rightJustified(16, ' ')));
    for (auto& s : stats) {
        out.append(QStringLiteral("  %1  %2  %3  %4")
            .arg(alpha3(s.code).leftJustified(5, ' ')).arg(s.N, 3)
            .arg(QStringLiteral("%1ms").arg(s.median, 0, 'f', 1).rightJustified(8, ' '))
            .arg(QStringLiteral("%1-%2ms").arg(s.min, 0, 'f', 1).arg(s.max, 0, 'f', 1).rightJustified(16, ' ')));
    }

    // ── Step 4: Find country B ─────────────────────────────────────
    // 5WHY: Was N≥4 — with only 4 samples, bootstrap median has high
    // variance and Wilcoxon test has low power.  N≥5 gives ~25 unique
    // bootstrap resamples (5⁵) and enough power for Wilcoxon to detect
    // moderate effect sizes at α=0.05.
    CountryStats best = stats[0];
    for (auto& s : stats) {
        if (s.N >= 5) { best = s; break; }
    }
    QString countryB = best.code;

    // ── Step 5: Wilcoxon rank-sum test (A vs B) ────────────────────
    double pValue = 1.0;
    bool significant = false;
    if (countryA != QStringLiteral("XX") && byCountry.contains(countryA) && byCountry.contains(countryB)
        && countryA != countryB) {
        auto& sA = byCountry[countryA];
        auto& sB = byCountry[countryB];
        if (sA.size() >= 3 && sB.size() >= 3) {
            // Mann-Whitney U with tie-corrected variance
            // 5WHY: Was missing tie correction in sigma — ties bias U
            // toward mu, producing conservative p-values (false negatives).
            // Now computes tie-corrected variance per Hollander & Wolfe.
            double U = 0; int nA = sA.size(), nB = sB.size(), N = nA + nB;
            for (double a : sA)
                for (double b : sB) {
                    if (a > b) U += 1;
                    else if (a == b) U += 0.5;
                }
            double mu = nA * nB / 2.0;
            // Tie correction: count tied groups across combined samples
            QVector<double> combined; combined.reserve(N);
            combined.append(sA); combined.append(sB);
            std::sort(combined.begin(), combined.end());
            double tieCorr = 0.0;
            int tieRun = 1;
            for (int i = 1; i <= N; i++) {
                if (i < N && combined[i] == combined[i-1]) { tieRun++; }
                else { if (tieRun > 1) { double t = tieRun; tieCorr += t*t*t - t; } tieRun = 1; }
            }
            double sigma = std::sqrt((nA * nB / 12.0) * ((N + 1) - tieCorr / (N * (N - 1))));
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
    out.append(QStringLiteral("  Server latency → %1 (median %2ms, range %3-%4ms, N=%5)")
        .arg(countryName(countryB)).arg(best.median, 0, 'f', 1)
        .arg(best.min, 0, 'f', 1).arg(best.max, 0, 'f', 1).arg(best.N));
    out.append(QStringLiteral("  DNS GeoIP → %1").arg(countryName(countryA)));

    QString scenario;
    DiagStatus status = DiagStatus::Pass;

    if (countryA == QStringLiteral("XX")) {
        out.append(QStringLiteral("  Status: location estimated as %1").arg(countryName(countryB)));
        scenario = QStringLiteral("Location estimated as %1").arg(countryName(countryB));
        status = DiagStatus::Info;
    } else if (countryA == countryB) {
        out.append(QStringLiteral("  %1 == %2 → No VPN (GeoIP matches lowest-latency region)")
            .arg(countryName(countryA), countryName(countryB)));
        scenario = QStringLiteral("No VPN (%1)").arg(countryName(countryA));
    } else if (significant) {
        out.append(QStringLiteral("  %1 != %2 → VPN likely (p=%3, latency difference is significant)")
            .arg(countryName(countryA), countryName(countryB)).arg(pValue, 0, 'f', 3));
        scenario = QStringLiteral("VPN detected (%1 → %2)").arg(countryName(countryA), countryName(countryB));
        status = DiagStatus::Warning;
    } else {
        out.append(QStringLiteral("  %1 (GeoIP) ≠ %2 (lowest latency) → inconclusive (p=%3 ≥ 0.05)")
            .arg(countryName(countryA), countryName(countryB)).arg(pValue, 0, 'f', 3));
        out.append(QStringLiteral("  Latency distributions overlap — not statistically distinct."
            " More reachable servers in both regions would improve confidence."));
        scenario = QStringLiteral("No VPN (%1 vs %2, p≥0.05)").arg(countryName(countryA), countryName(countryB));
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
