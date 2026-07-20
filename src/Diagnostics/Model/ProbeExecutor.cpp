// =============================================================================
// ProbeExecutor.cpp — Persistent probe worker thread implementation
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

ProbeExecutor::~ProbeExecutor() { shutdown(); }

void ProbeExecutor::notify() {
    QMutexLocker lock(&m_mutex);
    m_condition.wakeOne();
}

void ProbeExecutor::shutdown() {
    m_shutdown.store(true);
    notify();
    if (isRunning()) {
        wait(5000);
        if (isRunning()) terminate();
    }
}

void ProbeExecutor::run() {
    while (!m_shutdown.load()) {
        // Wait for notification
        {
            QMutexLocker lock(&m_mutex);
            while (!m_db->hasWaitingTasks() && !m_shutdown.load()) {
                m_condition.wait(&m_mutex, 1000);
            }
        }
        if (m_shutdown.load()) break;

        // Fetch a batch of Waiting tasks from Database
        QVector<ServerTask> batch = m_db->fetchWaiting(512);
        if (batch.isEmpty()) continue;

        // Build SpeedTest database for metadata lookup
        G1G2G3Native::SpeedTest st;

        // Parallel probe using std::thread pool
        std::atomic<int> workIdx{0};
        const int kThreads = 10;
        std::vector<std::thread> threads;
        threads.reserve(kThreads);

        for (int t = 0; t < kThreads; t++) {
            threads.emplace_back([&]() {
                while (true) {
                    int idx = workIdx.fetch_add(1);
                    if (idx >= batch.size()) break;

                    ServerTask& task = batch[idx];

                    // ① Parse host:port
                    int colon = task.key.lastIndexOf(':');
                    QString host = task.key.left(colon);
                    int port = task.key.mid(colon + 1).toInt();
                    if (port <= 0) port = 80;

                    // ② Lookup metadata from SpeedTest database
                    QString country = "XX";
                    QStringList regionTags;
                    for (const auto& srv : st.allServers()) {
                        if (srv.host == host && srv.port == port) {
                            country = srv.country;
                            break;
                        }
                    }
                    regionTags = G1G2G3Native::GeoProbe::regionTags(country);

                    // ③ Execute TTFB probes
                    QVector<double> results;
                    results.reserve(task.rounds);
                    QString path = "/";
                    for (int r = 0; r < task.rounds; r++) {
                        double ttfb = G1G2G3Native::httpTtfb(host, port, path, 3000, 8);
                        if (ttfb >= 0) results.append(ttfb);
                    }

                    // ④ Write raw results + metadata back to Database
                    m_db->writeResults(task.key, results, country, regionTags);
                }
            });
        }
        for (auto& t : threads) t.join();
    }
}
