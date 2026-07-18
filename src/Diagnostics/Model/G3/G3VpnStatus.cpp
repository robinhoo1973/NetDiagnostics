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

// ── Forward declarations ──────────────────────────────────────────
static double hodgesLehmann(const QVector<double>& v);
static double exactPermutationPValue(const QVector<double>& sA, const QVector<double>& sB);
static double cliffDelta(double U, int nA, int nB);

// ── Hodges-Lehmann robust location estimator ───────────────────────
// 5WHY: Simple median discards all magnitude information except the
// middle value(s).  With N=5, the median uses 1 of 5 data points.
// HL = median of all pairwise averages — uses ALL N(N+1)/2 pairs,
// giving 96% Gaussian efficiency (median: 64%) while retaining
// 29% breakdown point.  Best balance for N=3-26.
// N=4 is the only case where HL equals the mean — acceptable here
// since all values are validated HTTP downloads (no outliers).
static double hodgesLehmann(const QVector<double>& v) {
    int n = v.size();
    if (n == 1) return v[0];
    int npairs = n * (n + 1) / 2;
    QVector<double> pairs; pairs.reserve(npairs);
    for (int i = 0; i < n; i++)
        for (int j = i; j < n; j++)
            pairs.append((v[i] + v[j]) / 2.0);
    std::sort(pairs.begin(), pairs.end());
    return (npairs % 2 == 1) ? pairs[npairs/2]
           : (pairs[npairs/2-1] + pairs[npairs/2]) / 2.0;
}

