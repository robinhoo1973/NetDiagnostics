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
struct RunSnapshot;
class QTimer;
class DiagnosticBase;

class DiagnosticSuite : public QObject {
    Q_OBJECT
public:

    explicit DiagnosticSuite(DiagGroup group, QObject* parent = nullptr);
    ~DiagnosticSuite() override;

    DiagGroup group() const { return m_group; }
    QVector<DiagId> diagIds() const { return m_ids; }
    void setDiagIds(const QVector<DiagId>& ids);
    int enabledCount() const { return m_ids.size(); }

    qint64 deadlineSec() const { return m_deadlineSec; }
    void setDeadlineSec(qint64 sec) { m_deadlineSec = sec; }

    bool isRunning() const { return m_running; }
    // 5WHY (simplify 2026-09-05): Stats 结构与 stats()/statsChanged 已删除——
    // 零消费方的写-only 机器（8 计数器 + mirror 字段每结果维护）。唯一被
    // 消费的是 completed 进度（经 progressChanged），保留 m_completedCount
    // 与 m_total 两个整型。

    // 5WHY (复核 2026-08-18 计时连续性): 暴露运行中探针的单调起点——UI
    // 委托重建后从模型恢复真实计时，而非委托诞生时刻重新起算。
    // 5WHY (复核 2026-08-20 墙钟死链): 曾墙钟/单调双表并存——UI 改读单调
    // 后墙钟表零消费方仍每次重建双份扫描（~45 项 × 每次结果落地）。
    // 删墙钟表，仅留单调（同 MonotonicClock 基准）。
    QHash<DiagId, qint64> runningStartTimesMono() const;

    // Run the suite.  Non-runnable (platform/scheme/device) tests are reported
    // as Skipped so progress totals are stable.  (A4: caller = AppState.)
    void run(const QString& target, const QString& schemeLower = {});
    void cancel();

signals:
    // 5WHY (simplify 2026-09-05): statsChanged 删除——零消费方（见类头注释）。
    void progressChanged(int percent, const QString& stage);
    void resultReady(const DiagnosticResult& result);      // per-test completion
    void suiteFinished();

private slots:
    void onProbeFinished(const DiagnosticResult& r);
    void onDeadline();

private:
    void finishRun();
    void emitProgress();

    DiagGroup      m_group;
    QVector<DiagId> m_ids;
    QThreadPool*   m_pool = nullptr;
    QList<DiagnosticBase*> m_probes;
    bool           m_running = false;
    int            m_completedCount = 0;
    int            m_total = 0;   // 5WHY (simplify 2026-09-05): 仅 emitProgress 消费
    QTimer*        m_deadlineTimer = nullptr;
    qint64         m_deadlineSec = 600;
    // 每轮运行共享的系统快照（nmcli 等外部命令一轮只 spawn 一次）
    std::shared_ptr<RunSnapshot> m_snapshot;
};
