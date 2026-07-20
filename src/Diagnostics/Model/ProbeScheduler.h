// =============================================================================
// ProbeScheduler.h — Submit & merge probe requests into Database
// =============================================================================
#pragma once

#include "Diagnostics/Model/ProbeConfig.h"
#include <QStringList>

class ProbeDatabase;

class ProbeScheduler {
public:
    explicit ProbeScheduler(ProbeDatabase* db);

    // Submit probe request — non-blocking
    void submit(const ProbeConfig& config);

    // Resolve config.scope → list of "host:port" keys
    QStringList resolveHosts(const ProbeConfig& config) const;

private:
    ProbeDatabase* m_db;
};
