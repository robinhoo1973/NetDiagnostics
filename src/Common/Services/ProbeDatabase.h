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
        // 5WHY (2026-09-05 清表代际): 插入时烙上表代际——clear() 递增代际后，
        // 上一轮迟到的执行器 worker 按 key 写回会命中新一轮同名新任务
        // （数据串轮）。写回时校验快照代际，不匹配即丢弃。
        qint64 generation = 0;
    };

    ProbeDatabase() = default;

    // ── Scheduler API ────────────────────────────────────────────────
    void upsert(const QString& key, int rounds);

    // ── Executor API ─────────────────────────────────────────────────
    QVector<Task> fetchWaiting(int maxCount);
    // forceDone：执行器停机/线程创建失败等终局路径——跳过回合数校验直接
    // 落 Done（否则重入队后无人消费，waitForCompletion 等满 120s 上限）。
    // taskGeneration：fetch 快照携带的清表代际——clear() 后同名新任务拒绝
    // 旧轮迟到写（< 0 跳过校验，兼容不带快照的调用方）。
    void writeResults(const QString& key, const QVector<double>& results,
                      const QString& country, const QStringList& regionTags,
                      bool forceDone = false, qint64 taskGeneration = -1);

    // ── Feedback API ─────────────────────────────────────────────────
    Task read(const QString& key) const;
    void waitForCompletion(const QStringList& keys);

    // ── Executor idle API ─────────────────────────────────────────────
    // 条件变量等待：表内出现 Waiting 任务 / stop 置位 / wake() 即返回。
    // 不限时——每个 Waiting 跃迁（upsert/writeResults）与 wake() 均持锁
    // 唤醒，检查-等待同锁无丢失窗口（见 ProbeExecutor.cpp 5WHY 2026-09-05）。
    void waitForNewWork(const std::shared_ptr<std::atomic<bool>>& stop);
    // 唤醒 idle 等待的执行器（ProbeExecutor::requestStop 置 stop 标志后调用，
    // 否则执行器要等下一次 Waiting 跃迁才感知停机）。
    void wake();

    // ── Lifecycle ────────────────────────────────────────────────────
    void clear();

private:
    QHash<QString, Task> m_table;
    mutable QMutex m_mutex;
    QWaitCondition m_condition;
    // 5WHY (2026-09-05 清表代际): clear() 递增——在途 waitForCompletion 凭代际
    // 变化立即返回（键已被清空且晚到写入被丢弃，等满 120s 上限纯属空转；
    // 最坏情况旧套件析构在主线程等池线程 → UI 冻结至上限）。
    qint64 m_generation = 0;
};
