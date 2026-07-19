// =============================================================================
// GeoProbe.cpp — Shared geographic probe engine (used by G3 diagnostics)
// =============================================================================
#include "Diagnostics/Model/GeoProbe.h"
#include "Common/Utils/NetUtil.h"
#include <algorithm>
#include <QSet>

namespace G1G2G3Native {

// ── Region tag mapping ───────────────────────────────────────────────
// Hierarchical: Continent / Sub-region / Economic zone
QStringList GeoProbe::regionTags(const QString& cc) {
    static const QMap<QString, QStringList> map = {
        // East Asia
        {"CN",{"Asia","Asia/East Asia","Asia/East Asia/Greater China"}},
        {"HK",{"Asia","Asia/East Asia","Asia/East Asia/Greater China"}},
        {"TW",{"Asia","Asia/East Asia","Asia/East Asia/Greater China"}},
        {"MO",{"Asia","Asia/East Asia","Asia/East Asia/Greater China"}},
        {"JP",{"Asia","Asia/East Asia"}},
        {"KR",{"Asia","Asia/East Asia"}},
        {"MN",{"Asia","Asia/East Asia"}},
        // Southeast Asia
        {"SG",{"Asia","Asia/Southeast Asia"}},
        {"TH",{"Asia","Asia/Southeast Asia"}},
        {"MY",{"Asia","Asia/Southeast Asia"}},
        {"ID",{"Asia","Asia/Southeast Asia"}},
        {"PH",{"Asia","Asia/Southeast Asia"}},
        {"VN",{"Asia","Asia/Southeast Asia"}},
        // South Asia
        {"IN",{"Asia","Asia/South Asia"}},
        {"PK",{"Asia","Asia/South Asia"}},
        {"BD",{"Asia","Asia/South Asia"}},
        {"LK",{"Asia","Asia/South Asia"}},
        // Middle East (West Asia)
        {"AE",{"Asia","Asia/Middle East"}},
        {"SA",{"Asia","Asia/Middle East"}},
        {"TR",{"Asia","Asia/Middle East"}},
        {"IL",{"Asia","Asia/Middle East"}},
        {"QA",{"Asia","Asia/Middle East"}},
        {"EG",{"Africa","Africa/North Africa"}},
        // Europe
        {"GB",{"Europe","Europe/Western Europe"}},
        {"DE",{"Europe","Europe/Western Europe"}},
        {"FR",{"Europe","Europe/Western Europe"}},
        {"NL",{"Europe","Europe/Western Europe"}},
        {"IT",{"Europe","Europe/Southern Europe"}},
        {"ES",{"Europe","Europe/Southern Europe"}},
        {"GR",{"Europe","Europe/Southern Europe"}},
        {"SE",{"Europe","Europe/Northern Europe"}},
        {"RU",{"Europe","Europe/Eastern Europe"}},
        {"PL",{"Europe","Europe/Eastern Europe"}},
        {"UA",{"Europe","Europe/Eastern Europe"}},
        {"CH",{"Europe","Europe/Western Europe"}},
        {"AT",{"Europe","Europe/Western Europe"}},
        {"BE",{"Europe","Europe/Western Europe"}},
        {"NO",{"Europe","Europe/Northern Europe"}},
        {"FI",{"Europe","Europe/Northern Europe"}},
        {"DK",{"Europe","Europe/Northern Europe"}},
        {"IE",{"Europe","Europe/Western Europe"}},
        {"PT",{"Europe","Europe/Southern Europe"}},
        // North America
        {"US",{"North America"}},
        {"CA",{"North America"}},
        {"MX",{"North America"}},
        // South America
        {"BR",{"South America"}},
        {"AR",{"South America"}},
        {"CO",{"South America"}},
        {"CL",{"South America"}},
        {"PE",{"South America"}},
        // Africa
        {"ZA",{"Africa","Africa/Southern Africa"}},
        {"NG",{"Africa","Africa/West Africa"}},
        {"KE",{"Africa","Africa/East Africa"}},
        // Oceania
        {"AU",{"Oceania"}},
        {"NZ",{"Oceania"}},
    };
    return map.value(cc, {cc}); // fallback: country code itself as tag
}


GeoProbe::GeoProbe() : m_speedTest() {}

// ── Full probe pipeline ──────────────────────────────────────────────
GeoProbe::Result GeoProbe::probe(int maxTimeSec) {
    Result r;
    QElapsedTimer totalTimer; totalTimer.start();

    auto allServers = m_speedTest.allServers();
    QVector<ServerResult> results = probeAllServers(allServers, maxTimeSec);
    r.totalServers = allServers.size();
    r.totalOk = 0;
    for (auto& sr : results) if (sr.ok) r.totalOk++;

    // Aggregate by country and by region
    r.countries = aggregateByCountry(results);
    r.regions  = aggregateByRegion(results);

    // Physical country = lowest TTFB with >=2 servers
    for (auto& cr : r.countries) {
        if (cr.serverCount >= 2) { r.physicalCountry = cr.code; break; }
    }
    if (r.physicalCountry.isEmpty() && !r.countries.isEmpty())
        r.physicalCountry = r.countries[0].code;

    // Round 2: best server via multi-round CI selection
    // selectBestServer internally sorts by TTFB, takes top 5, and runs CI probes
    QVector<ServerResult> allOk;
    for (auto& sr : results)
        if (sr.ok && sr.ttfbMs > 0) allOk.append(sr);
    r.bestServer = selectBestServer(allOk, 3);

    r.durationSec = totalTimer.elapsed() / 1000.0;
    return r;
}

// ── Pick best server in a specific country ───────────────────────────
GeoProbe::BestServer GeoProbe::pickBestInCountry(const QString& country, int rounds) {
    auto allServers = m_speedTest.allServers();
    QVector<ServerResult> candidates;
    // Quick probe: single TTFB per server
    for (auto& srv : allServers) {
        if (srv.country != country) continue;
        ServerResult sr;
        sr.host = srv.host; sr.port = srv.port;
        sr.country = srv.country; sr.sponsor = srv.sponsor; sr.name = srv.name;
        sr.regions = regionTags(srv.country);

        double ttfb = httpTtfb(parseHttpUrl(srv.url));
        if (ttfb >= 0) { sr.tcpMs = ttfb; sr.ttfbMs = ttfb; sr.ok = true; candidates.append(sr); }
    }
    return selectBestServer(candidates, rounds);
}

// ── Pick best server in a region ─────────────────────────────────────
GeoProbe::BestServer GeoProbe::pickBestInRegion(const QString& regionTag, int rounds) {
    auto allServers = m_speedTest.allServers();
    QVector<ServerResult> candidates;
    for (auto& srv : allServers) {
        auto tags = regionTags(srv.country);
        if (!tags.contains(regionTag)) continue;
        ServerResult sr;
        sr.host = srv.host; sr.port = srv.port;
        sr.country = srv.country; sr.sponsor = srv.sponsor; sr.name = srv.name;
        sr.regions = tags;

        double ttfb = httpTtfb(parseHttpUrl(srv.url));
        if (ttfb >= 0) { sr.tcpMs = ttfb; sr.ttfbMs = ttfb; sr.ok = true; candidates.append(sr); }
    }
    return selectBestServer(candidates, rounds);
}

// ── Multi-threaded TTFB probe of all servers ─────────────────────────
QVector<GeoProbe::ServerResult> GeoProbe::probeAllServers(
    const QVector<SpeedTest::Server>& targets, int maxTimeSec)
{
    QVector<ServerResult> results(targets.size());
    for (int i = 0; i < targets.size(); i++) {
        results[i].host = targets[i].host;
        results[i].port = targets[i].port;
        results[i].country = targets[i].country;
        results[i].sponsor = targets[i].sponsor;
        results[i].name = targets[i].name;
        results[i].regions = regionTags(targets[i].country);
    }

    std::atomic<int> workIdx{0};
    // Use atomic deadline instead of QElapsedTimer to avoid data race
    // across 10 threads (ARM64 may tear QElapsedTimer's internal fields).
    qint64 startMs = QDateTime::currentMSecsSinceEpoch();
    std::atomic<qint64> deadline{startMs + maxTimeSec * 1000};
    const int kThreads = 10;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; t++) {
        threads.emplace_back([&]() {
            while (QDateTime::currentMSecsSinceEpoch() < deadline.load(std::memory_order_acquire)) {
                int idx = workIdx.fetch_add(1);
                if (idx >= targets.size()) break;
                auto& srv = targets[idx];
                auto& out = results[idx];

                // Re-check deadline before starting expensive network I/O
                if (QDateTime::currentMSecsSinceEpoch() >= deadline.load(std::memory_order_acquire)) break;

                double ttfb = httpTtfb(parseHttpUrl(srv.url), 3000, 8);
                if (ttfb >= 0) { out.tcpMs = ttfb; out.ttfbMs = ttfb; out.ok = true; }
                else { out.tcpMs = -1.0; out.ttfbMs = -1.0; }

            }
        });
    }
    for (auto& t : threads) t.join();
    return results;
}

