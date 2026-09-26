// =============================================================================
// ProbeFeedback.h — Statistics & aggregation
//
// Reads raw TTFB data from Database, computes per-server HL/MAD/CI,
// aggregates by country/region, applies topN truncation, returns ProbeResult.
// =============================================================================
#pragma once

#include "Diagnostics/Model/ProbeConfig.h"
#include "Common/Services/ProbeDatabase.h"  // for ProbeDatabase::Task
#include <atomic>   // get() 取消指针（自包含，5WHY 2026-09-26）

class ProbeScheduler;

class ProbeFeedback {
public:
    ProbeFeedback(ProbeDatabase* db, ProbeScheduler* sched);

    // Block until all hosts done, then compute statistics, aggregate, return.
    // cancelled 可空：置位即提前返回空结果（5WHY 2026-09-26 取消解堵）。
    ProbeResult get(const ProbeConfig& config, const std::atomic<bool>* cancelled = nullptr);

private:
    ServerResult computeServerStats(const ProbeDatabase::Task& task) const;
    QVector<CountryResult> aggregateByCountry(const QVector<ServerResult>& servers) const;
    QVector<RegionResult> aggregateByRegion(const QVector<ServerResult>& servers) const;

    ProbeDatabase* m_db;
    ProbeScheduler* m_sched;
};
