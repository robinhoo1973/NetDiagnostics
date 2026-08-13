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
    DiagId    diagId()    const { return m_id; }
    QString   displayName() const { return diagnosticMeta(m_id).displayName; }
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

signals:
    void finished(const DiagnosticResult& result);
    void progressChanged(int percent, const QString& stage);

private:
    void onFutureFinished();
    void onWatchdogTimeout();
    unsigned registeredPlatforms() const;

    DiagId     m_id;
    QString    m_target;
    QThreadPool* m_pool;
    std::function<DiagnosticResult(DiagId, const QString&, RunContext&)> m_impl;
    int        m_timeoutMs;
    std::atomic<bool> m_cancelled{false};
    std::atomic<bool> m_finishedEmitted{false};
    bool       m_started = false;
    QFutureWatcher<DiagnosticResult>* m_watcher = nullptr;
    QTimer*    m_watchdog = nullptr;
};
