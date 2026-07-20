// =============================================================================
// ProbeScheduler.h — Submit & merge probe requests into Database
// =============================================================================
#pragma once

#include "Diagnostics/Model/ProbeConfig.h"
#include <QStringList>

class ProbeDatabase;
class ProbeExecutor;

class ProbeScheduler {
public:
    ProbeScheduler(ProbeDatabase* db, ProbeExecutor* exec);

    // Submit probe request — non-blocking.
    // Automatically starts Executor if it's not already running.
    void submit(const ProbeConfig& config);

    // Resolve config.scope → list of "host:port" keys
    QStringList resolveHosts(const ProbeConfig& config) const;

private:
    ProbeDatabase* m_db;
    ProbeExecutor* m_exec;
};
