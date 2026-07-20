// =============================================================================
// ProbeFeedback.h — Statistics & aggregation
//
// Reads raw TTFB data from Database, computes per-server HL/MAD/CI,
// aggregates by country/region, applies topN truncation, returns ProbeResult.
// =============================================================================
#pragma once

#include "Diagnostics/Model/ProbeConfig.h"
#include "Common/Services/ProbeDatabase.h"  // for ProbeDatabase::Task

class ProbeScheduler;

class ProbeFeedback {
public:
    ProbeFeedback(ProbeDatabase* db, ProbeScheduler* sched);

    // Block until all hosts done, then compute statistics, aggregate, return
    ProbeResult get(const ProbeConfig& config);

private:
    ServerResult computeServerStats(const ProbeDatabase::Task& task) const;
    QVector<CountryResult> aggregateByCountry(const QVector<ServerResult>& servers) const;
    QVector<RegionResult> aggregateByRegion(const QVector<ServerResult>& servers) const;

    ProbeDatabase* m_db;
    ProbeScheduler* m_sched;
};
