// =============================================================================
// DiagnosticTask.cpp — Per-task timeout watchdog via QTimer + QFutureWatcher
// =============================================================================
#include "Common/Services/DiagnosticTask.h"
#include <QFutureWatcher>
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

    // Lifetime guarantee: the task self-deletes ONLY in onFutureFinished(),
    // which runs after run() has returned. Therefore `this` is guaranteed to
    // outlive the worker, and capturing it here is safe. (A watchdog timeout
    // emits finished() early but must NOT delete the task while run() is
    // still executing on the pool thread — see onWatchdogTimeout.)
    //
    // CRITICAL: run() must never let a C++ exception escape. QtConcurrent stores
    // a thrown exception in the QFuture and re-throws it from QFuture::result()
    // on the MAIN thread (onFutureFinished), where it is uncaught → std::terminate
    // → hard crash. Any diagnostic that throws (std::bad_alloc, out-of-range,
    // std::system_error from sockets, etc.) would take down the whole app. Catch
    // everything here and turn it into a failed result.
    m_watcher->setFuture(QtConcurrent::run([this]() -> DiagnosticResult {
        try {
            return run();
        } catch (const std::exception& e) {
            return DiagnosticResult::error(m_id,
                QStringLiteral("Diagnostic crashed: %1").arg(QString::fromUtf8(e.what())));
        } catch (...) {
            return DiagnosticResult::error(m_id,
                QStringLiteral("Diagnostic crashed: unknown exception"));
        }
    }));
}

void DiagnosticTask::cancel() {
    m_cancelled.store(true, std::memory_order_release);
    if (m_watchdog) m_watchdog->stop();
    // The QFuture continues in the background until run() returns,
    // but the finished signal will be suppressed by the cancelled flag.
}

void DiagnosticTask::onFutureFinished() {
    if (m_watchdog) m_watchdog->stop();
    // Deliver the real result only if the watchdog hasn't already reported
    // a timeout for this task.
    if (!m_finishedEmitted.exchange(true) &&
        !m_cancelled.load(std::memory_order_acquire)) {
        // QFuture::result() re-throws any exception stored during the run.
        // The worker lambda already catches everything, but guard here too so
        // a stored exception can never terminate the app on the main thread.
        try {
            emit finished(m_watcher->result());
        } catch (const std::exception& e) {
            emit finished(DiagnosticResult::error(m_id,
                QStringLiteral("Diagnostic crashed: %1").arg(QString::fromUtf8(e.what()))));
        } catch (...) {
            emit finished(DiagnosticResult::error(m_id,
                QStringLiteral("Diagnostic crashed: unknown exception")));
        }
    }
    // run() has returned; the object is now safe to destroy.
    deleteLater();
}

void DiagnosticTask::onWatchdogTimeout() {
    m_cancelled.store(true, std::memory_order_release);
    // Report the timeout, but DO NOT delete the task here: the worker thread
    // may still be executing run() using this object. Self-deletion is
    // deferred to onFutureFinished(), which fires once run() returns.
    if (!m_finishedEmitted.exchange(true)) {
        emit finished(DiagnosticResult::timeout(m_id, m_timeoutMs));
    }
}
