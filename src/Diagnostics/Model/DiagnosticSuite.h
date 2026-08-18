// =============================================================================
// DiagnosticSuite.h — Group scheduling unit (§7, mirrors PageDisplay)
//
// A4 boundary: AppState remains the orchestration owner (startNextGroup/cancel/
// runGeneration/cellularWarn).  Suite supplies structure + concurrency + stats.
// NEW-15: suite is created per run by AppState and holds the probes; probes are
// deleteLater'd after all finished on the main thread.
// =============================================================================
#pragma once

#include "Common/Model/DiagId.h"
#include "Common/Model/DiagnosticResult.h"
#include <QObject>
#include <QVector>
#include <QList>
#include <QHash>

class QThreadPool;
class QTimer;
class DiagnosticBase;

class DiagnosticSuite : public QObject {
    Q_OBJECT
public:
    struct Stats {
        int total = 0, completed = 0, pass = 0, warn = 0, fail = 0,
            skip = 0, info = 0, error = 0, cancelled = 0;   // NEW-17
    };

    explicit DiagnosticSuite(DiagGroup group, QObject* parent = nullptr);
    ~DiagnosticSuite() override;

    DiagGroup group() const { return m_group; }
    QVector<DiagId> diagIds() const { return m_ids; }
    void setDiagIds(const QVector<DiagId>& ids);
    int enabledCount() const { return m_ids.size(); }

    qint64 deadlineSec() const { return m_deadlineSec; }
    void setDeadlineSec(qint64 sec) { m_deadlineSec = sec; }

    bool isRunning() const { return m_running; }
    const Stats& stats() const { return m_stats; }

    // 5WHY (复核 2026-08-18 计时连续性): 暴露运行中探针的墙钟起点——UI 委托
    // 重建后从模型恢复真实计时，而非委托诞生时刻重新起算。
    QHash<DiagId, qint64> runningStartTimes() const;

    // Run the suite.  Non-runnable (platform/scheme/device) tests are reported
    // as Skipped so progress totals are stable.  (A4: caller = AppState.)
    void run(const QString& target, const QString& schemeLower = {});
    void cancel();

signals:
    void statsChanged();                                   // DIAG-10
    void progressChanged(int percent, const QString& stage);
    void resultReady(const DiagnosticResult& result);      // per-test completion
    void suiteFinished();

private slots:
    void onProbeFinished(const DiagnosticResult& r);
    void onProbeProgress(int pct, const QString& stage);
    void onDeadline();

private:
    void finishRun();
    void emitProgress();

    DiagGroup      m_group;
    QVector<DiagId> m_ids;
    QThreadPool*   m_pool = nullptr;
    QList<DiagnosticBase*> m_probes;
    Stats          m_stats;
    bool           m_running = false;
    int            m_completedCount = 0;
    QTimer*        m_deadlineTimer = nullptr;
    qint64         m_deadlineSec = 600;
    QString        m_target;
    QString        m_scheme;
};
