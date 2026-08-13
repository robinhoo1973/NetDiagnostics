// =============================================================================
// DiagnosticSuite.cpp — Group scheduling: build probes from AdapterRegistry,
// run on the suite QThreadPool, aggregate stats/progress, enforce deadline.
// =============================================================================
#include "Diagnostics/Model/DiagnosticSuite.h"
#include "Common/Services/DiagnosticBase.h"
#include "Common/Services/PlatformAdapter.h"
#include "Common/Platform/DeviceCapability.h"

#include <QThreadPool>
#include <QThread>
#include <QTimer>

#include <algorithm>

DiagnosticSuite::DiagnosticSuite(DiagGroup group, QObject* parent)
    : QObject(parent), m_group(group) {
    m_pool = new QThreadPool(this);
    m_pool->setMaxThreadCount(std::max(1, std::min(8, QThread::idealThreadCount()))); // DIAG-3
    m_deadlineTimer = new QTimer(this);
    m_deadlineTimer->setSingleShot(true);
    connect(m_deadlineTimer, &QTimer::timeout, this, &DiagnosticSuite::onDeadline);
}

DiagnosticSuite::~DiagnosticSuite() = default;

void DiagnosticSuite::setDiagIds(const QVector<DiagId>& ids) { m_ids = ids; }

void DiagnosticSuite::run(const QString& target, const QString& schemeLower) {
    if (m_running) return;
    m_running = true;
    m_target = target;
    m_scheme = schemeLower;
    m_completedCount = 0;
    m_stats = Stats{};
    m_stats.total = m_ids.size();
    m_probes.clear();

    DeviceCapability::invalidateCache();   // NEW-4: refresh device probes pre-run

    for (DiagId id : m_ids) {
        const PlatformAdapter* adapter = AdapterRegistry::select(id, schemeLower);
        if (!adapter || !DeviceCapability::diagSupportedOnDevice(id)) {
            ++m_completedCount;   // Skipped by capability -> auto-complete (stable totals)
            ++m_stats.skip;       // R1-5: 每状态计数随结果维护
            m_stats.completed = m_completedCount;
            emit resultReady(DiagnosticResult::skipped(
                id, QStringLiteral("Not runnable on this device/target")));
            continue;
        }
        auto* probe = new DiagnosticBase(
            id, target, m_pool, adapter->run,
            static_cast<int>(diagnosticMeta(id).durationProfileMs), this);
        connect(probe, &DiagnosticBase::finished, this, &DiagnosticSuite::onProbeFinished);
        connect(probe, &DiagnosticBase::progressChanged, this, &DiagnosticSuite::onProbeProgress);
        m_probes.append(probe);
    }

    for (auto* p : m_probes) p->start();
    m_deadlineTimer->start(static_cast<int>(m_deadlineSec * 1000));

    emitProgress();
    if (m_probes.isEmpty()) finishRun();   // everything auto-skipped
}

void DiagnosticSuite::cancel() {
    if (!m_running) return;
    m_deadlineTimer->stop();
    for (auto* p : m_probes) p->cancel();
}

void DiagnosticSuite::onProbeFinished(const DiagnosticResult& r) {
    if (auto* p = qobject_cast<DiagnosticBase*>(sender())) m_probes.removeOne(p);
    ++m_completedCount;
    m_stats.completed = m_completedCount;   // R3-1
    switch (r.status) {   // R1-5: per-status counters feed statusChanged observers
        case DiagStatus::Pass:      ++m_stats.pass; break;
        case DiagStatus::Warning:   ++m_stats.warn; break;
        case DiagStatus::Fail:      ++m_stats.fail; break;
        case DiagStatus::Skipped:   ++m_stats.skip; break;
        case DiagStatus::Error:     ++m_stats.error; break;
        case DiagStatus::Info:      ++m_stats.info; break;
        case DiagStatus::Cancelled: ++m_stats.cancelled; break;
    }
    emit resultReady(r);
    emit statsChanged();
    emitProgress();
    if (m_completedCount >= m_stats.total) finishRun();
}

void DiagnosticSuite::onProbeProgress(int /*pct*/, const QString& /*stage*/) {
    // Per-probe fine-grained progress; suite-level aggregate is emitted by
    // emitProgress() on completion (A5: completed/total x100).
}

void DiagnosticSuite::onDeadline() {
    if (!m_running) return;
    m_deadlineTimer->stop();
    // NEW-17: deadline-aborted items are Cancelled (not Skipped/Fail).
    const auto probes = m_probes;
    m_probes.clear();
    for (auto* p : probes) {
        // R5-2: 先断开 finished——worker 可能仍在运行，稍后 onFutureFinished
        // 仍会 emit finished → onProbeFinished 再次计数/发结果（同一探针双计）。
        QObject::disconnect(p, &DiagnosticBase::finished,
                            this, &DiagnosticSuite::onProbeFinished);
        p->cancel();
        ++m_completedCount;
        m_stats.completed = m_completedCount;   // R3-1
        ++m_stats.cancelled;   // R1-5
        emit resultReady(DiagnosticResult::cancelled(
            p->diagId(), QStringLiteral("Suite deadline exceeded (%1s)").arg(m_deadlineSec)));
        // 5WHY（UAF，C3）：禁止 deleteLater——探针 worker 可能仍在池线程运行。
        // 探针在 onFutureFinished（future 结束后）自行 deleteLater；若 worker
        // 永不返回，Suite 析构时 QThreadPool 析构会等待其完成。
    }
    finishRun();
}

void DiagnosticSuite::finishRun() {
    m_running = false;
    m_deadlineTimer->stop();
    emit statsChanged();
    emitProgress();
    emit suiteFinished();
}

void DiagnosticSuite::emitProgress() {
    // A5: completed/total x100; 0/0 -> 0% (NEW-17).
    const int total = m_stats.total > 0 ? m_stats.total : 1;
    emit progressChanged((m_completedCount * 100) / total, QString());
}
