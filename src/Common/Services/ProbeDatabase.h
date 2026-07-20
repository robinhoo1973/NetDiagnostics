// =============================================================================
// ProbeDatabase.h — Thread-safe probe task table
//
// Stores per-server probe tasks with status state machine:
//   Waiting → Running → Done (→ Waiting on requeue)
//
// Used by:
//   ProbeScheduler (upsert tasks)
//   ProbeExecutor   (fetchWaiting + writeResults)
//   ProbeFeedback   (read + waitForCompletion)
// =============================================================================
#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMutex>
#include <QWaitCondition>

// ── Database class ───────────────────────────────────────────────────
class ProbeDatabase {
public:
    // ── Task record ──────────────────────────────────────────────────
    struct Task {
        QString key;              // primary key: "host:port"
        QString host;             // hostname (filled by Executor)
        int port = 80;            // port (filled by Executor)
        enum Status { Waiting, Running, Done };
        Status status = Waiting;
        int rounds = 0;           // requested measurement rounds
        QVector<double> results;  // raw TTFB measurements in ms
        QString country;          // server country (filled by Executor)
        QStringList regionTags;   // region tags (filled by Executor)
    };

    ProbeDatabase() = default;

    // ── Scheduler API ────────────────────────────────────────────────
    void upsert(const QString& key, int rounds);

    // ── Executor API ─────────────────────────────────────────────────
    QVector<Task> fetchWaiting(int maxCount);
    void writeResults(const QString& key, const QVector<double>& results,
                      const QString& country, const QStringList& regionTags);

    // ── Feedback API ─────────────────────────────────────────────────
    Task read(const QString& key) const;
    void waitForCompletion(const QStringList& keys);

    // ── Lifecycle ────────────────────────────────────────────────────
    void clear();
    bool hasWaitingTasks() const;

private:
    QHash<QString, Task> m_table;
    mutable QMutex m_mutex;
    QWaitCondition m_condition;
};
