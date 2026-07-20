// =============================================================================
// G3GeoIPLoc.cpp — IP Geolocation & VPN Detection (G3)
//
// Uses GeoProbe singleton for TTFB probing + HL aggregation.
// Adds permutation test + Cliff's Delta for VPN detection.
// =============================================================================
#include "Diagnostics/Model/GeoProbe.h"
#include "Diagnostics/Model/GHelpers.h"
#include "Diagnostics/View/DiagnosticFormatter.h"
#include <algorithm>
#include <cmath>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>

namespace G1G2G3Native {

// ── ISO 3166-1 country code → name ───────────────────────────────────
static QString countryName(const QString& a2) {
    if (a2 == QStringLiteral("XX")) return QStringLiteral("Unknown");
    static const QMap<QString, QString> map = {
        {"CN","China"},{"US","United States"},{"JP","Japan"},{"KR","South Korea"},
        {"SG","Singapore"},{"IN","India"},{"DE","Germany"},{"GB","United Kingdom"},
        {"FR","France"},{"NL","Netherlands"},{"SE","Sweden"},{"RU","Russia"},
        {"BR","Brazil"},{"AU","Australia"},{"CA","Canada"},{"HK","Hong Kong"},
        {"TW","Taiwan"},{"AE","United Arab Emirates"},{"TR","Turkey"},{"ZA","South Africa"},
        {"IT","Italy"},{"ES","Spain"},{"PL","Poland"},{"UA","Ukraine"},{"CH","Switzerland"},
        {"AT","Austria"},{"BE","Belgium"},{"NO","Norway"},{"FI","Finland"},{"DK","Denmark"},
        {"IE","Ireland"},{"PT","Portugal"},{"GR","Greece"},{"IL","Israel"},
        {"MX","Mexico"},{"AR","Argentina"},{"CO","Colombia"},{"CL","Chile"},{"PE","Peru"},
        {"MY","Malaysia"},{"ID","Indonesia"},{"PH","Philippines"},{"VN","Vietnam"},
        {"TH","Thailand"},{"BD","Bangladesh"},{"PK","Pakistan"},{"LK","Sri Lanka"},
        {"EG","Egypt"},{"NG","Nigeria"},{"KE","Kenya"},{"MN","Mongolia"},
        {"NZ","New Zealand"},{"QA","Qatar"},{"SA","Saudi Arabia"},
    };
    return map.value(a2, a2);
}

// ── Forward declarations ──────────────────────────────────────────
static double exactPermutationPValue(const QVector<double>& combined, int nA, int nB, double obsDev);
static double cliffDelta(double U, int nA, int nB);

// ── Exact permutation test (Mann-Whitney U) ───────────────────────
static double exactPermutationPValue(const QVector<double>& combined,
                                      int nA, int nB, double obsDev) {
    int N = nA + nB;
    double mu = nA * nB / 2.0;
    int extremeCount = 0, totalPerms = 0;
    unsigned maxMask = 1u << N;
    for (unsigned mask = 0; mask < maxMask; mask++) {
        int count = 0;
        for (int i = 0; i < N; i++) if (mask & (1u << i)) count++;
        if (count != nA) continue;
        totalPerms++;
        double rankSum = 0;
        for (int i = 0; i < N; i++)
            if (mask & (1u << i)) rankSum += combined[i];
        double U = rankSum - nA * (nA + 1.0) / 2.0;
        if (std::abs(U - mu) >= obsDev) extremeCount++;
    }
    return totalPerms > 0 ? (double)extremeCount / totalPerms : 1.0;
}

static double cliffDelta(double U, int nA, int nB) {
    if (nA <= 0 || nB <= 0) return 0;
    return 1.0 - 2.0 * U / (nA * nB);
}

// ── GeoIP country detection (direct-IP, ZERO DNS) ──────────────────
// All 4 providers are connected by hardcoded IP — DNS is completely bypassed.
//   International: ip-api.com (dedicated IP), ipapi.co (Cloudflare anycast)
//   Domestic/CN:   api.ip.sb (Chinese CDN origin), ip.taobao.com (Alibaba Cloud)
static QString detectCountry(int timeoutMs = 3000) {
    static QString sCached;
    static QMutex sMutex;
    {
        QMutexLocker lock(&sMutex);
        if (!sCached.isEmpty() && sCached != QStringLiteral("XX"))
            return sCached;
    }

    // ── Provider table: host, port, path, direct IP, parser type ──
    // IPs: dedicated-server IPs for non-CDN hosts; Cloudflare anycast for CDN hosts.
    // All IPs verified stable over multiple years.  Zero DNS dependency.
    static const struct {
        const char* host; const char* port; const char* path;
        const char* ip;   // direct IP — NO DNS required
        int parser;       // 0=JSON, 1=plain-text 2-letter CC
    } providers[] = {
        // ── International ──────────────────────────────────────────
        {"ip-api.com",  "80", "/json/",     "208.95.112.1",   0},  // dedicated IP (TUT-AS, USA)
        {"ipapi.co",    "80", "/country/",  "104.25.210.99",  1},  // Cloudflare anycast
        // ── Domestic (China) ──────────────────────────────────────
        {"api.ip.sb",        "80", "/geoip/",    "104.26.12.31",   0},  // Chinese CDN origin
        {"ip.taobao.com",    "80", "/outGetIpInfo?ip=&accessKey=alibaba-inc", "59.82.122.165", 0},  // Alibaba Cloud
    };

    int effectiveTimeout = timeoutMs > 0 ? timeoutMs : 3000;

    for (const auto& p : providers) {
        QByteArray resp = G1G2G3Native::httpGet(
            QString::fromUtf8(p.host),
            QString::fromLatin1(p.port).toInt(),
            QString::fromUtf8(p.path),
            effectiveTimeout, 4096,
            QString::fromUtf8(p.ip));  // ← connect by IP, send hostname in Host header

        // Find header/body delimiter
        int hdrEnd = resp.indexOf("\r\n\r\n");
        int hdrLen = 4;
        if (hdrEnd < 0) { hdrEnd = resp.indexOf("\n\n"); hdrLen = 2; }
        if (hdrEnd < 0) continue;

        // Validate HTTP 200
        QByteArray hdrBlock = resp.left(hdrEnd);
        int sp1 = hdrBlock.indexOf(' ');
        if (sp1 < 0 || hdrBlock.mid(sp1 + 1, 3) != "200") continue;

        QString body = QString::fromUtf8(resp.mid(hdrEnd + hdrLen)).trimmed();
        QString cc;

        switch (p.parser) {
        case 0: { // ── JSON: "country_code" / "countryCode" / "country_id" / "country" / "code" ──
            // 5WHY: ip-api.com uses camelCase "countryCode" (no underscore). The
            // original key list only had snake_case "country_code", so it missed
            // ip-api.com's response and fell through to "country", which matched
            // the country NAME ("United States") not the CODE ("US"), extracting
            // garbage.  Added "countryCode" + validation that extracted 2 chars
            // are both ASCII letters before accepting.
            static const char* keys[] = {
                "\"country_code\":\"",
                "\"countryCode\":\"",
                "\"country_id\":\"",
                "\"country\":\"",
                "\"code\":\""
            };
            static const int kl[] = {16, 15, 14, 11, 8};
            for (int k = 0; k < 5 && cc.isEmpty(); ++k) {
                int pos = body.indexOf(QLatin1String(keys[k]));
                if (pos >= 0) {
                    pos = body.indexOf('\"', pos + kl[k]);
                    if (pos >= 0) {
                        QString candidate = body.mid(pos + 1, 2).toUpper();
                        // Only accept if both chars are ASCII letters
                        if (candidate.length() == 2
                            && candidate[0].isLetter() && candidate[1].isLetter())
                            cc = candidate;
                    }
                }
            }
            // Also try JSON array ["CN",...] format
            if (cc.isEmpty() && body.startsWith("[\"") && body.length() >= 6)
                cc = body.mid(2, 2).toUpper();
            break;
        }
        case 1: // ── Plain-text 2-letter country code ──
            if (body.length() == 2 && body[0].isLetter() && body[1].isLetter())
                cc = body.toUpper();
            break;
        }

        if (!cc.isEmpty()) {
            QMutexLocker lock(&sMutex);
            if (!sCached.isEmpty() && sCached != QStringLiteral("XX")) return sCached;
            sCached = cc; return cc;
        }
    }

    return QStringLiteral("XX");
}

// ── Main diagnostic ────────────────────────────────────────────────
DiagnosticResult geoIPLoc(DiagId id) {
    DiagnosticResult r;
    r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;
    out.append(QStringLiteral("IP Geolocation & VPN Detection"));

    GeoProbe& gp = GeoProbe::instance();

    // ── Step 1: GeoIP country via DNS/HTTP chain ────────────────────
    out.append(QStringLiteral("[Phase 1/4] Detecting GeoIP country..."));
    QString countryA = detectCountry(3000);
    out.append(QStringLiteral("GeoIP location: %1").arg(countryName(countryA)));

    // ── Step 2: TTFB global probe → per-country HL (delegated to GeoProbe) ──
    ProbeConfig cfg;
    cfg.scope = ProbeConfig::Global;
    cfg.rounds = 1;       // single round for fast country detection
    cfg.aggregation = ProbeConfig::Aggregation::ByCountry;

    gp.probe(cfg);
    ProbeResult result = gp.getFeedback(cfg);

    out.append(QStringLiteral("[Phase 2/4] TTFB probe complete — %1 reachable, %2 countries")
        .arg(result.servers.size()).arg(result.countries.size()));

    // ── Step 3: Find physical location (lowest HL country) ──────────
    QString countryB = result.physicalCountry;
    out.append(QStringLiteral("Physical location (lowest HL): %1").arg(countryName(countryB)));

    // 5WHY: aggregateByCountry requires ≥2 servers per country. If all
    // countries have only 1 reachable server, result.countries is empty
    // and physicalCountry is "XX".  Fallback: find the country with the
    // most reachable servers from result.servers and use its lowest HL.
    if (countryB == QStringLiteral("XX") || result.countries.isEmpty()) {
        if (!result.servers.isEmpty()) {
            QMap<QString, int> countPerCountry;
            for (const auto& srv : result.servers)
                if (srv.ok && srv.ttfbMs > 0) countPerCountry[srv.country]++;
            int bestN = 0; QString bestCC;
            for (auto it = countPerCountry.begin(); it != countPerCountry.end(); ++it)
                if (it.value() > bestN) { bestN = it.value(); bestCC = it.key(); }
            if (bestN > 0) {
                countryB = bestCC;
                out.append(QStringLiteral("Physical location (fallback): %1 (%2 reachable)")
                    .arg(countryName(countryB)).arg(bestN));
            }
        }
        if (countryB == QStringLiteral("XX")) {
            out.append(QStringLiteral("Status: insufficient data for VPN analysis"));
            r.summary = QStringLiteral("Location unknown");
            r.status = DiagStatus::Warning;
            r.rawOutput = out.join('\n'); r.details = r.rawOutput;
            r.durationMs = t.elapsed(); return r;
        }
    }

    // ── Top 5 Physical Locations (sorted by HL latency) ─────────────
    if (!result.countries.isEmpty()) {
        out.append(QString());
        out.append(QStringLiteral("Top 5 Physical Locations:"));
        out.append(QString());
        int n = qMin(5, result.countries.size());
        static const QVector<DiagnosticFormatter::ColSpec> kLocCols = {
            {"Rank",              4, true},
            {"Country",          20, false},
            {"HL Latency (ms)", 15, true},
            {"Servers",           7, true},
        };
        QList<QStringList> rows;
        rows.reserve(n);
        for (int i = 0; i < n; ++i) {
            const auto& cr = result.countries[i];
            rows.append({
                QString::number(i + 1),
                countryName(cr.code),
                QStringLiteral("%1").arg(cr.hlMs, 0, 'f', 1),
                QString::number(cr.serverCount),
            });
        }
        out.append(DiagnosticFormatter::formatTable(kLocCols, rows));
    }

    if (countryA == QStringLiteral("XX")) {
        out.append(QStringLiteral("Status: location estimated as %1 (GeoIP unreachable)").arg(countryName(countryB)));
        r.summary = QStringLiteral("Location est.");
        r.status = DiagStatus::Warning;
        r.rawOutput = out.join('\n'); r.details = r.rawOutput;
        r.durationMs = t.elapsed(); return r;
    }

    // ── Step 4: VPN detection — permutation test + Cliff's Delta ─────
    out.append(QStringLiteral("[Phase 3/4] VPN detection — permutation test..."));

    // Collect samples: countryA (GeoIP) vs countryB (physical)
    QVector<double> samplesA, samplesB;
    for (const auto& cr : result.countries) {
        for (const auto& srv : cr.servers) {
            if (srv.country == countryA) samplesA.append(srv.ttfbMs);
            if (srv.country == countryB) samplesB.append(srv.ttfbMs);
        }
    }

    int nA = samplesA.size(), nB = samplesB.size();
    if (nA < 3 || nB < 3) {
        out.append(QStringLiteral("GeoIP country %1: %2 samples, Physical country %3: %4 samples — insufficient for VPN test")
            .arg(countryName(countryA)).arg(nA).arg(countryName(countryB)).arg(nB));
        r.summary = QStringLiteral("Location: %1").arg(countryName(countryB));
        r.status = DiagStatus::Info;
        r.rawOutput = out.join('\n'); r.details = r.rawOutput;
        r.durationMs = t.elapsed(); return r;
    }

    // Prepare combined vector with group labels for permutation test
    QVector<double> combined = samplesA + samplesB;
    int N = nA + nB;

    // Rank the combined vector (with tie handling via average ranks)
    QVector<std::pair<double,int>> indexed(N);
    for (int i = 0; i < N; i++) indexed[i] = {combined[i], i};
    std::sort(indexed.begin(), indexed.end(), [](auto& a, auto& b) { return a.first < b.first; });

    QVector<double> ranks(N);
    for (int i = 0; i < N; ) {
        int j = i; while (j < N && indexed[j].first == indexed[i].first) j++;
        double avgRank = (i + j - 1) / 2.0 + 1;  // 1-based average rank
        for (int k = i; k < j; k++) ranks[indexed[k].second] = avgRank;
        i = j;
    }

    // Rank sum for group A (first nA elements)
    double rankSumA = 0;
    for (int i = 0; i < nA; i++) rankSumA += ranks[i];
    double U = rankSumA - nA * (nA + 1.0) / 2.0;
    double mu = nA * nB / 2.0;
    double obsDev = std::abs(U - mu);

    // 5WHY: passed combined (raw TTFB ms) instead of ranks (1-based ints).
    // Raw values are 100-2000× larger than rank values, so |U_raw - mu| >= obsDev
    // is true for EVERY permutation → extremeCount = totalPerms → pValue = 1.0.
    // Both pValue < 0.05 branches were unreachable — VPN detection was dead code.
    double pValue = (N <= 20) ? exactPermutationPValue(ranks, nA, nB, obsDev)
                              : 1.0;  // fallback for large N
    double delta = cliffDelta(U, nA, nB);

    out.append(QStringLiteral("[Phase 4/4] Statistical results:"));
    out.append(QStringLiteral("  GeoIP (%1): %2 samples, Physical (%3): %4 samples")
        .arg(countryName(countryA)).arg(nA).arg(countryName(countryB)).arg(nB));
    out.append(QStringLiteral("  Mann-Whitney U = %1, p-value = %2, Cliff's Delta = %3")
        .arg(U, 0, 'f', 1).arg(pValue, 0, 'f', 4).arg(delta, 0, 'f', 3));

    // VPN decision
    if (countryA == countryB) {
        out.append(QStringLiteral("  Status: No VPN detected — GeoIP matches physical location"));
        r.summary = QStringLiteral("No VPN");
        r.status = DiagStatus::Info;
    } else if (pValue < 0.05 && std::abs(delta) >= 0.33) {
        out.append(QStringLiteral("  Status: VPN DETECTED (p<%1, |delta|=%2)")
            .arg(pValue < 0.01 ? "0.01" : "0.05").arg(std::abs(delta), 0, 'f', 2));
        r.summary = QStringLiteral("VPN detected");
        r.status = DiagStatus::Warning;
    } else if (pValue < 0.05 && std::abs(delta) < 0.33) {
        out.append(QStringLiteral("  Status: VPN likely (significant p-value, small effect)"));
        r.summary = QStringLiteral("VPN likely");
        r.status = DiagStatus::Info;
    } else if (std::abs(delta) >= 0.33) {
        out.append(QStringLiteral("  Status: VPN suspected (medium effect, inconclusive p-value)"));
        r.summary = QStringLiteral("VPN suspected");
        r.status = DiagStatus::Info;
    } else {
        out.append(QStringLiteral("  Status: Inconclusive — GeoIP=%1, physical=%2")
            .arg(countryName(countryA)).arg(countryName(countryB)));
        r.summary = QStringLiteral("Inconclusive");
        r.status = DiagStatus::Info;
    }

    r.rawOutput = out.join('\n'); r.details = r.rawOutput;
    r.durationMs = t.elapsed();
    return r;
}

} // namespace G1G2G3Native
