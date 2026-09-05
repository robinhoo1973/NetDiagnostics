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
#include <atomic>
#include <memory>

// ── Probe executor worst-case completion budget ────────────────────────
// 5WHY: waitForCompletion() used a bare 120'000 ms with no derivation, and
// the executor's real worst case is a moving target. Derivation:
//   • one probe round ≈ 3s connect + 8s read = 11s (ProbeExecutor uses
//     httpTtfb(..., 3000, 8))
//   • ProbeConfig::kDefaultRounds = 3 → ~33s per server thread
//   • ~138 servers / 64-thread batches = 3 waves → ~99s worst case
//   • rounds 竞态修复的重入队（上限 1 次）至多加一波缺额续测
//     （失败服务器单轮 ~3s × ≤3 波 ≈ ≤9s）——仍在预算内
// Keep this ≥ 1.2 × worst case so a wedged executor times out instead of
// returning incomplete data silently. Callers skip empty results gracefully.
static constexpr qint64 kWaitForCompletionTimeoutMs = 120'000;

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
        // 5WHY (2026-09-05 rounds 竞态修复): 写回时回合数未满足则重入队——
        // 每键重试上限（失败服务器不无限重入、waitForCompletion 仍有界）。
        int attempts = 0;
    };

    ProbeDatabase() = default;

    // ── Scheduler API ────────────────────────────────────────────────
    void upsert(const QString& key, int rounds);

    // ── Executor API ─────────────────────────────────────────────────
    QVector<Task> fetchWaiting(int maxCount);
    // forceDone：执行器停机/线程创建失败等终局路径——跳过回合数校验直接
    // 落 Done（否则重入队后无人消费，waitForCompletion 等满 120s 上限）。
    void writeResults(const QString& key, const QVector<double>& results,
                      const QString& country, const QStringList& regionTags,
                      bool forceDone = false);

    // ── Feedback API ─────────────────────────────────────────────────
    Task read(const QString& key) const;
    void waitForCompletion(const QStringList& keys);

    // ── Executor idle API ─────────────────────────────────────────────
    // 条件变量等待：表内出现 Waiting 任务 / stop 置位 / timeoutMs 超时即
    // 返回（执行器空闲期不再盲轮询，见 ProbeExecutor.cpp 5WHY 2026-09-05）。
    void waitForNewWork(const std::shared_ptr<std::atomic<bool>>& stop, int timeoutMs);

    // ── Lifecycle ────────────────────────────────────────────────────
    void clear();

private:
    QHash<QString, Task> m_table;
    mutable QMutex m_mutex;
    QWaitCondition m_condition;
};
