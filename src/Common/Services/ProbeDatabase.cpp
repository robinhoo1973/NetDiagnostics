// =============================================================================
// ProbeDatabase.cpp — Thread-safe probe task table implementation
// =============================================================================
#include "Common/Services/ProbeDatabase.h"

void ProbeDatabase::upsert(const QString& key, int rounds) {
    QMutexLocker lock(&m_mutex);
    auto it = m_table.find(key);

    if (it == m_table.end()) {
        ProbeDatabase::Task t;
        t.key = key; t.rounds = rounds; t.status = ProbeDatabase::Task::Waiting;
        m_table.insert(key, t);
        return;
    }

    ProbeDatabase::Task& t = it.value();

    if (t.status == ProbeDatabase::Task::Done && t.rounds >= rounds) {
        return;  // already satisfied
    }

    if (t.status == ProbeDatabase::Task::Done && t.rounds < rounds) {
        t.rounds = rounds;
        t.results.clear();
        t.status = ProbeDatabase::Task::Waiting;
        return;
    }

    // Waiting or Running — bump round count if needed
    if (t.rounds < rounds) {
        t.rounds = rounds;
    }
}

QVector<ProbeDatabase::Task> ProbeDatabase::fetchWaiting(int maxCount) {
    QMutexLocker lock(&m_mutex);
    QVector<ProbeDatabase::Task> batch;
    for (auto& t : m_table) {
        if (t.status == ProbeDatabase::Task::Waiting) {
            t.status = ProbeDatabase::Task::Running;
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
    ProbeDatabase::Task& t = it.value();
    t.results.append(results);
    t.country = country;
    t.regionTags = regionTags;
    t.status = ProbeDatabase::Task::Done;
    m_condition.wakeAll();
}

ProbeDatabase::ProbeDatabase::Task ProbeDatabase::read(const QString& key) const {
    QMutexLocker lock(&m_mutex);
    return m_table.value(key);
}

void ProbeDatabase::waitForCompletion(const QStringList& keys) {
    QMutexLocker lock(&m_mutex);
    QDeadlineTimer deadline(60'000);  // 60s timeout guard
    while (!deadline.hasExpired()) {
        bool allDone = true;
        for (const auto& key : keys) {
            auto it = m_table.find(key);
            if (it == m_table.end() || it.value().status != ProbeDatabase::Task::Done) {
                allDone = false;
                break;
            }
        }
        if (allDone) break;
        m_condition.wait(&m_mutex, 1000);  // wake every 1s to re-check
    }
}

void ProbeDatabase::clear() {
    QMutexLocker lock(&m_mutex);
    m_table.clear();
}

bool ProbeDatabase::hasWaitingTasks() const {
    QMutexLocker lock(&m_mutex);
    for (const auto& t : m_table) {
        if (t.status == ProbeDatabase::Task::Waiting) return true;
    }
    return false;
}
