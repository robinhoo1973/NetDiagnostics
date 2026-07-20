// =============================================================================
// ProbeFeedback.cpp — Statistics & aggregation implementation
// =============================================================================
#include "Diagnostics/Model/ProbeFeedback.h"
#include "Diagnostics/Model/ProbeScheduler.h"
#include "Diagnostics/Model/GHelpers.h"
#include <algorithm>
#include <cmath>
#include <QMap>
#include <QSet>

ProbeFeedback::ProbeFeedback(ProbeDatabase* db, ProbeScheduler* sched)
    : m_db(db), m_sched(sched) {}

ProbeResult ProbeFeedback::get(const ProbeConfig& config) {
    // Step 1: resolve hosts + wait for all to be Done
    QStringList hosts = m_sched->resolveHosts(config);
    m_db->waitForCompletion(hosts);

    // Step 2: read raw TTFB → per-server statistics
    QVector<ServerResult> servers;
    for (const auto& host : hosts) {
        ServerTask task = m_db->read(host);
        if (task.results.isEmpty()) continue;
        servers.append(computeServerStats(task));
    }

    // Step 3: aggregation
    QVector<CountryResult> countries;
    QVector<RegionResult> regions;
    QString physicalCountry;

    switch (config.aggregation) {
        case ProbeConfig::ByCountry: {
            countries = aggregateByCountry(servers);
            if (!countries.isEmpty()) physicalCountry = countries[0].code;
            break;
        }
        case ProbeConfig::ByRegion: {
            regions = aggregateByRegion(servers);
            break;
        }
        case ProbeConfig::None:
            break;
    }

    // Step 4: sort by TTFB ASC + topN truncation
    std::sort(servers.begin(), servers.end(),
              [](const ServerResult& a, const ServerResult& b) { return a.ttfbMs < b.ttfbMs; });
    if (config.topN > 0 && servers.size() > config.topN) {
        servers.resize(config.topN);
    }

    return {servers, countries, regions, physicalCountry};
}

// ── Per-server statistics: HL median + MAD + 95% CI ─────────────────
ServerResult ProbeFeedback::computeServerStats(const ServerTask& task) const {
    ServerResult sr;
    sr.host = task.host;
    sr.port = task.port;
    sr.country = task.country;
    sr.regionTags = task.regionTags;

    const auto& raw = task.results;
    if (raw.isEmpty()) return sr;

    int n = raw.size();
    sr.rounds = n;
    sr.ok = true;

    // HL median
    double hl = G1G2G3Native::hodgesLehmann(raw);
    sr.ttfbMs = hl;

    if (n < 2) return sr;

    // MAD (Median Absolute Deviation)
    QVector<double> absDev(n);
    for (int i = 0; i < n; i++) absDev[i] = std::abs(raw[i] - hl);
    sr.mad = G1G2G3Native::median(absDev);

    // 95% CI using t-distribution (small-sample corrected)
    // t_0.025,df indexed by df = min(n-1, 6).  z=1.96 for n≥8.
    // df:  1      2      3      4      5      6       ≥7
    static const double T95[] = {0, 12.71, 4.30, 3.18, 2.78, 2.57, 2.45, 1.96};
    int df = std::min(n - 1, 6);
    double tval = (df < 7) ? T95[df] : 1.96;
    // MAD→SD consistency factor: 1.4826 under normality
    sr.ciHalf = tval * 1.4826 * mad / std::sqrt(static_cast<double>(n));

    return sr;
}

// ── GROUP BY country → HL → SORT ───────────────────────────────────
QVector<CountryResult> ProbeFeedback::aggregateByCountry(
    const QVector<ServerResult>& servers) const
{
    QMap<QString, QVector<double>> byCC;
    for (const auto& srv : servers) {
        if (srv.ok && srv.ttfbMs > 0) byCC[srv.country].append(srv.ttfbMs);
    }

    QVector<CountryResult> out;
    for (auto it = byCC.begin(); it != byCC.end(); ++it) {
        if (it.value().size() < 2) continue;
        CountryResult cr;
        cr.code = it.key();
        cr.hlMs = G1G2G3Native::hodgesLehmann(it.value());
        cr.serverCount = it.value().size();
        // attach matching servers
        for (const auto& srv : servers) {
            if (srv.country == cr.code) cr.servers.append(srv);
        }
        out.append(cr);
    }
    std::sort(out.begin(), out.end(),
              [](const CountryResult& a, const CountryResult& b) { return a.hlMs < b.hlMs; });
    return out;
}

// ── GROUP BY region → HL → SORT ────────────────────────────────────
QVector<RegionResult> ProbeFeedback::aggregateByRegion(
    const QVector<ServerResult>& servers) const
{
    QMap<QString, QVector<double>> byRegion;
    for (const auto& srv : servers) {
        if (!srv.ok || srv.ttfbMs <= 0) continue;
        for (const auto& tag : srv.regionTags) {
            byRegion[tag].append(srv.ttfbMs);
            break; // top-level tag only
        }
    }

    QVector<RegionResult> out;
    for (auto it = byRegion.begin(); it != byRegion.end(); ++it) {
        if (it.value().size() < 2) continue;
        RegionResult rr;
        rr.tag = it.key();
        rr.hlMs = G1G2G3Native::hodgesLehmann(it.value());
        rr.serverCount = it.value().size();
        QSet<QString> ccs;
        for (const auto& srv : servers)
            if (srv.regionTags.contains(rr.tag)) ccs.insert(srv.country);
        rr.countryCount = ccs.size();
        for (const auto& srv : servers)
            if (srv.regionTags.contains(rr.tag)) rr.servers.append(srv);
        out.append(rr);
    }
    std::sort(out.begin(), out.end(),
              [](const RegionResult& a, const RegionResult& b) { return a.hlMs < b.hlMs; });
    return out;
}
