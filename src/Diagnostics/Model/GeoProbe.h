// =============================================================================
// GeoProbe.h — Geographic Probe Engine (facade)
//
// Composes four internal components:
//   ProbeScheduler — submit & merge requests
//   ProbeDatabase  — thread-safe task table
//   ProbeExecutor  — persistent probe worker thread
//   ProbeFeedback  — statistics & aggregation
//
// Public API: only probe() and getFeedback().
// =============================================================================
#pragma once

#include "Diagnostics/Model/ProbeConfig.h"
#include <QObject>

class ProbeDatabase;
class ProbeScheduler;
class ProbeExecutor;
class ProbeFeedback;

class GeoProbe {
public:
    GeoProbe();
    ~GeoProbe();

    // Submit probe request — non-blocking
    void probe(const ProbeConfig& config);

    // Block until results ready, then compute statistics, aggregate, return
    ProbeResult getFeedback(const ProbeConfig& config);

    // Access internal Database (for AppState::runDiagnostics → clear)
    ProbeDatabase* database() const { return m_database; }

    // Static: region tag mapping (shared by Executor, Feedback, Scheduler)
    static QStringList regionTags(const QString& countryCode);

private:
    ProbeDatabase*  m_database  = nullptr;
    ProbeScheduler* m_scheduler = nullptr;
    ProbeExecutor*  m_executor  = nullptr;
    ProbeFeedback*  m_feedback  = nullptr;
};
