// =============================================================================
// ProbeExecutor.h — On-demand probe worker thread
//
// Started by Scheduler when Waiting tasks exist.  Runs until requestStop()
// is called (app shutdown / database teardown).
//
// 5WHY (doc drift): the original comment claimed "auto-stops when no more
// Waiting tasks remain" but run() never implemented that — it polls
// fetchWaiting() and idles on msleep(100) when the queue is empty.  The
// cost of the idle poll is negligible (one fetchWaiting + sleep per 100ms),
// while real auto-stop would need the Scheduler to restart the executor on
// every new upsert (a start/stop race).  Documented behaviour now matches
// implementation: continuous run until explicit stop.
// =============================================================================
#pragma once

#include <QThread>
#include <atomic>
#include <memory>

class ProbeDatabase;

class ProbeExecutor : public QThread {
    Q_OBJECT
public:
    explicit ProbeExecutor(ProbeDatabase* db, QObject* parent = nullptr);
    ~ProbeExecutor();

    void run() override;
    // called during shutdown; returns false when the thread could not be
    // stopped within the grace period (caller must NOT free resources the
    // still-running thread may touch — see 5WHY in GeoProbe::~GeoProbe).
    bool requestStop();

private:
    ProbeDatabase* m_db;
    // M7 (5WHY): 原 worker lambda 捕获裸 this——executor 析构后线程仍在运行
    // 时，lambda 访问停止标志/m_db 导致 use-after-free。
    // 改用 shared_ptr 共享停止标志：executor 和 worker 线程各持一份引用，
    // 最后一个引用释放时自动清理。worker 线程不再依赖 executor 生命周期。
    // 5WHY (2026-09-04 修正复核): m_stopRequested 在 M7 后成为只写死状态
    // （worker 只读共享标志）——两份标志有漂移风险，已移除，唯一真相
    // 为 m_stopFlag。
    std::shared_ptr<std::atomic<bool>> m_stopFlag;
};
