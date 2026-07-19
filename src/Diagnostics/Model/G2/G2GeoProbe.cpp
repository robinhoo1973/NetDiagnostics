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

    // Round 2: top 5 globally by TTFB, 3-repeat CI selection
    QVector<ServerResult> allOk;
    for (auto& sr : results)
        if (sr.ok && sr.ttfbMs > 0) allOk.append(sr);
    std::sort(allOk.begin(), allOk.end(),
              [](const ServerResult& a, const ServerResult& b) { return a.ttfbMs < b.ttfbMs; });
    if (allOk.size() > 5) allOk.resize(5);
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

        // Use host:port directly — already parsed from URL
        for (int r = 0; r < rounds; r++) {
            QElapsedTimer t; t.start();
            int sock = tcpConnect(srv.host, srv.port, 5000);
            if (sock >= 0) {
                QString dlHost = (srv.port != 80) ? QStringLiteral("%1:%2").arg(srv.host).arg(srv.port) : srv.host;
                QByteArray req = QStringLiteral("GET / HTTP/1.0\r\nHost: %1\r\nUser-Agent: ND/1.0\r\nConnection: close\r\n\r\n")
                    .arg(dlHost).toUtf8();
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
    out.append(QStringLiteral("Internet Connectivity"));
    out.append(QStringLiteral("Method: TTFB global probe → top 5 → 3-round CI → speed test"));
    out.append(QString());

    // ── Phase 1-2: Location ──
    out.append(QStringLiteral("Physical location: %1").arg(result.physicalCountry));
    out.append(QStringLiteral("Probed %1 servers, %2 reachable (%3s)")
        .arg(result.totalServers).arg(result.totalOk).arg(result.durationSec, 0, 'f', 1));
    out.append(QString());

    // ── Phase 3: Top 5 servers ──
    int shown = 0;
    for (auto& cr : result.countries) {
        for (auto& sr : cr.servers) {
            if (shown >= 5) break;
            out.append(QStringLiteral("  %1. %2 (%3) — %4ms")
                .arg(shown + 1).arg(sr.sponsor).arg(cr.code).arg(sr.ttfbMs, 0, 'f', 1));
            shown++;
        }
        if (shown >= 5) break;
    }
    out.append(QString());

    // ── Phase 4: Best server with CI ──
    if (result.bestServer.valid) {
        out.append(QStringLiteral("Best server (%1): %2 — %3ms (95%CI ±%4ms, %5 rounds)")
            .arg(result.physicalCountry).arg(result.bestServer.sponsor)
            .arg(result.bestServer.ttfbMs, 0, 'f', 1)
            .arg(result.bestServer.ttfbCI, 0, 'f', 1)
            .arg(result.bestServer.rounds));
    } else {
        out.append(QStringLiteral("No reachable server in %1").arg(result.physicalCountry));
        r.summary = QStringLiteral("Location: %1").arg(result.physicalCountry);
        r.status = DiagStatus::Warning;
        r.rawOutput = out.join('\n'); r.details = r.rawOutput;
        r.durationMs = t.elapsed(); return r;
    }

    // ── Phase 5: Speed test on best server ──
    out.append(QString());
    out.append(QStringLiteral("Running speed test on %1...").arg(result.bestServer.sponsor));
    QString dlUrl = QStringLiteral("http://%1:%2/download?size=250000")
        .arg(result.bestServer.host).arg(result.bestServer.port);
    SpeedResult dl = httpDownload(dlUrl, 250000, 15000);
    if (dl.ok && dl.mbps > 0.01) {
        out.append(QStringLiteral("  Download: %1 Mbps (%2 bytes in %3ms)")
            .arg(dl.mbps, 0, 'f', 1).arg(dl.bytes).arg(dl.durationMs));
        r.summary = QStringLiteral("Connected — %1 (%2ms, %3 Mbps)")
            .arg(result.physicalCountry).arg(result.bestServer.ttfbMs, 0, 'f', 0)
            .arg(dl.mbps, 0, 'f', 1);
        r.status = DiagStatus::Pass;
    } else {
        out.append(QStringLiteral("  Speed test failed — server unreachable for download"));
        r.summary = QStringLiteral("Connected — %1 (%2ms, speed N/A)")
            .arg(result.physicalCountry).arg(result.bestServer.ttfbMs, 0, 'f', 0);
        r.status = DiagStatus::Warning;
    }

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.durationMs = t.elapsed();
    return r;
}

} // namespace G1G2G3Native
