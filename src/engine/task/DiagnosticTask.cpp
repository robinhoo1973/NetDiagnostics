// =============================================================================
// DiagnosticTask.cpp — Per-task timeout watchdog via QTimer + QFutureWatcher
// =============================================================================
#include "engine/task/DiagnosticTask.h"
#include <QtConcurrent/QtConcurrent>

DiagnosticTask::DiagnosticTask(DiagId id, const QString& target,
                                 int timeoutMs, QObject* parent)
    : QObject(parent), m_id(id), m_target(target), m_timeoutMs(timeoutMs) {}

DiagnosticTask::~DiagnosticTask() {
    cancel();
}

void DiagnosticTask::start() {
    m_cancelled.store(false, std::memory_order_release);

    m_watchdog = new QTimer(this);
    m_watchdog->setSingleShot(true);
    connect(m_watchdog, &QTimer::timeout, this, &DiagnosticTask::onWatchdogTimeout);
    m_watchdog->start(m_timeoutMs);

    m_watcher = new QFutureWatcher<DiagnosticResult>(this);
    connect(m_watcher, &QFutureWatcher<DiagnosticResult>::finished,
            this, &DiagnosticTask::onFutureFinished);

    // Use QPointer to guard against deletion during concurrent execution.
    // If the watchdog fires first and deleteLater destroys the task,
    // the QPointer nulls out, so the worker safely returns a default result.
    QPointer<DiagnosticTask> guard(this);
    m_watcher->setFuture(QtConcurrent::run([guard]() -> DiagnosticResult {
        if (guard.isNull()) return {};
        return guard->run();
    }));
}

void DiagnosticTask::cancel() {
    m_cancelled.store(true, std::memory_order_release);
    if (m_watchdog) m_watchdog->stop();
    // The QFuture continues in the background until run() returns,
    // but the finished signal will be suppressed by the cancelled flag.
}

void DiagnosticTask::onFutureFinished() {
    if (!m_cancelled.load(std::memory_order_acquire)) {
        m_watchdog->stop();
        emit finished(m_watcher->result());
    }
}

void DiagnosticTask::onWatchdogTimeout() {
    m_cancelled.store(true, std::memory_order_release);
    emit finished(DiagnosticResult::timeout(m_id, group(), m_timeoutMs));
}
