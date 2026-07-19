// =============================================================================
// G2GeoProbe.cpp — Geographic Probe Engine implementation
// =============================================================================
#include "Diagnostics/Model/G2/G2GeoProbe.h"
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

    // Best server in physical country
    if (!r.physicalCountry.isEmpty()) {
        QVector<ServerResult> cs;
        for (auto& sr : results)
            if (sr.ok && sr.country == r.physicalCountry) cs.append(sr);
        r.bestServer = selectBestServer(cs, 5);
    }

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

        ParsedUrl pu = parseHttpUrl(srv.url);
        QElapsedTimer t; t.start();
        int sock = tcpConnect(pu.host, pu.port, 5000);
        if (sock >= 0) {
            sr.tcpMs = t.elapsed();
            QString dlHost = (pu.port != 80) ? QStringLiteral("%1:%2").arg(pu.host).arg(pu.port) : pu.host;
            QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: ND/1.0\r\nConnection: close\r\n\r\n")
                .arg(pu.path, dlHost).toUtf8();
            ::send(sock, req.constData(), req.size(), 0);
            fd_set fds; struct timeval tv = {5, 0};
            FD_ZERO(&fds); FD_SET(sock, &fds);
            if (select(sock + 1, &fds, nullptr, nullptr, &tv) > 0) {
                char b[1]; if (recv(sock, b, 1, 0) > 0) { sr.ttfbMs = t.elapsed(); sr.ok = true; }
            }
            closeSocket(sock);
        }
        candidates.append(sr);
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

        ParsedUrl pu = parseHttpUrl(srv.url);
        QElapsedTimer t; t.start();
        int sock = tcpConnect(pu.host, pu.port, 5000);
        if (sock >= 0) {
            sr.tcpMs = t.elapsed();
            QString dlHost = (pu.port != 80) ? QStringLiteral("%1:%2").arg(pu.host).arg(pu.port) : pu.host;
            QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: ND/1.0\r\nConnection: close\r\n\r\n")
                .arg(pu.path, dlHost).toUtf8();
            ::send(sock, req.constData(), req.size(), 0);
            fd_set fds; struct timeval tv = {5, 0};
            FD_ZERO(&fds); FD_SET(sock, &fds);
            if (select(sock + 1, &fds, nullptr, nullptr, &tv) > 0) {
                char b[1]; if (recv(sock, b, 1, 0) > 0) { sr.ttfbMs = t.elapsed(); sr.ok = true; }
            }
            closeSocket(sock);
        }
        candidates.append(sr);
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

                double ttfb = -1.0, tcpMs = -1.0;
                ParsedUrl pu = parseHttpUrl(srv.url);
                QElapsedTimer pt; pt.start();
                int sock = tcpConnect(pu.host, pu.port, 3000);
                if (sock >= 0) {
                    tcpMs = pt.elapsed();
                    QString dlHost = (pu.port != 80) ? QStringLiteral("%1:%2").arg(pu.host).arg(pu.port) : pu.host;
                    QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: ND/1.0\r\nConnection: close\r\n\r\n")
                        .arg(pu.path, dlHost).toUtf8();
                    ::send(sock, req.constData(), req.size(), 0);

                    fd_set fds; struct timeval tv = {8, 0};
                    FD_ZERO(&fds); FD_SET(sock, &fds);
                    if (select(sock + 1, &fds, nullptr, nullptr, &tv) > 0) {
                        char b[1];
                        if (recv(sock, b, 1, 0) > 0) { ttfb = pt.elapsed(); out.ok = true; }
                    }
                    closeSocket(sock);
                }
                out.tcpMs = tcpMs; out.ttfbMs = ttfb;

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
    QSet<QString> countriesInRegion;
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

    // Sort by initial TTFB, take top 3
    auto top = candidates;
    std::sort(top.begin(), top.end(),
              [](const ServerResult& a, const ServerResult& b) { return a.ttfbMs < b.ttfbMs; });
    if (top.size() > 3) top.resize(3);

    struct Scored { ServerResult srv; double median; double ci; };
    QVector<Scored> scored;

    for (auto& srv : top) {
        QVector<double> measurements;
        measurements.reserve(rounds + 1);
        if (srv.ttfbMs > 0) measurements.append(srv.ttfbMs);

        // Use host:port directly — already parsed from URL
        for (int r = 0; r < rounds; r++) {
            QElapsedTimer t; t.start();
            int sock = tcpConnect(srv.host, srv.port, 5000);
            if (sock >= 0) {
                QString path = QStringLiteral("/download?size=100000");
                QString dlHost = (srv.port != 80) ? QStringLiteral("%1:%2").arg(srv.host).arg(srv.port) : srv.host;
                QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: ND/1.0\r\nConnection: close\r\n\r\n")
                    .arg(path, dlHost).toUtf8();
                ::send(sock, req.constData(), req.size(), 0);
                fd_set fds; struct timeval tv = {5, 0};
                FD_ZERO(&fds); FD_SET(sock, &fds);
                if (select(sock + 1, &fds, nullptr, nullptr, &tv) > 0) {
                    char b[1];
                    if (recv(sock, b, 1, 0) > 0) measurements.append(t.elapsed());
                }
                closeSocket(sock);
            }
        }

        if (measurements.size() >= 3) {
            double hl = hodgesLehmann(measurements);
            double med = hl;
            QVector<double> absDev(measurements.size());
            for (int i = 0; i < measurements.size(); i++)
                absDev[i] = std::abs(measurements[i] - med);
            std::sort(absDev.begin(), absDev.end());
            double mad = (absDev.size() % 2 == 1) ? absDev[absDev.size()/2]
                         : (absDev[absDev.size()/2-1] + absDev[absDev.size()/2]) / 2.0;
            double ci = 1.96 * 1.4826 * mad / std::sqrt((double)measurements.size());
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

// ── internetConnectivity diagnostic ─────────────────────────────────
DiagnosticResult internetConnectivity(DiagId id) {
    DiagnosticResult r;
    r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();

    GeoProbe probe;
    GeoProbe::Result result = probe.probe(45);

    QStringList out;
    out.append(QStringLiteral("Internet Connectivity — TTFB Geographic Probe"));
    out.append(QString());
    out.append(QStringLiteral("Physical country: %1").arg(result.physicalCountry));
    out.append(QStringLiteral("Servers probed: %1, TTFB OK: %2, Duration: %3s")
        .arg(result.totalServers).arg(result.totalOk).arg(result.durationSec, 0, 'f', 1));
    out.append(QString());

    // Country ranking
    out.append(QStringLiteral("Country ranking (TTFB HL):"));
    for (auto& cr : result.countries) {
        out.append(QStringLiteral("  %1: %2ms (N=%3)")
            .arg(cr.code).arg(cr.hlMs, 0, 'f', 1).arg(cr.serverCount));
    }

    // Region ranking
    out.append(QString());
    out.append(QStringLiteral("Region ranking (TTFB HL):"));
    for (auto& rr : result.regions) {
        out.append(QStringLiteral("  %1: %2ms (%3 servers, %4 countries)")
            .arg(rr.tag).arg(rr.hlMs, 0, 'f', 1).arg(rr.serverCount).arg(rr.countryCount));
    }

    // Best server
    out.append(QString());
    out.append(QStringLiteral("--- Best Speed Test Server (%1) ---").arg(result.physicalCountry));
    if (result.bestServer.valid) {
        out.append(QStringLiteral("  Host: %1:%2").arg(result.bestServer.host).arg(result.bestServer.port));
        out.append(QStringLiteral("  Sponsor: %1").arg(result.bestServer.sponsor));
        out.append(QStringLiteral("  TTFB: %1ms (95%CI: ±%2ms, %3 rounds)")
            .arg(result.bestServer.ttfbMs, 0, 'f', 1)
            .arg(result.bestServer.ttfbCI, 0, 'f', 1)
            .arg(result.bestServer.rounds));
        r.summary = QStringLiteral("Connected — %1 (%2ms)")
            .arg(result.physicalCountry).arg(result.bestServer.ttfbMs, 0, 'f', 0);
        r.status = DiagStatus::Pass;
    } else {
        out.append(QStringLiteral("  No reachable server in %1").arg(result.physicalCountry));
        r.summary = QStringLiteral("Location: %1").arg(result.physicalCountry);
        r.status = DiagStatus::Warning;
    }

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.durationMs = t.elapsed();
    return r;
}

} // namespace G1G2G3Native
