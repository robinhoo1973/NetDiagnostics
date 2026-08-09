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

class ProbeDatabase;

class ProbeExecutor : public QThread {
    Q_OBJECT
public:
    explicit ProbeExecutor(ProbeDatabase* db, QObject* parent = nullptr);
    ~ProbeExecutor();

    void run() override;
    void requestStop();            // called during shutdown

private:
    ProbeDatabase* m_db;
    std::atomic<bool> m_stopRequested{false};
};
