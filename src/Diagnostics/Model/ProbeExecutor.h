// =============================================================================
// ProbeExecutor.h — Persistent probe worker thread
//
// Polls ProbeDatabase for Waiting tasks, executes TTFB measurements
// via 10-thread parallel pool, writes raw results back to Database.
// All hostname resolution and metadata lookup happens here (high cohesion).
// =============================================================================
#pragma once

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>

class ProbeDatabase;

class ProbeExecutor : public QThread {
    Q_OBJECT
public:
    explicit ProbeExecutor(ProbeDatabase* db, QObject* parent = nullptr);
    ~ProbeExecutor();

    void run() override;
    void notify();               // wake the worker thread
    void shutdown();

private:
    ProbeDatabase* m_db;
    QMutex m_mutex;
    QWaitCondition m_condition;
    std::atomic<bool> m_shutdown{false};
};