// ── Aggregate by country ─────────────────────────────────────────────
QVector<GeoProbe::CountryResult> GeoProbe::aggregateByCountry(
    const QVector<ServerResult>& results)
{
    QMap<QString, QVector<double>> byCC;
    for (auto& sr : results)
        if (sr.ok && sr.ttfbMs > 0) byCC[sr.country].append(sr.ttfbMs);

    QVector<CountryResult> out;
    for (auto it = byCC.begin(); it != byCC.end(); ++it) {
        if (it.value().size() < 2) continue;
        CountryResult cr;
        cr.code = it.key(); cr.hlMs = hodgesLehmann(it.value());
        cr.serverCount = it.value().size();
        for (auto& sr : results)
            if (sr.ok && sr.country == cr.code) cr.servers.append(sr);
        out.append(cr);
    }
    std::sort(out.begin(), out.end(),
              [](const CountryResult& a, const CountryResult& b) { return a.hlMs < b.hlMs; });
    return out;
}

// ── Aggregate by region ──────────────────────────────────────────────
QVector<GeoProbe::RegionResult> GeoProbe::aggregateByRegion(
    const QVector<ServerResult>& results)
{
    QMap<QString, QVector<double>> byRegion;
    // Use the first region tag (continent) for each server
    for (auto& sr : results) {
        if (!sr.ok || sr.ttfbMs <= 0) continue;
        for (auto& tag : sr.regions) {
            byRegion[tag].append(sr.ttfbMs);
            break; // use top-level tag only
        }
    }

    QVector<RegionResult> out;
    for (auto it = byRegion.begin(); it != byRegion.end(); ++it) {
        if (it.value().size() < 2) continue;
        RegionResult rr;
        rr.tag = it.key(); rr.hlMs = hodgesLehmann(it.value());
        rr.serverCount = it.value().size();
        // Count unique countries
        QSet<QString> ccs;
        for (auto& sr : results)
            if (sr.ok && sr.regions.contains(rr.tag)) ccs.insert(sr.country);
        rr.countryCount = ccs.size();
        for (auto& sr : results)
            if (sr.ok && sr.regions.contains(rr.tag)) rr.servers.append(sr);
        out.append(rr);
    }
    std::sort(out.begin(), out.end(),
              [](const RegionResult& a, const RegionResult& b) { return a.hlMs < b.hlMs; });
    return out;
}

