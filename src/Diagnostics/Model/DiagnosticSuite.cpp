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
    m_completedCount = 0;
    m_total = m_ids.size();
    m_probes.clear();
    // 5WHY (复核 2026-08-20 nmcli 双 spawn): 每轮新建共享快照——池线程并行
    // 的 probeDhcp/probeIpConfig 经 RunContext.snapshot 复用同一次
    // `nmcli device show` 输出（互斥惰性填充，一轮只 spawn 一次）。
    m_snapshot = std::make_shared<RunSnapshot>();

    DeviceCapability::invalidateCache();   // NEW-4: refresh device probes pre-run

    for (DiagId id : m_ids) {
        const PlatformAdapter* adapter = AdapterRegistry::select(id, schemeLower);
        if (!adapter || !DeviceCapability::diagSupportedOnDevice(id)) {
            ++m_completedCount;   // Skipped by capability -> auto-complete (stable totals)
            emit resultReady(DiagnosticResult::skipped(
                id, QStringLiteral("Not runnable on this device/target")));
            continue;
        }
        auto* probe = new DiagnosticBase(
            id, target, m_pool, adapter->run,
            static_cast<int>(diagnosticMeta(id).durationProfileMs), this, m_snapshot);
        connect(probe, &DiagnosticBase::finished, this, &DiagnosticSuite::onProbeFinished);
        m_probes.append(probe);
    }

    // 5WHY (review 2026-08-17): range-for 期间 onProbeFinished() 可同步触发
    // removeOne(p)（空闲池线程上瞬时完成的探针经 setFuture→finished 直接回调）
    // ——遍历中改 m_probes 是 QList 迭代器失效 UB。迭代本地拷贝。
    const auto probes = m_probes;
    for (auto* p : probes) p->start();
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
    emit resultReady(r);
    emitProgress();
    if (m_completedCount >= m_total) finishRun();
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
    emitProgress();
    emit suiteFinished();
}

void DiagnosticSuite::emitProgress() {
    // A5: completed/total x100; 0/0 -> 0% (NEW-17).
    const int total = m_total > 0 ? m_total : 1;
    emit progressChanged((m_completedCount * 100) / total, QString());
}

QHash<DiagId, qint64> DiagnosticSuite::runningStartTimesMono() const {
    QHash<DiagId, qint64> out;
    // 5WHY (复核 2026-08-21 哨兵显式化): 曾收录全部 m_probes——建表时
    // 尚未 start() 的探针 startedAtMonoMs 仍为 0，UI 的 hasStartedAt
    // （键存在即真）把未启动瓦片误判"有起点"（计时圆点显示应用启动
    // 时长，正是旧 0 值哨兵防的那类伪值）。仅收录已 start 的探针：
    // 键缺席 = 未注入，与显式标记语义对齐。
    for (const auto* p : m_probes)
        if (p->isStarted())
            out.insert(p->diagId(), p->startedAtMonoMs());
    return out;
}
