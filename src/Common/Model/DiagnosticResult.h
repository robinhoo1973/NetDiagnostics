// =============================================================================
// DiagnosticResult.h — Immutable result of a single diagnostic test (contract)
// =============================================================================
#pragma once

#include <QString>
#include <QDateTime>
#include <QVector>
#include <QVariantMap>
#include "Common/Model/DiagId.h"
#include "Common/Model/ResultProperty.h"

struct DiagnosticResult {
    DiagId      id;
    QString     displayName;
    DiagGroup   group;
    // 5WHY (复核 2026-08-18): 无默认值——漏赋值的生产者留下未定义值，
    // completed 计数了而徽标无处落账。默认 Error：遗漏 = 可见且可计数。
    DiagStatus  status = DiagStatus::Error;
    QString     summary;
    QString     details;
    qint64      durationMs = 0;
    QDateTime   timestamp;
    QVector<ResultProperty> properties;
    QString     rawOutput;
    QString     errorOutput;

    // Structured data for detail-page visualizations (contract-driven).
    QVariantMap data;

    // ── Convenience ──────────────────────────────────────────────────────
    bool isPass()     const { return status == DiagStatus::Pass; }
    bool isFail()     const { return status == DiagStatus::Fail; }
    bool isWarning()  const { return status == DiagStatus::Warning; }
    bool isSkipped()  const { return status == DiagStatus::Skipped; }
    bool isError()    const { return status == DiagStatus::Error; }
    bool isInfo()     const { return status == DiagStatus::Info; }
    bool isCancelled() const { return status == DiagStatus::Cancelled; } // NEW-17
    bool wasExecuted() const { return status != DiagStatus::Skipped && status != DiagStatus::Cancelled; }
    QString statusIcon() const { return diagStatusIcon(status); }

    // ── Factory helpers ──────────────────────────────────────────────────
    static DiagnosticResult skipped(DiagId id, const QString& reason);
    static DiagnosticResult error(DiagId id, const QString& msg);
    static DiagnosticResult timeout(DiagId id, qint64 durationMs);
    static DiagnosticResult cancelled(DiagId id, const QString& reason); // NEW-17
};