// ── Best-server selection with repeated testing + CI ────────────────
GeoProbe::BestServer GeoProbe::selectBestServer(
    const QVector<ServerResult>& candidates, int rounds)
{
    BestServer best;
    if (candidates.isEmpty()) return best;

    // Sort by initial TTFB, take top 5
    auto top = candidates;
    std::sort(top.begin(), top.end(),
              [](const ServerResult& a, const ServerResult& b) { return a.ttfbMs < b.ttfbMs; });
    if (top.size() > 5) top.resize(5);

    struct Scored { ServerResult srv; double median; double ci; };
    QVector<Scored> scored;

    for (auto& srv : top) {
        QVector<double> measurements;
        measurements.reserve(rounds + 1);
        if (srv.ttfbMs > 0) measurements.append(srv.ttfbMs);

        // Repeated TTFB probes for CI estimation
        for (int r = 0; r < rounds; r++) {
            double ms = httpTtfb(srv.host, srv.port, QStringLiteral("/"), 5000, 5);
            if (ms >= 0) measurements.append(ms);
        }

        if (measurements.size() >= 3) {
            double hl = hodgesLehmann(measurements);
            QVector<double> absDev(measurements.size());
            for (int i = 0; i < measurements.size(); i++)
                absDev[i] = std::abs(measurements[i] - hl);
            std::sort(absDev.begin(), absDev.end());
            double mad = (absDev.size() % 2 == 1) ? absDev[absDev.size()/2]
                         : (absDev[absDev.size()/2-1] + absDev[absDev.size()/2]) / 2.0;
            // t-distribution 95% CI for small n (3-6 samples). z=1.96
            // understates the CI width by 1.3-2.2x at these sample sizes.
            // t_0.025,df for df=2..5; fallback z=1.96 for n>6.
            static const double t95[] = {0, 0, 0, 4.30, 3.18, 2.78, 2.57};
            int n = measurements.size();
            double tval = (n <= 6) ? t95[n] : 1.96;
            double ci = tval * 1.4826 * mad / std::sqrt((double)n);
            scored.append({srv, hl, ci});
        }
    }

    if (scored.isEmpty()) {
        best.host = top[0].host; best.port = top[0].port;
        best.sponsor = top[0].sponsor; best.country = top[0].country;
        best.regions = top[0].regions;
        best.ttfbMs = top[0].ttfbMs; best.rounds = 1; best.valid = true;
        return best;
    }

    std::sort(scored.begin(), scored.end(),
              [](const Scored& a, const Scored& b) { return a.ci < b.ci; });

    best.host = scored[0].srv.host; best.port = scored[0].srv.port;
    best.sponsor = scored[0].srv.sponsor; best.country = scored[0].srv.country;
    best.regions = scored[0].srv.regions;
    best.ttfbMs = scored[0].median;
    best.ttfbCI = scored[0].ci;
    best.rounds = rounds + 1;
    best.valid = true;
    return best;
}

} // namespace G1G2G3Native
