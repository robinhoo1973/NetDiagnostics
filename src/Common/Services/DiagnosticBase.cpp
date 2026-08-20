// =============================================================================
// DiagnosticBase.cpp
// =============================================================================
#include "Common/Services/DiagnosticBase.h"
#include "Common/Platform/DeviceCapability.h"
#include "Common/Services/MonotonicClock.h"

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QMetaObject>

DiagnosticBase::DiagnosticBase(
    DiagId id, const QString& target, QThreadPool* pool,
    std::function<DiagnosticResult(DiagId, const QString&, RunContext&)> impl,
    int timeoutMs, QObject* parent, std::shared_ptr<RunSnapshot> snapshot)
    : QObject(parent), m_id(id), m_pool(pool),
      m_timeoutMs(timeoutMs > 0 ? timeoutMs : 60000),
      m_state(std::make_shared<State>()) {
    m_state->id = id;
    m_state->target = target;
    m_state->impl = std::move(impl);
    m_state->snapshot = std::move(snapshot);
}

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

    // 空 impl 防护：worker 抛 bad_function_call 会经 future 重抛到主线程。
    if (!m_state->impl) {
        emit finished(DiagnosticResult::error(m_id,
            QStringLiteral("Probe implementation missing (no adapter run fn)")));
        deleteLater();
        return;
    }

    m_watchdog = new QTimer(this);
    m_watchdog->setSingleShot(true);
    m_watchdog->setInterval(m_timeoutMs);
    connect(m_watchdog, &QTimer::timeout, this, &DiagnosticBase::onWatchdogTimeout);

    m_watcher = new QFutureWatcher<DiagnosticResult>(this);
    connect(m_watcher, &QFutureWatcher<DiagnosticResult>::finished,
            this, &DiagnosticBase::onFutureFinished);

    // 8-16：探针时长计时（结果未自带时长时由 onFutureFinished 补齐）
    m_elapsed.start();
    // 单调起点（同 MonotonicClock 基准，UI 计时防墙钟步进）
    m_startedAtMonoMs = monotonicMsSinceAppStart();

    // NEW-5: progress() only queues to the main thread; the worker never
    // touches QObject signal machinery directly.
    // 生命周期不变量：`this` 的存活期 ⊇ future（worker）存活期——watchdog 超时
    // 与 Suite deadline 均不再 deleteLater，销毁只发生在 onFutureFinished。
    const auto state = m_state;
    const auto progressFn = [this](int pct, const QString& stage) {
        QMetaObject::invokeMethod(this, [this, pct, stage]() {
            emit progressChanged(pct, stage);
        }, Qt::QueuedConnection);
    };

    const auto work = [state, progressFn]() -> DiagnosticResult {
        if (state->cancelled.load(std::memory_order_acquire))
            return DiagnosticResult::cancelled(state->id, QStringLiteral("Cancelled before start"));
        RunContext ctx{state->cancelled, progressFn, state->snapshot};
        return state->impl(state->id, state->target, ctx);
    };

    m_watcher->setFuture(QtConcurrent::run(m_pool ? m_pool : QThreadPool::globalInstance(), work));
    m_watchdog->start();
}

void DiagnosticBase::cancel() {
    m_state->cancelled.store(true, std::memory_order_release);
    if (m_watchdog) m_watchdog->stop();
}

void DiagnosticBase::onFutureFinished() {
    m_watchdog->stop();
    // 5WHY（UAF）：对象销毁唯一合法点——future 已完成，worker 不再访问任何
    // 成员。watchdog 超时/取消路径只 emit，绝不 deleteLater。
    if (!m_state->finishedEmitted.exchange(true)) {
        DiagnosticResult result;
        try {
            result = m_watcher->result();
        } catch (const std::exception& e) {
            result = DiagnosticResult::error(m_id,
                QStringLiteral("Probe threw: %1").arg(QString::fromLatin1(e.what())));
        } catch (...) {
            result = DiagnosticResult::error(m_id,
                QStringLiteral("Probe threw unknown exception"));
        }
        if (m_state->cancelled.load(std::memory_order_acquire)
            && result.status != DiagStatus::Cancelled && !result.wasExecuted()) {
            result = DiagnosticResult::cancelled(m_id, QStringLiteral("Cancelled"));
        }
        // 8-16：集中补时长——诊断函数未填时以探针墙钟为准（≥1ms）
        if (result.durationMs <= 0 && m_elapsed.isValid())
            result.durationMs = qMax<qint64>(1, m_elapsed.elapsed());
        emit finished(result);
    }
    deleteLater();   // owned by Suite; safe to destroy after finished (NEW-15)
}

void DiagnosticBase::onWatchdogTimeout() {
    m_state->cancelled.store(true, std::memory_order_release);
    // 5WHY（UAF）：worker 可能仍在池线程运行——禁止 deleteLater。emit 超时结果
    // 后由 onFutureFinished 统一销毁（worker 结束后）。
    if (!m_state->finishedEmitted.exchange(true))
        emit finished(DiagnosticResult::timeout(m_id, m_timeoutMs));
}
