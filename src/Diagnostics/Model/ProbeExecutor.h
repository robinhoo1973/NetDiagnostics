// =============================================================================
// ProbeExecutor.h — On-demand probe worker thread
//
// Started by Scheduler when Waiting tasks exist, auto-stops when
// no more Waiting tasks remain (fetchWaiting returns empty).
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
