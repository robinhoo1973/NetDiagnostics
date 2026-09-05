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
        ProbeDatabase::Task task = m_db->read(host);
        if (task.results.isEmpty()) continue;
        servers.append(computeServerStats(task));
    }

    // Step 3: aggregation
    QVector<CountryResult> countries;
    QVector<RegionResult> regions;
    QString physicalCountry;

    switch (config.aggregation) {
        case ProbeConfig::Aggregation::ByCountry: {
            countries = aggregateByCountry(servers);
            if (!countries.isEmpty()) physicalCountry = countries[0].code;
            break;
        }
        case ProbeConfig::Aggregation::ByRegion: {
            regions = aggregateByRegion(servers);
            break;
        }
        case ProbeConfig::Aggregation::None:
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
ServerResult ProbeFeedback::computeServerStats(const ProbeDatabase::Task& task) const {
    ServerResult sr;
    // 5WHY: task.host and task.port are documented as "filled by Executor"
    // but ProbeExecutor::writeResults() never writes them — it only writes
    // results, country, and regionTags.  task.key is the canonical source:
    // ProbeScheduler sets it to "host:port" at upsert time.  Parse it here
    // so ServerResult always has valid host/port regardless of Executor.
    int colon = task.key.lastIndexOf(':');
    sr.host = (colon > 0) ? task.key.left(colon) : task.key;
    sr.port = (colon > 0) ? task.key.mid(colon + 1).toInt() : 80;
    sr.country = task.country;
    sr.regionTags = task.regionTags;

    const auto& raw = task.results;
    if (raw.isEmpty()) return sr;

    int n = raw.size();
    sr.rounds = n;
    sr.ok = true;

    // HL median
    double hl = SystemDiagnostics::hodgesLehmann(raw);
    sr.ttfbMs = hl;

    if (n < 2) return sr;

    // MAD (Median Absolute Deviation)
    QVector<double> absDev(n);
    for (int i = 0; i < n; i++) absDev[i] = std::abs(raw[i] - hl);
    sr.mad = SystemDiagnostics::median(absDev);

    // 95% CI using t-distribution (small-sample corrected)
    // t_0.025,df indexed by df = min(n-1, 6).  z=1.96 for n≥8.
    // df:  1      2      3      4      5      6       ≥7
    static const double T95[] = {0, 12.71, 4.30, 3.18, 2.78, 2.57, 2.45, 1.96};
    // 5WHY (2026-09-05 z=1.96 不可达): 曾 df = min(n-1, 6)——df 上限 6 使
    // `(df < 7) ? T95[df] : 1.96` 的 1.96 分支恒不可达，n≥8 样本全部用
    // t(6)=2.45，95% CI 被系统性夸大 ~25%。上限 7：df 0..6 走表，df≥7
    // 走正态近似 z=1.96（与注释语义一致）。
    int df = std::min(n - 1, 7);
    double tval = (df <= 6) ? T95[df] : 1.96;
    // MAD→SD consistency factor: 1.4826 under normality
    sr.ciHalf = tval * 1.4826 * sr.mad / std::sqrt(static_cast<double>(n));

    return sr;
}

// ── GROUP BY country → HL → SORT ───────────────────────────────────
QVector<CountryResult> ProbeFeedback::aggregateByCountry(
    const QVector<ServerResult>& servers) const
{
    // Single-pass: group servers by country, collect TTFB values to compute HL later
    struct Group { QVector<ServerResult> srv; QVector<double> ttfb; };
    QMap<QString, Group> byCC;
    for (const auto& srv : servers) {
        if (srv.ok && srv.ttfbMs > 0) {
            auto& g = byCC[srv.country];
            g.srv.append(srv);
            g.ttfb.append(srv.ttfbMs);
        }
    }

    QVector<CountryResult> out;
    for (auto it = byCC.begin(); it != byCC.end(); ++it) {
        if (it.value().ttfb.size() < 2) continue;
        CountryResult cr;
        cr.code = it.key();
        cr.hlMs = SystemDiagnostics::hodgesLehmann(it.value().ttfb);
        cr.serverCount = it.value().ttfb.size();
        cr.servers = std::move(it.value().srv);
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
    // 5WHY: the original code re-scanned ALL servers once per region
    // (O(R×S)) to build countryCount and servers, after already having
    // grouped by tag in the first pass.  A single pass now collects the
    // ttfb values, the server list, and the country set per region in one
    // place.  "Top-level tag only" behaviour is kept: regionTags[0] is the
    // primary region; aggregating by every sub-region would fragment the
    // ranking into near-duplicate rows.
    struct RegionAccum {
        QVector<double> ttfb;
        QVector<ServerResult> servers;
        QSet<QString> countries;
    };
    QMap<QString, RegionAccum> byRegion;
    for (const auto& srv : servers) {
        if (!srv.ok || srv.ttfbMs <= 0 || srv.regionTags.isEmpty()) continue;
        const QString topTag = srv.regionTags.first();
        auto& acc = byRegion[topTag];
        acc.ttfb.append(srv.ttfbMs);
        acc.servers.append(srv);
        acc.countries.insert(srv.country);
    }

    QVector<RegionResult> out;
    out.reserve(byRegion.size());
    for (auto it = byRegion.begin(); it != byRegion.end(); ++it) {
        if (it.value().ttfb.size() < 2) continue;
        RegionResult rr;
        rr.tag = it.key();
        rr.hlMs = SystemDiagnostics::hodgesLehmann(it.value().ttfb);
        rr.serverCount = it.value().ttfb.size();
        rr.countryCount = it.value().countries.size();
        rr.servers = it.value().servers;
        out.append(rr);
    }
    std::sort(out.begin(), out.end(),
              [](const RegionResult& a, const RegionResult& b) { return a.hlMs < b.hlMs; });
    return out;
}
