// =============================================================================
// ProbeExecutor.cpp — On-demand probe worker thread
// =============================================================================
#include "Diagnostics/Model/ProbeExecutor.h"
#include "Common/Services/ProbeDatabase.h"
#include "Diagnostics/Model/G3/G3InternetDns.h"
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
    G1G2G3Native::SpeedTest st;
    struct Meta { QString country; QStringList regionTags; };
    QHash<QString, Meta> metaByHost;
    for (const auto& srv : st.allServers()) {
        Meta m; m.country = srv.country;
        m.regionTags = G1G2G3Native::GeoProbe::regionTags(srv.country);
        metaByHost.insert(srv.host + ":" + QString::number(srv.port), m);
    }

    while (!m_stopRequested.load()) {
        QVector<ServerTask> batch = m_db->fetchWaiting(512);
        if (batch.isEmpty()) break;  // auto-stop

        // 5WHY: st.allServers() called from 10 threads caused data race
        // on QMap implicit-sharing refcount.  Pre-built QHash above is
        // read-only during the parallel loop — thread-safe.
        const auto& lookup = metaByHost;

        std::atomic<int> workIdx{0};
        const int kThreads = 10;
        std::vector<std::thread> threads;
        threads.reserve(kThreads);

        for (int t = 0; t < kThreads; t++) {
            threads.emplace_back([&]() {
                while (!m_stopRequested.load()) {
                    int idx = workIdx.fetch_add(1);
                    if (idx >= batch.size()) break;

                    ServerTask& task = batch[idx];

                    int colon = task.key.lastIndexOf(':');
                    QString host = task.key.left(colon);
                    int port = task.key.mid(colon + 1).toInt();
                    if (port <= 0) port = 80;

                    auto it = lookup.find(task.key);
                    QString country = (it != lookup.end()) ? it->country : "XX";
                    QStringList regionTags = (it != lookup.end()) ? it->regionTags : QStringList();

                    QVector<double> results;
                    results.reserve(task.rounds);
                    for (int r = 0; r < task.rounds; r++) {
                        double ttfb = G1G2G3Native::httpTtfb(host, port, "/", 3000, 8);
                        if (ttfb >= 0) results.append(ttfb);
                    }

                    m_db->writeResults(task.key, results, country, regionTags);
                }
            });
        }
        for (auto& t : threads) t.join();
    }
}
