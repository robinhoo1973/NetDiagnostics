// =============================================================================
// DiagnosticBase.h — Execution core (§5, evolved from DiagnosticTask)
//
// Preserves the verified Task mechanics (QFutureWatcher + watchdog + cancel +
// timeout) and adds: RunContext (DIAG-3), progressChanged (DIAG-3/NEW-5),
// adapter-based runnable() (DIAG-1/NEW-1/NEW-4), Suite QThreadPool execution
// (A4).  Probes are implemented by PlatformAdapter::run — DiagnosticBase is
// the uniform runner.
// =============================================================================
#pragma once

#include <QObject>
#include <QTimer>
#include <QThreadPool>
#include <QElapsedTimer>
#include <atomic>
#include <functional>
#include <memory>

template<typename T> class QFutureWatcher;
#include "Common/Model/DiagId.h"
#include "Common/Model/DiagNames.h"
#include "Common/Model/DiagnosticMeta.h"
#include "Common/Model/DiagnosticResult.h"
#include "Common/Model/OutputContract.h"
#include "Common/Services/PlatformAdapter.h"

class DiagnosticBase : public QObject {
    Q_OBJECT
public:
    DiagnosticBase(DiagId id, const QString& target, QThreadPool* pool,
                   std::function<DiagnosticResult(DiagId, const QString&, RunContext&)> impl,
                   int timeoutMs, QObject* parent = nullptr);
    ~DiagnosticBase() override;

    // ── Identity / metadata ──────────────────────────────────────────────
    DiagId    diagId() const { return m_id; }
    QString   displayName() const { return ::diagDisplayName(m_id); }  // M1arch: DiagNames 单一来源
    QString   iconName()  const { return diagnosticMeta(m_id).iconName; }
    DiagGroup group()     const { return diagGroup(m_id); }
    DiagAnimType animType() const { return diagnosticMeta(m_id).animType; }
    const DiagnosticMeta& meta() const { return diagnosticMeta(m_id); }
    OutputContract contract() const { return contractFor(m_id); }  // A3 view
    bool hasPlatform(unsigned flag) const { return (registeredPlatforms() & flag) != 0; }

    // DIAG-1 + NEW-4: platform/scheme via registry, device via DeviceCapability.
    static bool runnable(DiagId id, const QString& schemeLower = {});

    // ── Execution ────────────────────────────────────────────────────────
    void start();          // run() on the Suite QThreadPool + arm watchdog
    void cancel();         // set cancellation flag, stop watchdog
    bool isCancelled() const { return m_state->cancelled.load(std::memory_order_acquire); }
    void setTimeoutMs(int ms) { m_timeoutMs = ms > 0 ? ms : 60000; }

signals:
    void finished(const DiagnosticResult& result);
    void progressChanged(int percent, const QString& stage);

private:
    void onFutureFinished();
    void onWatchdogTimeout();
    unsigned registeredPlatforms() const;

    // ── 5WHY（UAF，C2）：worker lambda 只能捕获 State 控制块（内含 cancelled/
    // finishedEmitted/id/target/impl），绝不捕获裸 this。对象销毁统一推迟到
    // onFutureFinished（future 已结束）之后的 deleteLater；watchdog 超时与
    // Suite deadline 只置 cancelled + emit，不 deleteLater。
    struct State {
        DiagId id;
        QString target;
        std::function<DiagnosticResult(DiagId, const QString&, RunContext&)> impl;
        std::atomic<bool> cancelled{false};
        std::atomic<bool> finishedEmitted{false};
    };
    std::shared_ptr<State> m_state;

    DiagId     m_id;
    QThreadPool* m_pool;
    int        m_timeoutMs;
    bool       m_started = false;
    // 8-16：探针级墙钟——集中补 durationMs（诊断函数大多不自填时长，
    // 导致 Dashboard 分层计时全 0 与详情页时长缺失）
    QElapsedTimer m_elapsed;
    QFutureWatcher<DiagnosticResult>* m_watcher = nullptr;
    QTimer*    m_watchdog = nullptr;
};
