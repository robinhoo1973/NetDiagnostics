// =============================================================================
// G3InternetConnectivity.cpp — Internet Connectivity diagnostic (G3)
//
// Uses GeoProbe for TTFB-based server probing + speed test on best server.
// Wired to DiagId::G3InternetConnectivity via TaskFactory.
// =============================================================================
#include "Diagnostics/Model/GeoProbe.h"
#include "Diagnostics/Model/G3/G3InternetDns.h"
#include "Diagnostics/Model/GHelpers.h"
#include "Common/Services/DnsResolver.h"
#include "Diagnostics/View/DiagnosticFormatter.h"

namespace G1G2G3Native {

DiagnosticResult internetConnectivity(DiagId id) {
    DiagnosticResult r;
    r.id = id; r.group = diagGroup(id);
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();

    GeoProbe& gp = GeoProbe::instance();

    ProbeConfig cfg;
    cfg.scope = ProbeConfig::Global;
    cfg.rounds = 3;
    cfg.aggregation = ProbeConfig::Aggregation::ByCountry;

    gp.probe(cfg);
    ProbeResult result = gp.getFeedback(cfg);

    // ── Build server metadata lookup (name, sponsor, IP) ──────────
    // static: server DB is immutable, copy once per process lifetime
    static SpeedTest st;
    struct Meta { QString name; QString sponsor; };
    QHash<QString, Meta> metaByKey;  // key = "host:port"
    for (const auto& srv : st.allServers()) {
        Meta m; m.name = srv.name; m.sponsor = srv.sponsor;
        metaByKey.insert(srv.host + QStringLiteral(":") + QString::number(srv.port), m);
    }
    // Resolve IP for a hostname (cached per host)
    QHash<QString, QString> ipCache;
    auto resolveIp = [&](const QString& host) -> QString {
        auto it = ipCache.find(host);
        if (it != ipCache.end()) return it.value();
        QString ip = DnsResolver::instance().resolve(host, 3000);
        ipCache[host] = ip;
        return ip;
    };

    QStringList out;
    out.append(QStringLiteral("Internet Connectivity"));
    out.append(QStringLiteral("Method: TTFB global probe → 3-round → HL aggregation → speed test"));
    out.append(QString());

    // ── Phase 1: Location ────────────────────────────────────────
    out.append(QStringLiteral("── Phase 1: Location ──"));
    out.append(QStringLiteral("Physical location: %1").arg(result.physicalCountry));
    out.append(QStringLiteral("Probed %1 servers, %2 countries reachable")
        .arg(result.servers.size()).arg(result.countries.size()));
    out.append(QString());

    // ── Phase 2: Top 5 servers (sorted by TTFB, fastest first) ──
    out.append(QStringLiteral("── Phase 2: Top 5 Servers ──"));
    int shown = 0;
    QList<QStringList> topRows;
    for (const auto& sr : result.servers) {
        if (shown >= 5) break;
        QString key = sr.host + QStringLiteral(":") + QString::number(sr.port);
        auto mit = metaByKey.constFind(key);
        QString name = (mit != metaByKey.cend()) ? mit->name : sr.host;
        QString ip = resolveIp(sr.host);
        topRows.append({
            QString::number(shown + 1),
            name,
            sr.country,
            ip.isEmpty() ? sr.host : ip,
            QStringLiteral("%1ms").arg(sr.ttfbMs, 0, 'f', 1),
            QStringLiteral("±%1ms").arg(sr.ciHalf, 0, 'f', 1),
        });
        shown++;
    }
    if (!topRows.isEmpty()) {
        static const QVector<DiagnosticFormatter::ColSpec> kTopCols = {
            {"Rank",    4, true},
            {"Server", 28, false},
            {"CC",      3, false},
            {"IP",     16, false},
            {"TTFB",    8, true},
            {"95% CI",  8, true},
        };
        out.append(DiagnosticFormatter::formatTable(kTopCols, topRows));
    }
    out.append(QString());

    // ── Phase 3: Best server ─────────────────────────────────────
    if (result.servers.isEmpty()) {
        out.append(QStringLiteral("No reachable server found"));
        r.summary = QStringLiteral("No internet connectivity");
        r.status = DiagStatus::Fail;
        r.rawOutput = out.join('\n'); r.details = r.rawOutput;
        r.durationMs = t.elapsed(); return r;
    }

    const ServerResult& best = result.servers[0];
    QString bestKey = best.host + QStringLiteral(":") + QString::number(best.port);
    auto bestMeta = metaByKey.constFind(bestKey);
    QString bestIp = resolveIp(best.host);

    out.append(QStringLiteral("── Phase 3: Best Server ──"));
    out.append(QStringLiteral("  Name:    %1").arg(bestMeta != metaByKey.cend() ? bestMeta->name : best.host));
    if (bestMeta != metaByKey.cend() && !bestMeta->sponsor.isEmpty())
        out.append(QStringLiteral("  Sponsor: %1").arg(bestMeta->sponsor));
    out.append(QStringLiteral("  Host:    %1:%2").arg(best.host).arg(best.port));
    out.append(QStringLiteral("  IP:      %1").arg(bestIp.isEmpty() ? QStringLiteral("(unresolved)") : bestIp));
    out.append(QStringLiteral("  Country: %1").arg(best.country));
    out.append(QStringLiteral("  TTFB:    %1ms (95% CI ±%2ms, %3 rounds)")
        .arg(best.ttfbMs, 0, 'f', 1).arg(best.ciHalf, 0, 'f', 1).arg(best.rounds));
    out.append(QString());

    // ── Phase 4: Speed Test ──────────────────────────────────────
    out.append(QStringLiteral("── Phase 4: Speed Test ──"));
    out.append(QStringLiteral("Server: %1:%2").arg(best.host).arg(best.port));

    // Pre-check: DNS resolution (reuses Phase 2 result, no redundant lookup)
    if (bestIp.isEmpty())
        out.append(QStringLiteral("  DNS:     ✗ failed — hostname not resolved"));
    else
        out.append(QStringLiteral("  DNS:     ✓ %1").arg(bestIp));

    // Pre-check: TCP ping
    int pingMs = G1G2G3Native::tcpPingMs(best.host, best.port);
    if (pingMs < 0)
        out.append(QStringLiteral("  Ping:    ✗ TCP connect failed"));
    else
        out.append(QStringLiteral("  Ping:    ✓ %1ms TCP connect").arg(pingMs));
    out.append(QString());

    // ── Tiered speed test ──
    struct Tier { int bytes; const char* label; };
    const Tier kDlTiers[] = {
        {  64000, "64 KB" },
        { 256000, "256 KB" },
        {1000000, "1 MB" },
    };
    const Tier kUlTiers[] = {
        {  64000, "64 KB" },
        { 256000, "256 KB" },
        {1000000, "1 MB" },
    };

    bool anyDlOk = false, anyUlOk = false;
    double bestDlMbps = 0, bestUlMbps = 0;

    static const QVector<DiagnosticFormatter::ColSpec> kSpeedCols = {
        {"Size",     7, false},
        {"Speed",   10,  true},
        {"Time",     6,  true},
        {"Bytes",    8,  true},
        {"Status",  18, false},
    };

    // Download table
    out.append(QStringLiteral("  Download:"));
    out.append(QString());
    {
        QList<QStringList> dlRows;
        for (const auto& tier : kDlTiers) {
            QString dlUrl = QStringLiteral("http://%1:%2/download?size=%3")
                .arg(best.host).arg(best.port).arg(tier.bytes);
            SpeedResult dl = httpDownload(dlUrl, tier.bytes, 15000);
            if (dl.ok && dl.mbps > 0.01) {
                dlRows.append({
                    QString::fromLatin1(tier.label),
                    QStringLiteral("%1 Mbps").arg(dl.mbps, 0, 'f', 1),
                    QStringLiteral("%1ms").arg(dl.durationMs),
                    QString::number(dl.bytes),
                    QStringLiteral("✓"),
                });
                anyDlOk = true;
                if (dl.mbps > bestDlMbps) bestDlMbps = dl.mbps;
            } else {
                QString err = dl.error.isEmpty() ? QStringLiteral("unknown error") : dl.error;
                dlRows.append({
                    QString::fromLatin1(tier.label),
                    QStringLiteral("—"),
                    QStringLiteral("—"),
                    QStringLiteral("—"),
                    err,
                });
            }
        }
        out.append(DiagnosticFormatter::formatTable(kSpeedCols, dlRows));
    }
    out.append(QString());

    // Upload table
    out.append(QStringLiteral("  Upload:"));
    out.append(QString());
    {
        QList<QStringList> ulRows;
        for (const auto& tier : kUlTiers) {
            QString ulUrl = QStringLiteral("http://%1:%2/upload").arg(best.host).arg(best.port);
            SpeedResult ul = httpUpload(ulUrl, tier.bytes, 15000);
            if (ul.ok && ul.mbps > 0.01) {
                ulRows.append({
                    QString::fromLatin1(tier.label),
                    QStringLiteral("%1 Mbps").arg(ul.mbps, 0, 'f', 1),
                    QStringLiteral("%1ms").arg(ul.durationMs),
                    QString::number(ul.bytes),
                    QStringLiteral("✓"),
                });
                anyUlOk = true;
                if (ul.mbps > bestUlMbps) bestUlMbps = ul.mbps;
            } else {
                QString err = ul.error.isEmpty() ? QStringLiteral("unknown error") : ul.error;
                ulRows.append({
                    QString::fromLatin1(tier.label),
                    QStringLiteral("—"),
                    QStringLiteral("—"),
                    QStringLiteral("—"),
                    err,
                });
            }
        }
        out.append(DiagnosticFormatter::formatTable(kSpeedCols, ulRows));
    }
    out.append(QString());

    // ── Summary ──────────────────────────────────────────────────
    if (anyDlOk && anyUlOk) {
        r.summary = QStringLiteral("Connected — %1 (%2ms, ↓%3/↑%4 Mbps)")
            .arg(result.physicalCountry).arg(best.ttfbMs, 0, 'f', 0)
            .arg(bestDlMbps, 0, 'f', 1).arg(bestUlMbps, 0, 'f', 1);
        r.status = DiagStatus::Pass;
    } else if (anyDlOk) {
        r.summary = QStringLiteral("Connected — %1 (%2ms, ↓%3 Mbps, upload N/A)")
            .arg(result.physicalCountry).arg(best.ttfbMs, 0, 'f', 0)
            .arg(bestDlMbps, 0, 'f', 1);
        r.status = DiagStatus::Warning;
    } else if (anyUlOk) {
        r.summary = QStringLiteral("Connected — %1 (%2ms, download N/A, ↑%3 Mbps)")
            .arg(result.physicalCountry).arg(best.ttfbMs, 0, 'f', 0)
            .arg(bestUlMbps, 0, 'f', 1);
        r.status = DiagStatus::Warning;
    } else {
        r.summary = QStringLiteral("Connected — %1 (%2ms, speed test failed)")
            .arg(result.physicalCountry).arg(best.ttfbMs, 0, 'f', 0);
        r.status = DiagStatus::Warning;
    }

    r.rawOutput = out.join('\n'); r.details = r.rawOutput;
    r.durationMs = t.elapsed();
    return r;
}

} // namespace G1G2G3Native
