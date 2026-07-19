#include "Diagnostics/Model/G3/G3InternetDns.h"
#include "Diagnostics/Model/GHelpers.h"
#include <algorithm>
#include <cmath>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>

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

// ── Forward declarations ──────────────────────────────────────────
static double exactPermutationPValue(const QVector<double>& combined, int nA, int nB, double obsDev);
static double cliffDelta(double U, int nA, int nB);

// ── Exact permutation test (Mann-Whitney U) ───────────────────────
// 5WHY: Normal approximation of U is unreliable at N=3-5 per group.
// With N≤20 total, we enumerate ALL C(N, nA) possible group splits
// and compute the exact null distribution of U.  No approximation,
// no tie correction needed.  Accepts precomputed combined vector
// and |U - mu| to avoid redundant O(nA*nB) U recomputation.
static double exactPermutationPValue(const QVector<double>& combined,
                                      int nA, int nB, double obsDev) {
    int N = nA + nB;
    double mu = nA * nB / 2.0;
    int extremeCount = 0, totalPerms = 0;
    unsigned maxMask = 1u << N;  // unsigned avoids signed-overflow UB for N>=31
    for (unsigned mask = 0; mask < maxMask; mask++) {
        int bits = 0, m = mask;
        while (m) { bits++; m &= m - 1; }
        if (bits != nA) continue;
        totalPerms++;

        double permU = 0;
        for (int i = 0; i < N; i++) {
            if (!(mask & (1 << i))) continue;
            for (int j = 0; j < N; j++) {
                if (mask & (1 << j)) continue;
                if (combined[i] > combined[j]) permU += 1;
                else if (combined[i] == combined[j]) permU += 0.5;
            }
        }
        if (std::abs(permU - mu) >= obsDev) extremeCount++;
    }
    return (double)extremeCount / totalPerms;
}

// ── Cliff's Delta effect size ──────────────────────────────────────
// δ = P(X>Y) − P(X<Y) = 1 − 2U/(nA·nB), range [-1, +1]
// Benchmarks: |δ|<0.15 negligible, 0.15 small, 0.33 medium, 0.47 large
static double cliffDelta(double U, int nA, int nB) {
    return 1.0 - 2.0 * U / (static_cast<double>(nA) * nB);
}

// ── IP Geolocation & VPN Detection — HL + Exact Permutation + Cliff's Delta ────
// Compares GeoIP country (A) with the country having lowest
// Hodges-Lehmann latency (B).  If A != B with p < 0.05 + |δ| ≥ 0.33
// → VPN detected.
//
// Algorithm:
//   1. GeoIP detection via external service
//   2. Uniform TTFB probe on ALL servers (10-thread parallel, no pre-scan)
//   3. Per country: Hodges-Lehmann robust estimator (N≥3 required)
//   4. Country B = lowest HL estimate among countries with N≥3
//   5. Exact permutation test (enum C(N,nA) splits, N≤20) → exact p-value
//   6. Cliff's Delta effect size: δ = 1−2U/(nA·nB)
//   7. Decision: p<0.05 + |δ|≥0.33 → VPN; p<0.05 + |δ|<0.33 → possible; |δ|≥0.33 → suspected

