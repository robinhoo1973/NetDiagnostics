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
        t.generation = m_generation;   // 清表代际烙入（writeResults 防串轮写）
        m_table.insert(key, t);
        m_condition.wakeAll();   // 唤醒空闲等待的执行器
        return;
    }

    ProbeDatabase::Task& t = it.value();

    if (t.status == ProbeDatabase::Task::Done && t.rounds >= rounds) {
        return;  // already satisfied
    }

    if (t.status == ProbeDatabase::Task::Done && t.rounds < rounds) {
        t.rounds = rounds;
        t.results.clear();
        t.attempts = 0;
        t.status = ProbeDatabase::Task::Waiting;
        m_condition.wakeAll();
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
                                 const QString& country, const QStringList& regionTags,
                                 bool forceDone, qint64 taskGeneration) {
    QMutexLocker lock(&m_mutex);
    auto it = m_table.find(key);
    if (it == m_table.end()) return;
    ProbeDatabase::Task& t = it.value();
    // 5WHY (2026-09-05 清表代际 串轮防护): clear() 后新一轮 upsert 会以同键
    // 重建任务——上一轮迟到的 worker（120s 超时后仍在飞行）按 key 写回会
    // 命中新任务、把旧轮测量混入新轮 CI/HL 表。快照代际不匹配即丢弃
    // （taskGeneration < 0 表示调用方未携带代际，跳过校验）。
    if (taskGeneration >= 0 && t.generation != taskGeneration) return;
    t.results.append(results);
    t.country = country;
    t.regionTags = regionTags;
    // 5WHY (2026-09-05 rounds 竞态): 执行器按 fetch 快照的旧 rounds 测量，
    // 并发 upsert 已把 Running 任务的 rounds 抬升——旧逻辑无条件落 Done，
    // 3 回合请求拿到 1 回合数据（CI/HL 表静默失真）且 Done 后永不重测。
    // 回合数未满足且未达重试上限 → 重入 Waiting，执行器下轮以"缺额"续测。
    // 终局路径（forceDone）直接落 Done 防死等。
    // 5WHY (2026-09-05 复核 重试上限=1): 上限曾拟 3——失败服务器多轮重试
    // 会把完成时间放大数倍并超出 kWaitForCompletionTimeoutMs 推导预算
    // （~99s 单波最坏），反馈层在超时处静默丢数据。竞态修复只需 1 次
    // 重入（缺额续测一轮即补足），上限 1 有界且覆盖全部修复语义。
    static constexpr int kMaxRequeue = 1;
    if (!forceDone && t.results.size() < t.rounds && t.attempts < kMaxRequeue) {
        ++t.attempts;
        t.status = ProbeDatabase::Task::Waiting;
    } else {
        t.status = ProbeDatabase::Task::Done;
    }
    m_condition.wakeAll();
}

ProbeDatabase::Task ProbeDatabase::read(const QString& key) const {
    QMutexLocker lock(&m_mutex);
    return m_table.value(key);
}

void ProbeDatabase::waitForNewWork(const std::shared_ptr<std::atomic<bool>>& stop,
                                   int timeoutMs) {
    QMutexLocker lock(&m_mutex);
    while (!stop->load(std::memory_order_acquire)) {
        for (auto it = m_table.cbegin(); it != m_table.cend(); ++it) {
            if (it.value().status == ProbeDatabase::Task::Waiting) return;
        }
        // 5WHY (2026-09-05 复核 空闲盲轮询): timeoutMs > 0 的定时等待会以
        // timeoutMs 周期反复全表扫描（进程余下生命周期空转）。每个 Waiting
        // 跃迁（upsert/writeResults）与 wake() 均在持锁下唤醒，检查-等待
        // 同锁无丢失窗口——不限时等待是安全的；timeoutMs <= 0 即不限时。
        if (timeoutMs > 0)
            m_condition.wait(&m_mutex, timeoutMs);
        else
            m_condition.wait(&m_mutex);
    }
}

void ProbeDatabase::wake() {
    QMutexLocker lock(&m_mutex);
    m_condition.wakeAll();
}

void ProbeDatabase::waitForCompletion(const QStringList& keys) {
    QMutexLocker lock(&m_mutex);
    // 5WHY: the guard was 60s, but ProbeExecutor's worst case is ~99s
    // (3 batches × 64 threads × up to 8s/round).  A 60s deadline silently
    // returned incomplete data and the feedback layer could report
    // "network unreachable" for servers that were simply still in flight.
    // The derived constant covers the executor's full worst case while
    // still bounding a wedged executor (the caller skips empty results
    // gracefully).
    QDeadlineTimer deadline(kWaitForCompletionTimeoutMs);
    // 5WHY (2026-09-05 清表代际): 新一轮 run 的 clear() 会清掉在途键且晚到
    // 写入被丢弃——键永久缺失、等满 120s 上限纯属空转（最坏：旧套件析构
    // 在主线程等池线程 → UI 冻结至上限）。代际变化即立即返回（反馈层对
    // 空结果优雅跳过）。
    const qint64 genAtEntry = m_generation;
    while (!deadline.hasExpired()) {
        if (m_generation != genAtEntry) return;
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
    // 清表代际 + 唤醒：在途 waitForCompletion 凭代际变化立即返回，
    // idle 执行器经 wake 后重扫（表已空，继续等待）。
    ++m_generation;
    m_condition.wakeAll();
}
