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
    : QThread(parent), m_db(db),
      m_stopFlag(std::make_shared<std::atomic<bool>>(false)) {}

ProbeExecutor::~ProbeExecutor() { requestStop(); }

bool ProbeExecutor::requestStop() {
    // 5WHY: Do NOT call terminate() here. terminate() forcibly kills the
    // thread without unwinding the stack or releasing any locks (e.g.,
    // ProbeDatabase::m_mutex). If the worker thread is mid-write inside
    // m_db->writeResults(), the mutex stays locked forever, causing a
    // permanent deadlock on any subsequent ProbeDatabase access — including
    // the next diagnostic run or even app shutdown. Instead, we extend the
    // graceful wait to 15 s and report failure when the thread still hasn't
    // cooperated so the OWNER (GeoProbe) can avoid freeing the database the
    // live thread is still writing into (5WHY review 2026-08-17: the old
    // version leaked the thread but then freed m_db anyway — use-after-free
    // at app exit).
    // M7: 设置共享停止标志——worker 线程通过 shared_ptr 访问，
    // 不依赖 executor 生命周期。
    m_stopFlag->store(true);
    // 5WHY (2026-09-05 复核 停机唤醒): 执行器可能正停在 waitForNewWork 的
    // 不限时等待上——只置标志不唤醒，停机感知延迟到下一个 timeoutMs 超时
    // （原 500ms 轮询是唯一停机路径）。置标志后立即唤醒条件变量。
    if (m_db) m_db->wake();
    if (isRunning()) {
        if (!wait(15000)) {
            qWarning("ProbeExecutor: thread did not stop within 15s, leaking");
            return false;
        }
    }
    return true;
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

    // M7 (5WHY): worker lambda 不再捕获 this——共享停止标志 + 数据库指针
    // 通过值捕获，executor 析构后 worker 线程仍可安全检查停止标志。
    auto stopFlag = m_stopFlag;
    auto* db = m_db;

    while (!stopFlag->load()) {
        // 5WHY: fetchWaiting(512) spawned up to 512 std::threads per batch,
        // each holding a socket for 3-8s — on mobile (low fd limits, small
        // stacks) this risks fd/thread exhaustion.  Cap the batch so no more
        // than 64 concurrent probes run; GeoProbe's ~138 servers then take
        // 2-3 bounded waves instead of one oversized burst.
        QVector<ProbeDatabase::Task> batch = db->fetchWaiting(64);
        if (batch.isEmpty()) {
            // 5WHY (2026-09-05 空闲轮询改事件等待): 曾 100ms 盲轮询——G3 探测
            // 完成后执行器线程在进程余下生命周期内每 10 次/秒空转（移动端
            // 电池常驻成本 + 新提交至多 100ms 延迟）。改条件变量等待：表内
            // 有 Waiting 或 stop 置位即返回。5WHY (2026-09-05 复核): 曾 500ms
            // 定时等待——空闲期每 500ms 全表重扫纯属空转；每个 Waiting 跃迁
            // 与 requestStop 的 wake() 都会唤醒，0 = 不限时等待。
            db->waitForNewWork(stopFlag, 0);
            continue;
        }

        // Pre-built QHash is read-only during the parallel loop — thread-safe.
        const auto& lookup = metaByHost;
        // All 64 threads block mostly on recv(), so context-switch overhead is
        // negligible compared to the 3-8s network wait per server.
        std::vector<std::thread> threads;
        threads.reserve(batch.size());

        for (int i = 0; i < batch.size(); i++) {
            if (stopFlag->load()) {
                // 5WHY (2026-09-05 停机路径任务卡死): 曾直接 break——fetchWaiting
                // 已把剩余任务翻为 Running，无人写回、upsert 无法复活 Running
                // 任务 → 键永久卡 Running，下一轮 waitForCompletion 等满 120s
                // 上限且数据缺空。停机时以空结果终局落账（forceDone），
                // 反馈层对空结果优雅跳过。
                for (int j = i; j < batch.size(); ++j) {
                    const auto itF = lookup.find(batch[j].key);
                    db->writeResults(batch[j].key, {},
                        (itF != lookup.end()) ? itF->country : QStringLiteral("XX"),
                        (itF != lookup.end()) ? itF->regionTags : QStringList(),
                        /*forceDone=*/true, batch[j].generation);
                }
                break;
            }
            // 5WHY (review 2026-08-17): pthread_create EAGAIN（低 RLIMIT_NPROC
            // /fd 耗尽）会以 std::system_error 逃逸 QThread::run() → 未捕获
            // 异常 → std::terminate 整进程中止。降级：写空结果完成任务流转。
            // 5WHY (review round 3): 仅"跳过并记录"会让任务卡在 Running——
            // fetchWaiting 已将其出队，反馈将等待至 120s 上限且静默缺数据。
            auto it = lookup.find(batch[i].key);
            const QString country = (it != lookup.end()) ? it->country : "XX";
            const QStringList regionTags = (it != lookup.end()) ? it->regionTags : QStringList();
            try {
                threads.emplace_back([stopFlag, task = &batch[i], country, regionTags, db]() {
                int colon = task->key.lastIndexOf(':');
                QString host = task->key.left(colon);
                int port = task->key.mid(colon + 1).toInt();
                if (port <= 0) port = 80;

                // 5WHY (2026-09-05 rounds 续测): 曾按快照的 task->rounds 满轮
                // 测量——重入队的任务已积累部分结果，再测满轮会超出请求回合。
                // 按缺额续测（快照含已积累 results），与 writeResults 的重入
                // 队校验配对收敛。
                const int need = qMax(0, task->rounds - task->results.size());
                QVector<double> results;
                results.reserve(need);
                for (int r = 0; r < need; r++) {
                    // 5WHY: an in-flight socket call cannot be aborted
                    // safely mid-flight, but on stop we must not start any
                    // further rounds.  Without this check, a stop during a
                    // 64-thread batch could keep the executor running up to
                    // ~99s of probes after shutdown began — exceeding
                    // requestStop()'s 15s wait and leaking the thread (with
                    // a live m_db reference).  One round is ~8s worst case.
                    if (stopFlag->load(std::memory_order_acquire)) break;
                    double ttfb = SystemDiagnostics::httpTtfb(host, port, "/", 3000, 8);
                    if (ttfb >= 0) results.append(ttfb);
                }

                // 5WHY (2026-09-05 复核 停机路径重入队孤儿): 停机时以部分
                // 结果写回——writeResults 的回合数校验会把它重入 Waiting，
                // 而执行器随即退出、无人消费（后续 waitForCompletion 等满
                // 120s 上限）。停机置位时以终局落账（forceDone），与批次
                // 尾部循环同门。
                db->writeResults(task->key, results, country, regionTags,
                                 /*forceDone=*/stopFlag->load(std::memory_order_acquire),
                                 task->generation);
            });
            } catch (const std::system_error& e) {
                qWarning("ProbeExecutor: thread creation failed (%s) — writing empty result for %s",
                         e.what(), qPrintable(batch[i].key));
                db->writeResults(batch[i].key, {}, country, regionTags,
                                 /*forceDone=*/true, batch[i].generation);
            }
        }
        for (auto& t : threads) t.join();
    }
}
