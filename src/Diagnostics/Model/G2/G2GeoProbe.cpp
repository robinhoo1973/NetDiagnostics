// =============================================================================
// G2GeoProbe.cpp — Geographic Probe Engine implementation
// =============================================================================
#include "Diagnostics/Model/G2/G2GeoProbe.h"
#include "Common/Utils/NetUtil.h"
#include <algorithm>
#include <QSet>

namespace G1G2G3Native {

// ── Hodges-Lehmann robust estimator ──────────────────────────────────
double GeoProbe::hodgesLehmann(const QVector<double>& v) {
    int n = v.size();
    if (n == 1) return v[0];
    int npairs = n * (n + 1) / 2;
    QVector<double> pairs; pairs.reserve(npairs);
    for (int i = 0; i < n; i++)
        for (int j = i; j < n; j++)
            pairs.append((v[i] + v[j]) / 2.0);
    std::sort(pairs.begin(), pairs.end());
    return (npairs % 2 == 1) ? pairs[npairs / 2]
           : (pairs[npairs / 2 - 1] + pairs[npairs / 2]) / 2.0;
}

GeoProbe::GeoProbe() : m_speedTest() {}

// ── Full probe pipeline ──────────────────────────────────────────────
GeoProbe::Result GeoProbe::probe(int maxTimeSec) {
    Result r;
    QElapsedTimer totalTimer; totalTimer.start();

    // Phase 1: Load all servers, TTFB probe every one
    auto allServers = m_speedTest.allServers();
    QVector<ServerResult> results = probeAllServers(allServers, maxTimeSec);
    r.totalServers = allServers.size();
    r.totalOk = 0;
    for (auto& sr : results) if (sr.ok) r.totalOk++;

    // Phase 2: Aggregate by country, sort by latency
    r.countries = aggregateByCountry(results);

    // Phase 3: Physical country = lowest TTFB with >=2 servers
    for (auto& cr : r.countries) {
        if (cr.serverCount >= 2) {
            r.physicalCountry = cr.code;
            break;
        }
    }
    if (r.physicalCountry.isEmpty() && !r.countries.isEmpty())
        r.physicalCountry = r.countries[0].code;

    // Phase 4: Pick best server in physical country
    if (!r.physicalCountry.isEmpty()) {
        QVector<ServerResult> countryServers;
        for (auto& sr : results)
            if (sr.ok && sr.country == r.physicalCountry)
                countryServers.append(sr);
        r.bestServer = selectBestServer(countryServers, 5);
    }

    r.durationSec = totalTimer.elapsed() / 1000.0;
    return r;
}

// ── Multi-threaded TTFB probe of all servers ─────────────────────────
QVector<GeoProbe::ServerResult> GeoProbe::probeAllServers(
    const QVector<SpeedTest::Server>& targets, int maxTimeSec)
{
    QVector<ServerResult> results(targets.size());
    // Initialize
    for (int i = 0; i < targets.size(); i++) {
        results[i].host = targets[i].host;
        results[i].port = targets[i].port;
        results[i].country = targets[i].country;
        results[i].sponsor = targets[i].sponsor;
        results[i].name = targets[i].name;
    }

    std::atomic<int> workIdx{0};
    std::atomic<bool> expired{false};
    QElapsedTimer globalTimer; globalTimer.start();
    const int kThreads = 10;
    const int kMaxMs = maxTimeSec * 1000;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; t++) {
        threads.emplace_back([&]() {
            while (!expired.load(std::memory_order_relaxed)) {
                int idx = workIdx.fetch_add(1);
                if (idx >= targets.size()) break;
                auto& srv = targets[idx];
                auto& out = results[idx];

                // TCP + HTTP TTFB
                double ttfb = -1.0, tcpMs = -1.0;
                QString url = srv.url + QStringLiteral("/download?size=100000");
                QElapsedTimer probeTimer; probeTimer.start();

                // Parse URL for httpDownload
                QString u = url;
                if (u.startsWith(QStringLiteral("http://"))) u = u.mid(7);
                auto slash = u.indexOf('/');
                QString hostPort = (slash > 0) ? u.left(slash) : u;
                QString path = (slash > 0) ? u.mid(slash) : QStringLiteral("/");
                QString host = hostPort; int port = 80;
                auto colon = hostPort.lastIndexOf(':');
                if (colon > 0) { host = hostPort.left(colon); port = hostPort.mid(colon + 1).toInt(); }

                // TCP connect
                int sock = tcpConnect(host, port, 3000);
                if (sock >= 0) {
                    tcpMs = probeTimer.elapsed();
                    // Send HTTP GET for TTFB
                    QString dlHost = (port != 80) ? QStringLiteral("%1:%2").arg(host).arg(port) : host;
                    QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: NetDiagnostics/1.0\r\nConnection: close\r\n\r\n")
                        .arg(path, dlHost).toUtf8();
                    ::send(sock, req.constData(), req.size(), 0);

                    // Read first response byte (TTFB)
                    fd_set fdset; struct timeval tv = {8, 0};
                    FD_ZERO(&fdset); FD_SET(sock, &fdset);
                    if (select(sock + 1, &fdset, nullptr, nullptr, &tv) > 0) {
                        char buf[1];
                        if (recv(sock, buf, 1, 0) > 0) {
                            ttfb = probeTimer.elapsed();
                            out.ok = true;
                        }
                    }
                    closeSocket(sock);
                }
                out.tcpMs = tcpMs;
                out.ttfbMs = ttfb;

                if (globalTimer.elapsed() > kMaxMs)
                    expired.store(true, std::memory_order_relaxed);
            }
        });
    }
    for (auto& t : threads) t.join();
    return results;
}