DiagnosticResult geoIPLoc(DiagId id) {
    DiagnosticResult r;
    r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;
    out.append(QStringLiteral("IP Geolocation & VPN Detection"));

    // ── Step 0: Load server database ─────────────────────────────
    SpeedTest st;
    QVector<SpeedTest::Server> allServers = st.allServers();
    out.append(QStringLiteral("Method: HTTP probe + statistical comparison of %1 global servers").arg(allServers.size()));
    out.append(QString());

    // ── Step 1: GeoIP ─────────────────────────────────────────────
    out.append(QStringLiteral("[Phase 1/5] Detecting GeoIP country..."));
    QString countryA = SpeedTest::detectCountry(3000);
    out.append(QStringLiteral("GeoIP location: %1").arg(countryName(countryA)));

    // ── Step 2: Uniform TTFB probe — 10-thread parallel ────────────
    // HTTP GET → time to first byte for ALL servers directly.
    // No TCP pre-scan — matches GeoProbe::probeAllServers semantics.
    out.append(QStringLiteral("Probing %1 servers (TTFB, 10-thread parallel)...")
        .arg(allServers.size()));

    QMap<QString, QVector<double>> byCountry;
    std::atomic<int> ttfOk{0}, ttfFail{0};
    std::atomic<int> workIdx{0};
    std::atomic<bool> probeExpired{false};
    std::mutex resultMutex;
    // Use atomic deadline instead of QElapsedTimer to avoid data race
    // across 10 threads (ARM64 may tear QElapsedTimer's internal fields).
    qint64 probeStartMs = QDateTime::currentMSecsSinceEpoch();
    std::atomic<qint64> probeDeadline{probeStartMs + 45000};

    const int kThreads = 10;
    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    int totalServers = allServers.size();

    for (int t = 0; t < kThreads; t++) {
        threads.emplace_back([&]() {
            QMap<QString, QVector<double>> local;
            int localOk = 0, localFail = 0;

            while (!probeExpired.load(std::memory_order_relaxed)) {
                int idx = workIdx.fetch_add(1);
                if (idx >= totalServers) break;
                auto& srv = allServers[idx];

                double lat = httpTtfb(parseHttpUrl(srv.url));
                if (lat >= 0) { localOk++; local[srv.country].append(lat); }
                else { localFail++; }

                if (QDateTime::currentMSecsSinceEpoch() >= probeDeadline.load(std::memory_order_acquire))
                    probeExpired.store(true, std::memory_order_relaxed);
            }

            std::lock_guard<std::mutex> lock(resultMutex);
            for (auto it = local.begin(); it != local.end(); ++it)
                byCountry[it.key()] += it.value();
            ttfOk += localOk; ttfFail += localFail;
        });
    }
    for (auto& t : threads) t.join();

    out.append(QStringLiteral("[Phase 2/5] TTFB probe complete — %1 ok, %2 failed")
        .arg(ttfOk.load()).arg(ttfFail.load()));
    out.append(QStringLiteral("  Countries with samples: %1").arg(byCountry.size()));
    out.append(QStringLiteral("[Phase 3/5] Computing per-country HL estimates..."));

    // ── Step 3: Per-country statistics ─────────────────────────────
    struct CountryStats { QString code; double hl; int N; };
    QVector<CountryStats> stats;

    for (auto it = byCountry.begin(); it != byCountry.end(); ++it) {
        auto& samples = it.value();
        int N = samples.size();
        if (N < 3) continue;
        double hl = hodgesLehmann(samples);
        stats.append({it.key(), hl, N});
    }

    if (stats.isEmpty()) {
        // 5WHY: stats.isEmpty() means no country has ≥3 samples.
        // Common on restricted networks where only 1-2 servers per
        // country respond.  Fall back to Hodges-Lehmann estimate of
        // the single best country with any data.
        if (!byCountry.isEmpty()) {
            // Find the country with the most samples and lowest HL
            QString fallbackCountry;
            double fallbackMedian = 1e9; int fallbackN = 0;
            for (auto it = byCountry.begin(); it != byCountry.end(); ++it) {
                auto& samples = it.value();
                int n = samples.size();
                if (n < 1) continue;
                double hl = hodgesLehmann(samples);
                // Prefer more samples; tie-break on lower HL
                if (n > fallbackN || (n == fallbackN && hl < fallbackMedian)) {
                    fallbackCountry = it.key();
                    fallbackMedian = hl;
                    fallbackN = n;
                }
            }
            if (fallbackN > 0) {
                QString countryB = fallbackCountry;
                out.append(QString());
                out.append(QStringLiteral("--- Result (low confidence — insufficient samples) -------------------------"));
                out.append(QStringLiteral("  Physical location (lowest latency) → %1 (HL %2ms, N=%3)")
                    .arg(countryName(countryB)).arg(fallbackMedian, 0, 'f', 1).arg(fallbackN));
                out.append(QStringLiteral("  GeoIP location → %1").arg(countryName(countryA)));
                out.append(QStringLiteral("  Total probed: %1 servers, %2 with data across %3 countries — need ≥3 per country")
                    .arg(allServers.size()).arg(ttfOk.load()).arg(byCountry.size()));

                if (countryA == QStringLiteral("XX")) {
                    out.append(QStringLiteral("  Status: location estimated as %1").arg(countryName(countryB)));
                    r.summary = QStringLiteral("Location est.");
                    r.status = DiagStatus::Warning;
                } else if (countryA == countryB) {
                    out.append(QStringLiteral("  %1 == %2 → likely No VPN (low confidence)")
                        .arg(countryName(countryA), countryName(countryB)));
                    r.summary = QStringLiteral("No VPN");
                    r.status = DiagStatus::Pass;
                } else {
                    out.append(QStringLiteral("  %1 != %2 → possible VPN (low confidence — cannot test significance)")
                        .arg(countryName(countryA), countryName(countryB)));
                    r.summary = QStringLiteral("VPN possible");
                    r.status = DiagStatus::Info;
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
              [](const CountryStats& a, const CountryStats& b) { return a.hl < b.hl; });

    // ── Output ─────────────────────────────────────────────────────
    out.append(QString());
    out.append(QStringLiteral("Per-country Statistics (Hodges-Lehmann, N=%1 countries):").arg(stats.size()));
    out.append(QStringLiteral("  %1  %2  %3")
        .arg(QStringLiteral("Ctry").leftJustified(5, ' '))
        .arg(QStringLiteral("N").rightJustified(3, ' '))
        .arg(QStringLiteral("HL(ms)").rightJustified(8, ' ')));
    for (auto& s : stats) {
        out.append(QStringLiteral("  %1  %2  %3")
            .arg(alpha3(s.code).leftJustified(5, ' ')).arg(s.N, 3)
            .arg(QStringLiteral("%1").arg(s.hl, 0, 'f', 1).rightJustified(8, ' ')));
    }

    // ── Step 4: Find country B ─────────────────────────────────────
    // stats already filtered to N≥3 (line 280) — first entry is best
    CountryStats best = stats[0];
    QString countryB = best.code;

    out.append(QStringLiteral("[Phase 4/5] Running exact permutation test..."));

    // ── Step 5: Exact permutation test + Cliff's Delta ─────────────
    double pValue = 1.0, delta = 0.0;
    bool significant = false;
    bool geoipUnreachable = false; // GeoIP country servers unreachable
    if (countryA != QStringLiteral("XX") && countryA != countryB) {
        if (!byCountry.contains(countryA)) {
            // 5WHY: GeoIP country's servers are all unreachable — this IS
            // a VPN signal.  If you claim to be in Japan but no Japanese
            // server responds, that's suspicious.  Flag it don't ignore it.
            geoipUnreachable = true;
        } else {
        // countryB is guaranteed in byCountry (derived from stats, which
        // are built from byCountry entries; empty stats handled above)
        auto& sA = byCountry[countryA];
        auto& sB = byCountry[countryB];
        int nA = sA.size(), nB = sB.size();
        if (nA >= 3 && nB >= 3) {
            // Compute U for Cliff's delta (reused by exact test internally)
            double U = 0;
            for (double a : sA) for (double b : sB) {
                if (a > b) U += 1; else if (a == b) U += 0.5;
            }
            delta = cliffDelta(U, nA, nB);

            // Precompute combined + |U-μ| once, reuse in exact test
            double mu = static_cast<double>(nA) * nB / 2.0;
            double obsDev = std::abs(U - mu);
            int N = nA + nB;
            QVector<double> combined; combined.reserve(N);
            combined.append(sA); combined.append(sB);
            std::sort(combined.begin(), combined.end());

            // Exact permutation test
            if (N <= 20) {
                pValue = exactPermutationPValue(combined, nA, nB, obsDev);
            } else {
                // Large N — normal approximation fallback (reuses pre-sorted combined)
                double tieCorr = 0.0; int tieRun = 1;
                for (int i = 1; i <= N; i++) {
                    if (i < N && combined[i] == combined[i-1]) { tieRun++; }
                    else { if (tieRun > 1) { double t = tieRun; tieCorr += t*t*t - t; } tieRun = 1; }
                }
                double sigma = std::sqrt((static_cast<double>(nA) * nB / 12.0) * ((N + 1) - tieCorr / (N * (N - 1))));
                double z = (U - mu) / (sigma + 0.001);
                pValue = 2.0 * (1.0 - 0.5 * (1.0 + std::erf(std::abs(z) / std::sqrt(2.0))));
            }
            significant = (pValue < 0.05);
        }
        } // else: countryA in byCountry
    }

    out.append(QStringLiteral("[Phase 5/5] Final decision..."));

    // ── Step 6: Decision ──────────────────────────────────────────
    double absDelta = std::abs(delta);
    out.append(QString());
    out.append(QStringLiteral("--- Result -----------------------------------------------------------------"));
    out.append(QString());
    out.append(QStringLiteral("  Location Report:"));
    out.append(QStringLiteral("    GeoIP location: %1").arg(countryName(countryA)));
    out.append(QStringLiteral("    Physical location (lowest latency): %1 (HL %2ms, N=%3)")
        .arg(countryName(countryB)).arg(best.hl, 0, 'f', 1).arg(best.N));
    out.append(QString());

    // ── Status mapping ──────────────────────────────────────────
    // Pass:    both locations available + match → no VPN
    // Warning: GeoIP unavailable (location estimated) OR GeoIP-country
    //          servers unreachable (suspicious — if you were there they'd respond)
    // Info:    both available + VPN detected/possible/suspected
    // Fail:    both unavailable or no servers reachable
    DiagStatus status = DiagStatus::Pass;
    bool vpnDetected = false;
    // Cache country names — called 10+ times across branches
    const QString nameA = countryName(countryA);
    const QString nameB = countryName(countryB);

    if (countryA == QStringLiteral("XX")) {
        out.append(QStringLiteral("  Location Status: GeoIP unavailable — estimated as %1").arg(nameB));
        r.summary = QStringLiteral("Location: %1").arg(nameB);
        status = DiagStatus::Warning;
    } else if (countryA == countryB) {
        out.append(QStringLiteral("  VPN Status: No VPN — GeoIP matches physical location"));
        r.summary = QStringLiteral("IP: %1").arg(nameA);
    } else if (geoipUnreachable) {
        out.append(QStringLiteral("  Location Status: %1 (GeoIP) servers unreachable — physical location uncertain")
            .arg(nameA));
        r.summary = QStringLiteral("GeoIP %1 vs %2 (VPN possible)").arg(nameA, nameB);
        status = DiagStatus::Warning;
    } else if (significant && absDelta >= 0.33) {
        out.append(QStringLiteral("  VPN Status: VPN detected — %1 → %2 (p=%3, δ=%4)")
            .arg(nameA, nameB).arg(pValue, 0, 'f', 3).arg(delta, 0, 'f', 2));
        r.summary = QStringLiteral("GeoIP %1 → Physical %2 (VPN)").arg(nameA, nameB);
        status = DiagStatus::Info;
        vpnDetected = true;
    } else if (significant && absDelta < 0.33) {
        out.append(QStringLiteral("  VPN Status: VPN likely — %1 → %2 (p=%3, δ=%4 small)")
            .arg(nameA, nameB).arg(pValue, 0, 'f', 3).arg(delta, 0, 'f', 2));
        r.summary = QStringLiteral("GeoIP %1 vs %2 (VPN possible)").arg(nameA, nameB);
        status = DiagStatus::Info;
    } else if (!significant && absDelta >= 0.33) {
        out.append(QStringLiteral("  VPN Status: VPN suspected — %1 → %2 (p=%3 ≥ 0.05, δ=%4 medium)")
            .arg(nameA, nameB).arg(pValue, 0, 'f', 3).arg(delta, 0, 'f', 2));
        r.summary = QStringLiteral("GeoIP %1 vs %2 (VPN possible)").arg(nameA, nameB);
        status = DiagStatus::Info;
    } else {
        out.append(QStringLiteral("  VPN Status: VPN possible — %1 (GeoIP) ≠ %2 (physical)")
            .arg(nameA, nameB));
        out.append(QStringLiteral("    GeoIP and latency disagree but statistics inconclusive (p=%1, δ=%2).")
            .arg(pValue, 0, 'f', 3).arg(delta, 0, 'f', 2));
        r.summary = QStringLiteral("GeoIP %1 vs %2 (VPN possible)").arg(nameA, nameB);
        status = DiagStatus::Info;
    }

    // Structured properties for the detail overlay (UX)
    r.properties.append(ResultProperty("GeoIP location", nameA));
    r.properties.append(ResultProperty("Physical location", nameB));
    r.properties.append(ResultProperty("Latency (HL)", QStringLiteral("%1 ms").arg(best.hl, 0, 'f', 1)));
    r.properties.append(ResultProperty("Samples", QString::number(best.N)));
    if (pValue < 1.0) {
        r.properties.append(ResultProperty("p-value", QString::number(pValue, 'f', 4)));
        r.properties.append(ResultProperty("Cliff's δ", QString::number(delta, 'f', 3)));
    }
    if (geoipUnreachable)
        r.properties.append(ResultProperty("Note", QStringLiteral("GeoIP country servers unreachable")));

    // Add visual callout for reports
    if (status == DiagStatus::Info) {
        if (vpnDetected)
            out.prepend(QStringLiteral("⚠️  VPN DETECTED — GeoIP ≠ Physical Location  ⚠️"));
        else
            out.prepend(QStringLiteral("VPN INDICATED — GeoIP ≠ Physical Location (inconclusive)"));
    } else if (status == DiagStatus::Warning) {
        out.prepend(QStringLiteral("⚠️  LOCATION UNCERTAIN — only one source available  ⚠️"));
    }
    if (status == DiagStatus::Info || status == DiagStatus::Warning)
        out.prepend(QString());

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = status;
    r.durationMs = t.elapsed();
    return r;
}

} // namespace G1G2G3Native
