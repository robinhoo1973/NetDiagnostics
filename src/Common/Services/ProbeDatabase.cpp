// =============================================================================
// ProbeDatabase.cpp — Thread-safe probe task table implementation
// =============================================================================
#include "Common/Services/ProbeDatabase.h"

void ProbeDatabase::upsert(const QString& key, int rounds) {
    QMutexLocker lock(&m_mutex);
    auto it = m_table.find(key);

    if (it == m_table.end()) {
        ServerTask t;
        t.key = key; t.rounds = rounds; t.status = ServerTask::Waiting;
        m_table.insert(key, t);
        return;
    }

    ServerTask& t = it.value();

    if (t.status == ServerTask::Done && t.rounds >= rounds) {
        return;  // already satisfied
    }

    if (t.status == ServerTask::Done && t.rounds < rounds) {
        t.rounds = rounds;
        t.results.clear();
        t.status = ServerTask::Waiting;
        return;
    }

    // Waiting or Running — bump round count if needed
    if (t.rounds < rounds) {
        t.rounds = rounds;
    }
}

QVector<ServerTask> ProbeDatabase::fetchWaiting(int maxCount) {
    QMutexLocker lock(&m_mutex);
    QVector<ServerTask> batch;
    for (auto& t : m_table) {
        if (t.status == ServerTask::Waiting) {
            t.status = ServerTask::Running;
            batch.append(t);
            if (batch.size() >= maxCount) break;
        }
    }
    return batch;
}

void ProbeDatabase::writeResults(const QString& key, const QVector<double>& results,
                                 const QString& country, const QStringList& regionTags) {
    QMutexLocker lock(&m_mutex);
    auto it = m_table.find(key);
    if (it == m_table.end()) return;
    ServerTask& t = it.value();
    t.results.append(results);
    t.country = country;
    t.regionTags = regionTags;
    t.status = ServerTask::Done;
    m_condition.wakeAll();
}

ProbeDatabase::ServerTask ProbeDatabase::read(const QString& key) const {
    QMutexLocker lock(&m_mutex);
    return m_table.value(key);
}

void ProbeDatabase::waitForCompletion(const QStringList& keys) {
    QMutexLocker lock(&m_mutex);
    while (true) {
        bool allDone = true;
        for (const auto& key : keys) {
            auto it = m_table.find(key);
            if (it == m_table.end() || it.value().status != ServerTask::Done) {
                allDone = false;
                break;
            }
        }
        if (allDone) break;
        m_condition.wait(&m_mutex);
    }
}

void ProbeDatabase::clear() {
    QMutexLocker lock(&m_mutex);
    m_table.clear();
}

bool ProbeDatabase::hasWaitingTasks() const {
    QMutexLocker lock(&m_mutex);
    for (const auto& t : m_table) {
        if (t.status == ServerTask::Waiting) return true;
    }
    return false;
}