// ── Aggregate results by country, compute HL, sort ──────────────────
QVector<GeoProbe::CountryResult> GeoProbe::aggregateByCountry(
    const QVector<ServerResult>& results)
{
    QMap<QString, QVector<double>> byCountry;
    for (auto& sr : results) {
        if (sr.ok && sr.ttfbMs > 0)
            byCountry[sr.country].append(sr.ttfbMs);
    }

    QVector<CountryResult> out;
    for (auto it = byCountry.begin(); it != byCountry.end(); ++it) {
        if (it.value().size() < 2) continue;
        CountryResult cr;
        cr.code = it.key();
        cr.hlMs = hodgesLehmann(it.value());
        cr.serverCount = it.value().size();
        // Attach raw server results
        for (auto& sr : results)
            if (sr.ok && sr.country == cr.code)
                cr.servers.append(sr);
        out.append(cr);
    }

    std::sort(out.begin(), out.end(),
              [](const CountryResult& a, const CountryResult& b) {
                  return a.hlMs < b.hlMs;
              });
    return out;
}

// ── Select best server: repeated testing + CI-based ranking ─────────
GeoProbe::BestServer GeoProbe::selectBestServer(
    const QVector<ServerResult>& countryServers, int rounds)
{
    BestServer best;
    if (countryServers.isEmpty()) return best;

    // Sort by initial TTFB, take top 3
    QVector<ServerResult> top = countryServers;
    std::sort(top.begin(), top.end(),
              [](const ServerResult& a, const ServerResult& b) {
                  return a.ttfbMs < b.ttfbMs;
              });
    if (top.size() > 3) top.resize(3);

    // For each top server, run `rounds` additional TTFB tests
    struct Scored { ServerResult srv; double median; double ci; };
    QVector<Scored> scored;

    for (auto& srv : top) {
        QVector<double> measurements;
        measurements.reserve(rounds + 1);
        measurements.append(srv.ttfbMs); // include initial measurement

        QString url = QStringLiteral("http://%1:%2/download?size=100000")
            .arg(srv.host).arg(srv.port);
        QString u = url.mid(7);
        auto slash = u.indexOf('/');
        QString hp = (slash > 0) ? u.left(slash) : u;
        QString pth = (slash > 0) ? u.mid(slash) : "/";
        QString hst = hp; int prt = 80;
        auto cln = hp.lastIndexOf(':');
        if (cln > 0) { hst = hp.left(cln); prt = hp.mid(cln + 1).toInt(); }

        for (int r = 0; r < rounds; r++) {
            QElapsedTimer t; t.start();
            int sock = tcpConnect(hst, prt, 5000);
            if (sock >= 0) {
                QString dlHost = (prt != 80) ? QStringLiteral("%1:%2").arg(hst).arg(prt) : hst;
                QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: NetDiagnostics/1.0\r\nConnection: close\r\n\r\n")
                    .arg(pth, dlHost).toUtf8();
                ::send(sock, req.constData(), req.size(), 0);
                fd_set fds; struct timeval tv = {5, 0};
                FD_ZERO(&fds); FD_SET(sock, &fds);
                if (select(sock + 1, &fds, nullptr, nullptr, &tv) > 0) {
                    char b[1];
                    if (recv(sock, b, 1, 0) > 0)
                        measurements.append(t.elapsed());
                }
                closeSocket(sock);
            }
        }

        if (measurements.size() >= 3) {
            double hl = hodgesLehmann(measurements);
            // 95% CI half-width via MAD
            QVector<double> absDev(measurements.size());
            double med = hl;
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
        // Fallback: use the first server with lowest initial TTFB
        best.host = top[0].host; best.port = top[0].port;
        best.sponsor = top[0].sponsor; best.country = top[0].country;
        best.ttfbMs = top[0].ttfbMs; best.rounds = 1; best.valid = true;
        return best;
    }

    // Pick server with lowest CI (most consistent, not necessarily fastest)
    std::sort(scored.begin(), scored.end(),
              [](const Scored& a, const Scored& b) { return a.ci < b.ci; });

    best.host = scored[0].srv.host;
    best.port = scored[0].srv.port;
    best.sponsor = scored[0].srv.sponsor;
    best.country = scored[0].srv.country;
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
    out.append(QStringLiteral("Total servers probed: %1, TTFB OK: %2")
        .arg(result.totalServers).arg(result.totalOk));
    out.append(QStringLiteral("Duration: %1s").arg(result.durationSec, 0, 'f', 1));
    out.append(QString());

    // Per-country summary
    out.append(QStringLiteral("Country ranking (TTFB HL estimate):"));
    for (auto& cr : result.countries) {
        out.append(QStringLiteral("  %1: %2ms (N=%3)")
            .arg(cr.code).arg(cr.hlMs, 0, 'f', 1).arg(cr.serverCount));
    }

    // Best server for speed test
    out.append(QString());
    out.append(QStringLiteral("--- Best Speed Test Server (%1) ---").arg(result.physicalCountry));
    if (result.bestServer.valid) {
        out.append(QStringLiteral("  Host: %1:%2").arg(result.bestServer.host).arg(result.bestServer.port));
        out.append(QStringLiteral("  Sponsor: %1").arg(result.bestServer.sponsor));
        out.append(QStringLiteral("  TTFB: %1ms (95%CI: ±%2ms, N=%3 rounds)")
            .arg(result.bestServer.ttfbMs, 0, 'f', 1)
            .arg(result.bestServer.ttfbCI, 0, 'f', 1)
            .arg(result.bestServer.rounds));
        r.summary = QStringLiteral("Connected — %1 (%2ms)")
            .arg(result.physicalCountry).arg(result.bestServer.ttfbMs, 0, 'f', 0);
        r.status = DiagStatus::Pass;
    } else {
        out.append(QStringLiteral("  No reachable server in %1").arg(result.physicalCountry));
        r.summary = QStringLiteral("Location: %1 (no speed test server)").arg(result.physicalCountry);
        r.status = DiagStatus::Warning;
    }

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.durationMs = t.elapsed();
    return r;
}

} // namespace G1G2G3Native
