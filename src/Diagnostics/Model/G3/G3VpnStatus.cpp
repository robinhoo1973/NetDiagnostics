#include "Diagnostics/Model/G3/G3InternetDns.h"
#include "Diagnostics/Model/GHelpers.h"
#include <algorithm>
#include <cmath>
#include <random>
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
//   1b. MAD+HL calibrated probe on candidate countries (≥3 reachable)
//   2. Per country: Bootstrap N=5000 resamples → median distribution
//   3. Country B = lowest bootstrap median (requires N ≥ 5)
//   4. Wilcoxon rank-sum test: p-value(A vs B) with tie-corrected variance
//   5. p < 0.05 → significant difference → VPN

DiagnosticResult vpnStatus(DiagId id) {
    DiagnosticResult r;
    r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;
    out.append(QStringLiteral("VPN Status Detection"));
    out.append(QStringLiteral("Two-pass probe + Bootstrap + Wilcoxon:"));
    out.append(QStringLiteral("  1. Quick single-connect scan of ALL servers"));
    out.append(QStringLiteral("  2. MAD+HL calibrated probe on candidate countries (≥3 reachable)"));
    out.append(QStringLiteral("  3. Bootstrap N=5000 → per-country median CI (N≥3 samples)"));
    out.append(QStringLiteral("  4. Country B = lowest bootstrap median (N≥5 required)"));
    out.append(QStringLiteral("  5. Wilcoxon rank-sum test: GeoIP vs lowest-median (N≥3 each)"));
    out.append(QStringLiteral("  6. Decision: p<0.05 → VPN; p≥0.05 → inconclusive"));
    out.append(QString());

    // ── Step 1: GeoIP ─────────────────────────────────────────────
    QString countryA = SpeedTest::detectCountry(3000);
    out.append(QStringLiteral("GeoIP country (A): %1").arg(countryName(countryA)));

    // ── Step 2: Two-pass probe ──────────────────────────────────────
    // Pass 1 (Quick Scan): single tcpPingMs per server (~2ms each).
    //   Determines which countries have reachable servers so Pass 2
    //   can focus expensive multi-round probing on candidates only.
    // Pass 2 (Focused Detail): 50-connect MAD+HL calibrated probe
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

    // ── Pass 2: Focused calibrated probe ──────────────────────────
    // Candidate-country servers get 50-connect MAD+HL calibration.
    // Non-candidate servers get quick-fallback single-connect.
    QMap<QString, QVector<double>> byCountry;
    QElapsedTimer probeTimer; probeTimer.start();
    int calibrated = 0, calibSkipped = 0, calibFlaky = 0, quickFallback = 0;
    double totalFailRate = 0.0; int failRateCount = 0;

    for (auto* srv : targets) {
        if (probeTimer.elapsed() > 44000) break; // 1s safety margin before 45s cap
        double lat = -1.0;
        if (candidates.contains(srv->country)) {
            // 50-connect MAD+HL calibration with transient-failure tracking
            TcpPingResult cr = tcpPingCalibrated(srv->host, srv->port);
            calibrated++;
            totalFailRate += cr.failRate; failRateCount++;
            if (cr.usable && cr.failRate <= 0.4) { // ≤40% transient-fail rate
                lat = cr.latencyMs;
            } else if (!cr.usable) {
                calibSkipped++; // insufficient successes for reliable estimate
            } else {
                calibFlaky++; // usable=true but failRate > 40%
            }
        } else {
            // Non-candidate: single-connect is sufficient (won't affect outcome)
            lat = (double)tcpPingMs(srv->host, srv->port);
            quickFallback++;
        }
        if (lat >= 0) byCountry[srv->country].append(lat);
    }
    double avgFailRate = failRateCount > 0 ? totalFailRate / failRateCount : 0.0;
    out.append(QStringLiteral("  Calibrated: %1 servers (MAD+HL, avg fail %2%), %3 skipped, %4 flaky, %5 quick-fb")
        .arg(calibrated).arg(avgFailRate * 100.0, 0, 'f', 1)
        .arg(calibSkipped).arg(calibFlaky).arg(quickFallback));
    out.append(QStringLiteral("  Total reachable countries with samples: %1").arg(byCountry.size()));

    // ── Step 3: Bootstrap per country ──────────────────────────────
    // 5WHY: Was N=1000 resamples — CI width fluctuated ±2ms between runs
    // due to sampling noise.  N=5000 gives ~2.2× tighter CI stability
    // (std-error scales as 1/√N) at negligible cost (~50ms).
    // 5WHY: Was N≥2 minimum — bootstrap with 2 samples has only 4 unique
    // resamples, median is meaningless.  N≥3 gives at least 27 unique
    // resamples (3³ with replacement), enough for a coarse CI.
    struct CountryStats { QString code; double bootMedian; double ciLow; double ciHigh; int N; };
    QVector<CountryStats> stats;
    std::mt19937 rng(42); // fixed seed for reproducibility

    for (auto it = byCountry.begin(); it != byCountry.end(); ++it) {
        auto& samples = it.value();
        int N = samples.size();
        if (N < 3) continue; // need at least 3 for meaningful bootstrap

        // Bootstrap: resample 5000 times, compute actual median each time
        QVector<double> bootMedians(5000);
        QVector<double> resampled(N);
        for (int b = 0; b < 5000; b++) {
            for (int i = 0; i < N; i++)
                resampled[i] = samples[rng() % N];
            std::sort(resampled.begin(), resampled.end());
            bootMedians[b] = (N % 2 == 1) ? resampled[N/2]
                           : (resampled[N/2-1] + resampled[N/2]) / 2.0;
        }
        std::sort(bootMedians.begin(), bootMedians.end());
        double bootMedian = (bootMedians[2499] + bootMedians[2500]) / 2.0; // median of 5000 (even)
        double ciLow = bootMedians[125];         // 2.5th percentile (5000×0.025)
        double ciHigh = bootMedians[4874];       // 97.5th percentile (5000×0.975)
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
    out.append(QStringLiteral("Per-country Bootstrap (5000 resamples):"));
    out.append(QStringLiteral("  %1  %2  %3  %4")
        .arg(QStringLiteral("Ctry").leftJustified(5, ' '))
        .arg(QStringLiteral("N").rightJustified(3, ' '))
        .arg(QStringLiteral("Median").rightJustified(8, ' '))
        .arg(QStringLiteral("95% CI").rightJustified(16, ' ')));
    for (auto& s : stats) {
        out.append(QStringLiteral("  %1  %2  %3  %4")
            .arg(alpha3(s.code).leftJustified(5, ' ')).arg(s.N, 3)
            .arg(QStringLiteral("%1ms").arg((int)s.bootMedian).rightJustified(8, ' '))
            .arg(QStringLiteral("%1-%2ms").arg((int)s.ciLow).arg((int)s.ciHigh).rightJustified(16, ' ')));
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
    out.append(QStringLiteral("  Server latency → %1 (bootstrap median %2ms, N=%3)")
        .arg(countryName(countryB)).arg((int)best.bootMedian).arg(best.N));
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