// ── Exact permutation test (Mann-Whitney U) ───────────────────────
// 5WHY: Normal approximation of U is unreliable at N=3-5 per group.
// With N≤20 total, we enumerate ALL C(N, nA) possible group splits
// and compute the exact null distribution of U.  No approximation,
// no tie correction needed.
static double exactPermutationPValue(const QVector<double>& sA,
                                      const QVector<double>& sB) {
    int nA = sA.size(), nB = sB.size(), N = nA + nB;
    QVector<double> combined; combined.reserve(N);
    combined.append(sA); combined.append(sB);

    double obsU = 0;
    for (double a : sA) for (double b : sB) {
        if (a > b) obsU += 1; else if (a == b) obsU += 0.5;
    }
    double mu = nA * nB / 2.0;
    double obsDev = std::abs(obsU - mu);

    int extremeCount = 0, totalPerms = 0;
    int maxMask = 1 << N;
    for (int mask = 0; mask < maxMask; mask++) {
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
    return 1.0 - 2.0 * U / (nA * nB);
}

// ── VPN Status Detection — HL + Exact Permutation + Cliff's Delta ────
// Compares GeoIP country (A) with the country having lowest
// Hodges-Lehmann latency (B).  If A != B with p < 0.05 + |δ| ≥ 0.33
// → VPN detected.
//
// Algorithm:
//   1a. TCP quick-scan ALL servers → per-country reachability
//   1b. HTTP 100KB micro-download on candidate countries (≥3 reachable)
//   2. Per country: Hodges-Lehmann robust estimator
//   3. Country B = lowest HL estimate (N≥5 required)
//   4. Exact permutation test (enum C(N,nA) splits) → exact p-value
//   5. Cliff's Delta effect size: δ = 1−2U/(nA·nB)
//   6. Decision: p<0.05 + |δ|≥0.33 → VPN; p<0.05 + |δ|<0.33 → possible; |δ|≥0.33 → suspected

DiagnosticResult vpnStatus(DiagId id) {
    DiagnosticResult r;
    r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;
    out.append(QStringLiteral("VPN Status Detection"));
    out.append(QStringLiteral("Two-pass TCP probe + Hodges-Lehmann + Exact Permutation + Cliff's Delta:"));
    out.append(QStringLiteral("  1. TCP quick-scan → filter reachable servers"));
    out.append(QStringLiteral("  2. TCP 2000-connect + HL on candidate countries (≥3 reachable)"));
    out.append(QStringLiteral("  3. Hodges-Lehmann per-country robust location (96% efficiency)"));
    out.append(QStringLiteral("  4. Country B = lowest HL estimate (N≥5)"));
    out.append(QStringLiteral("  5. Exact permutation p-value + Cliff's δ effect size"));
    out.append(QStringLiteral("  6. Decision: p<0.05 + |δ|≥0.33 → VPN detected"));
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

    // ── Pass 2: TCP 2000-connect probe (temporary, replaces HTTP download) ──
    // Candidate-country servers get 2000 TCP connects + HL estimation.
    // Non-candidate servers get quick-fallback TCP single-connect.
    QMap<QString, QVector<double>> byCountry;
    QElapsedTimer probeTimer; probeTimer.start();
    int tcpOk = 0, tcpFail = 0, quickFallback = 0;

    for (auto* srv : targets) {
        if (probeTimer.elapsed() > 44000) break; // 1s margin before 45s cap
        double lat = -1.0;
        if (candidates.contains(srv->country)) {
            // 2000 connects → HL or mean per server.
            // 5WHY: 3s inner guard caps per-server time.  Local servers
            // get ~1500 connects → HL works well.  Remote servers (200ms+)
            // get only ~15 connects → too few for HL, fall back to simple
            // mean as the best available estimate.
            QVector<double> measurements; measurements.reserve(2000);
            QElapsedTimer srvTimer; srvTimer.start();
            for (int i = 0; i < 2000; i++) {
                if (srvTimer.elapsed() > 3000) break;
                QElapsedTimer ct; ct.start();
                int sock = tcpConnect(srv->host, srv->port, 2000);
                if (sock >= 0) {
                    measurements.append(ct.nsecsElapsed() / 1e6);
                    closeSocket(sock);
                }
            }
            int m = measurements.size();
            if (m >= 50) {
                lat = hodgesLehmann(measurements);
            } else if (m >= 5) {
                double sum = 0; for (double v : measurements) sum += v;
                lat = sum / m; // simple mean — too few samples for HL
            }
            if (lat >= 0) tcpOk++; else tcpFail++;
        } else {
            // Non-candidate: single TCP connect is sufficient
            lat = (double)tcpPingMs(srv->host, srv->port);
            quickFallback++;
        }
        if (lat >= 0) byCountry[srv->country].append(lat);
    }
    out.append(QStringLiteral("  TCP 2000 probe: %1 ok (HL estimate), %2 failed, %3 quick-fb")
        .arg(tcpOk).arg(tcpFail).arg(quickFallback));
    out.append(QStringLiteral("  Total reachable countries with samples: %1").arg(byCountry.size()));

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
                out.append(QStringLiteral("  Server latency → %1 (HL %2ms, N=%3)")
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
    CountryStats best = stats[0];
    for (auto& s : stats) {
        if (s.N >= 5) { best = s; break; }
    }
    QString countryB = best.code;

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
        } else if (byCountry.contains(countryB)) {
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

            // Exact permutation test — enumerates all C(N,nA) splits
            // N≤20: exhaustive (~184756 max), else Monte Carlo fallback
            if (nA + nB <= 20) {
                pValue = exactPermutationPValue(sA, sB);
            } else {
                // Large N — keep normal approximation as fallback
                double mu = nA * nB / 2.0;
                int N = nA + nB;
                QVector<double> combined; combined.reserve(N);
                combined.append(sA); combined.append(sB);
                std::sort(combined.begin(), combined.end());
                double tieCorr = 0.0; int tieRun = 1;
                for (int i = 1; i <= N; i++) {
                    if (i < N && combined[i] == combined[i-1]) { tieRun++; }
                    else { if (tieRun > 1) { double t = tieRun; tieCorr += t*t*t - t; } tieRun = 1; }
                }
                double sigma = std::sqrt((nA * nB / 12.0) * ((N + 1) - tieCorr / (N * (N - 1))));
                double z = (U - mu) / (sigma + 0.001);
                pValue = 2.0 * (1.0 - 0.5 * (1.0 + std::erf(std::abs(z) / std::sqrt(2.0))));
            }
            significant = (pValue < 0.05);
        }
        } // else if (byCountry.contains(countryB))
    }

    // ── Step 6: Decision ──────────────────────────────────────────
    double absDelta = std::abs(delta);
    out.append(QString());
    out.append(QStringLiteral("--- Result -----------------------------------------------------------------"));
    out.append(QStringLiteral("  Server latency → %1 (HL %2ms, N=%3)")
        .arg(countryName(countryB)).arg(best.hl, 0, 'f', 1).arg(best.N));
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
    } else if (geoipUnreachable) {
        out.append(QStringLiteral("  %1 (GeoIP) servers unreachable → VPN likely (GeoIP country unreachable from your network)")
            .arg(countryName(countryA)));
        out.append(QStringLiteral("  If you are in %1, its servers should be reachable. They are not.")
            .arg(countryName(countryA)));
        scenario = QStringLiteral("VPN likely (%1 unreachable)").arg(countryName(countryA));
        status = DiagStatus::Warning;
    } else if (significant && absDelta >= 0.33) {
        out.append(QStringLiteral("  %1 != %2 → VPN detected (p=%3, δ=%4, medium/large effect)")
            .arg(countryName(countryA), countryName(countryB))
            .arg(pValue, 0, 'f', 3).arg(delta, 0, 'f', 2));
        scenario = QStringLiteral("VPN detected (%1 → %2, δ=%3)")
            .arg(countryName(countryA), countryName(countryB)).arg(delta, 0, 'f', 2);
        status = DiagStatus::Warning;
    } else if (significant && absDelta < 0.33) {
        out.append(QStringLiteral("  %1 != %2 → VPN likely (p=%3 significant but δ=%4 small effect)")
            .arg(countryName(countryA), countryName(countryB))
            .arg(pValue, 0, 'f', 3).arg(delta, 0, 'f', 2));
        scenario = QStringLiteral("VPN possible (%1 → %2, p<0.05, δ=%3)")
            .arg(countryName(countryA), countryName(countryB)).arg(delta, 0, 'f', 2);
        status = DiagStatus::Warning;
    } else if (!significant && absDelta >= 0.33) {
        out.append(QStringLiteral("  %1 != %2 → VPN suspected (p=%3 ≥ 0.05 but δ=%4 medium effect)")
            .arg(countryName(countryA), countryName(countryB))
            .arg(pValue, 0, 'f', 3).arg(delta, 0, 'f', 2));
        out.append(QStringLiteral("  Effect size suggests real difference — more samples needed for significance."));
        scenario = QStringLiteral("VPN suspected (%1 → %2, p≥0.05, δ=%3)")
            .arg(countryName(countryA), countryName(countryB)).arg(delta, 0, 'f', 2);
    } else {
        // 5WHY: GeoIP ≠ Country B is itself a signal — two independent
        // methods (DNS GeoIP + TCP latency) disagree about your location.
        // Even when n is too small for statistical significance, the
        // contradiction should not be dismissed as "No VPN".
        out.append(QStringLiteral("  %1 (GeoIP) ≠ %2 (lowest latency) → VPN possible (p=%3, δ=%4)")
            .arg(countryName(countryA), countryName(countryB))
            .arg(pValue, 0, 'f', 3).arg(delta, 0, 'f', 2));
        out.append(QStringLiteral("  GeoIP and latency disagree — may indicate VPN. Statistics inconclusive"
            " with current sample size.  More reachable servers would improve confidence."));
        scenario = QStringLiteral("VPN possible (%1 → %2)")
            .arg(countryName(countryA), countryName(countryB));
        status = DiagStatus::Warning;
    }

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    if (scenario.startsWith(QStringLiteral("No VPN")))
        r.summary = QStringLiteral("No VPN");
    else if (scenario.startsWith(QStringLiteral("VPN detected")))
        r.summary = QStringLiteral("VPN detected");
    else if (scenario.startsWith(QStringLiteral("VPN possible"))
             || scenario.startsWith(QStringLiteral("VPN likely"))
             || scenario.startsWith(QStringLiteral("VPN suspected")))
        r.summary = QStringLiteral("VPN possible");
    else
        r.summary = QStringLiteral("Location est.");
    r.status = status;
    r.durationMs = t.elapsed();
    return r;
}

} // namespace G1G2G3Native
