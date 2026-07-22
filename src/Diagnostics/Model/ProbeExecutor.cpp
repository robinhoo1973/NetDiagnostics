// =============================================================================
// ProbeExecutor.cpp — On-demand probe worker thread
// =============================================================================
#include "Diagnostics/Model/ProbeExecutor.h"
#include "Common/Services/ProbeDatabase.h"
#include "Diagnostics/Model/GeoProbe.h"
#include "Diagnostics/Model/GHelpers.h"
#include <QDateTime>
#include <thread>
#include <vector>

ProbeExecutor::ProbeExecutor(ProbeDatabase* db, QObject* parent)
    : QThread(parent), m_db(db) {}

ProbeExecutor::~ProbeExecutor() { requestStop(); }

void ProbeExecutor::requestStop() {
    m_stopRequested.store(true);
    if (isRunning()) {
        wait(5000);
        if (isRunning()) terminate();
    }
}

void ProbeExecutor::run() {
    // Pre-build host→metadata lookup table (single-threaded, then shared read-only)
    struct Meta { QString country; QStringList regionTags; };
    QHash<QString, Meta> metaByHost;
    for (const auto& srv : GeoProbe::allServers()) {
        Meta m; m.country = srv.country;
        m.regionTags = GeoProbe::regionTags(srv.country);
        metaByHost.insert(srv.host + ":" + QString::number(srv.port), m);
    }

    while (!m_stopRequested.load()) {
        QVector<ProbeDatabase::Task> batch = m_db->fetchWaiting(512);
        if (batch.isEmpty()) {
            QThread::msleep(100);  // idle: brief yield before re-check
            continue;
        }

        // Pre-built QHash is read-only during the parallel loop — thread-safe.
        const auto& lookup = metaByHost;
        // All 138 threads block mostly on recv(), so context-switch overhead is
        // negligible compared to 3-8s network wait per server.
        std::vector<std::thread> threads;
        threads.reserve(batch.size());

        for (int i = 0; i < batch.size(); i++) {
            if (m_stopRequested.load()) break;
            threads.emplace_back([this, &lookup, task = &batch[i]]() {
                int colon = task->key.lastIndexOf(':');
                QString host = task->key.left(colon);
                int port = task->key.mid(colon + 1).toInt();
                if (port <= 0) port = 80;

                auto it = lookup.find(task->key);
                QString country = (it != lookup.end()) ? it->country : "XX";
                QStringList regionTags = (it != lookup.end()) ? it->regionTags : QStringList();

                QVector<double> results;
                results.reserve(task->rounds);
                for (int r = 0; r < task->rounds; r++) {
                    double ttfb = SystemDiagnostics::httpTtfb(host, port, "/", 3000, 8);
                    if (ttfb >= 0) results.append(ttfb);
                }

                m_db->writeResults(task->key, results, country, regionTags);
            });
        }
        for (auto& t : threads) t.join();
    }
}
