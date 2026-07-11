// =============================================================================
// DiagnosticTask.h — Base class for diagnostic tests with per-task timeout
//
// Each task manages its own QTimer watchdog and QFutureWatcher completion.
// The finished() signal fires on the main thread for both success and timeout.
// =============================================================================
#pragma once

#include <QObject>
#include <QTimer>
#include <atomic>

template<typename T> class QFutureWatcher;
#include "Common/Model/DiagId.h"
#include "Common/Model/DiagnosticResult.h"

class DiagnosticTask : public QObject {
    Q_OBJECT
public:
    DiagnosticTask(DiagId id, const QString& target = {},
                   int timeoutMs = 60000, QObject* parent = nullptr);
    ~DiagnosticTask() override;

    DiagId    diagId()   const { return m_id; }
    DiagGroup group()    const { return diagGroup(m_id); }
    QString   target()   const { return m_target; }
    int       timeoutMs() const { return m_timeoutMs; }

    void start();   // launch worker thread + arm watchdog
    void cancel();  // set cancellation flag, stop timer

signals:
    void finished(const DiagnosticResult& result);

protected:
    virtual DiagnosticResult run() = 0;
    bool isCancelled() const { return m_cancelled.load(std::memory_order_acquire); }

private:
    void onFutureFinished();
    void onWatchdogTimeout();

    DiagId     m_id;
    QString    m_target;
    int        m_timeoutMs;
    std::atomic<bool> m_cancelled{false};
    std::atomic<bool> m_finishedEmitted{false};
    QFutureWatcher<DiagnosticResult>* m_watcher = nullptr;
    QTimer*    m_watchdog = nullptr;
};
