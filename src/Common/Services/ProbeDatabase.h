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

// ── Internal task record ─────────────────────────────────────────────
struct ServerTask {
    QString key;                 // primary key: "host:port"
    QString host;                // hostname (filled by Executor)
    int port = 80;               // port (filled by Executor)
    enum Status { Waiting, Running, Done };
    Status status = Waiting;
    int rounds = 0;              // requested measurement rounds
    QVector<double> results;     // raw TTFB measurements in ms
    QString country;             // server country (filled by Executor)
    QStringList regionTags;      // region tags (filled by Executor)
};

// ── Database class ───────────────────────────────────────────────────
class ProbeDatabase {
public:
    ProbeDatabase() = default;

    // ── Scheduler API ────────────────────────────────────────────────
    enum UpsertAction { Created, NoOp, Requeued, Merged };
    struct UpsertResult { UpsertAction action; ServerTask task; };
    UpsertResult upsert(const QString& key, int rounds);

    // ── Executor API ─────────────────────────────────────────────────
    QVector<ServerTask> fetchWaiting(int maxCount);
    void writeResults(const QString& key, const QVector<double>& results,
                      const QString& country, const QStringList& regionTags);

    // ── Feedback API ─────────────────────────────────────────────────
    ServerTask read(const QString& key) const;
    void waitForCompletion(const QStringList& keys);

    // ── Lifecycle ────────────────────────────────────────────────────
    void clear();
    bool hasWaitingTasks() const;

private:
    QHash<QString, ServerTask> m_table;
    mutable QMutex m_mutex;
    QWaitCondition m_condition;  // wakes Feedback when tasks complete
};
