// =============================================================================
// ProbeScheduler.cpp — Submit & merge probe requests
// =============================================================================
#include "Diagnostics/Model/ProbeScheduler.h"
#include "Common/Services/ProbeDatabase.h"
#include "Diagnostics/Model/ProbeExecutor.h"
#include "Diagnostics/Model/G3/G3InternetDns.h"
#include "Diagnostics/Model/GeoProbe.h"

ProbeScheduler::ProbeScheduler(ProbeDatabase* db, ProbeExecutor* exec)
    : m_db(db), m_exec(exec) {}

void ProbeScheduler::submit(const ProbeConfig& config) {
    QStringList hosts = resolveHosts(config);
    for (const auto& host : hosts) {
        m_db->upsert(host, config.rounds);
    }

    // QThread::start() is a no-op if already running — safe to call unconditionally
    m_exec->start();
}

QStringList ProbeScheduler::resolveHosts(const ProbeConfig& config) const {
    switch (config.scope) {
        case ProbeConfig::Global: {
            QStringList keys;
            G1G2G3Native::SpeedTest st;
            for (const auto& srv : st.allServers()) {
                keys.append(srv.host + ":" + QString::number(srv.port));
            }
            return keys;
        }
        case ProbeConfig::ByCountry: {
            QStringList keys;
            G1G2G3Native::SpeedTest st;
            for (const auto& srv : st.serversForCountry(config.scopeValue)) {
                keys.append(srv.host + ":" + QString::number(srv.port));
            }
            return keys;
        }
        case ProbeConfig::ByRegion: {
            QStringList keys;
            G1G2G3Native::SpeedTest st;
            for (const auto& srv : st.allServers()) {
                auto tags = G1G2G3Native::GeoProbe::regionTags(srv.country);
                if (tags.contains(config.scopeValue)) {
                    keys.append(srv.host + ":" + QString::number(srv.port));
                }
            }
            return keys;
        }
        case ProbeConfig::ByServers:
            return config.serverHosts;
    }
    return {};
}
