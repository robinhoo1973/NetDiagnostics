// =============================================================================
// DiagnosticBase.cpp
// =============================================================================
#include "Common/Services/DiagnosticBase.h"
#include "Common/Platform/DeviceCapability.h"

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QMetaObject>

DiagnosticBase::DiagnosticBase(
    DiagId id, const QString& target, QThreadPool* pool,
    std::function<DiagnosticResult(DiagId, const QString&, RunContext&)> impl,
    int timeoutMs, QObject* parent)
    : QObject(parent), m_id(id), m_target(target), m_pool(pool),
      m_impl(std::move(impl)), m_timeoutMs(timeoutMs > 0 ? timeoutMs : 60000) {}

DiagnosticBase::~DiagnosticBase() {
    delete m_watcher;
    delete m_watchdog;
}

bool DiagnosticBase::runnable(DiagId id, const QString& schemeLower) {
    if (AdapterRegistry::select(id, schemeLower) == nullptr) return false;
    return DeviceCapability::diagSupportedOnDevice(id);
}

unsigned DiagnosticBase::registeredPlatforms() const {
    return AdapterRegistry::registeredPlatforms(m_id);
}

void DiagnosticBase::start() {
    if (m_started) return;
    m_started = true;

    m_watchdog = new QTimer(this);
    m_watchdog->setSingleShot(true);
    m_watchdog->setInterval(m_timeoutMs);
    connect(m_watchdog, &QTimer::timeout, this, &DiagnosticBase::onWatchdogTimeout);

    m_watcher = new QFutureWatcher<DiagnosticResult>(this);
    connect(m_watcher, &QFutureWatcher<DiagnosticResult>::finished,
            this, &DiagnosticBase::onFutureFinished);

    // NEW-5: progress() only queues to the main thread; the worker never
    // touches QObject signal machinery directly.
    const auto progressFn = [this](int pct, const QString& stage) {
        QMetaObject::invokeMethod(this, [this, pct, stage]() {
            emit progressChanged(pct, stage);
        }, Qt::QueuedConnection);
    };

    const auto work = [this, progressFn]() -> DiagnosticResult {
        if (m_cancelled.load(std::memory_order_acquire))
            return DiagnosticResult::cancelled(m_id, QStringLiteral("Cancelled before start"));
        RunContext ctx{m_cancelled, progressFn};
        return m_impl(m_id, m_target, ctx);
    };

    m_watcher->setFuture(QtConcurrent::run(m_pool ? m_pool : QThreadPool::globalInstance(), work));
    m_watchdog->start();
}

void DiagnosticBase::cancel() {
    m_cancelled.store(true, std::memory_order_release);
    if (m_watchdog) m_watchdog->stop();
}

void DiagnosticBase::onFutureFinished() {
    m_watchdog->stop();
    if (m_finishedEmitted.exchange(true)) return;

    DiagnosticResult result = m_watcher->result();
    if (m_cancelled.load(std::memory_order_acquire)
        && result.status != DiagStatus::Cancelled && !result.wasExecuted()) {
        result = DiagnosticResult::cancelled(m_id, QStringLiteral("Cancelled"));
    }
    emit finished(result);
    deleteLater();   // owned by Suite; safe to destroy after finished (NEW-15)
}

void DiagnosticBase::onWatchdogTimeout() {
    m_cancelled.store(true, std::memory_order_release);
    if (m_finishedEmitted.exchange(true)) return;
    emit finished(DiagnosticResult::timeout(m_id, m_timeoutMs));
    deleteLater();
}
