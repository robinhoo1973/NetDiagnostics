// =============================================================================
// DiagnosticResult.h — Immutable result of a single diagnostic test
// =============================================================================
#pragma once

#include <QString>
#include <QDateTime>
#include <QVector>
#include "DiagId.h"
#include "ResultProperty.h"

struct DiagnosticResult {
    DiagId      id;
    QString     displayName;
    DiagGroup   group;
    DiagStatus  status;
    QString     summary;
    QString     details;
    qint64      durationMs = 0;
    QDateTime   timestamp;
    QVector<ResultProperty> properties;
    QString     rawOutput;
    QString     errorOutput;

    // ── Convenience ──────────────────────────────────────────────────────────
    bool isPass()    const { return status == DiagStatus::Pass; }
    bool isFail()    const { return status == DiagStatus::Fail; }
    bool isWarning() const { return status == DiagStatus::Warning; }
    bool isSkipped() const { return status == DiagStatus::Skipped; }
    bool isError()   const { return status == DiagStatus::Error; }
    bool isInfo()    const { return status == DiagStatus::Info; }
    // 5WHY: isDone() returned false for Skipped — a skipped test IS "done"
    // (no further work will occur). The name was misleading. Replaced with
    // wasExecuted() which correctly expresses the semantic: "did this test
    // actually run?" vs "is this test finished?"
    [[deprecated("Use wasExecuted() instead — isDone() returns false for Skipped which is misleading")]]
    bool isDone()    const { return status != DiagStatus::Skipped; }
    bool wasExecuted() const { return status != DiagStatus::Skipped; }
    QString statusIcon() const { return diagStatusIcon(status); }

    // ── Factory helpers ──────────────────────────────────────────────────────
    static DiagnosticResult skipped(DiagId id, const QString& reason);
    static DiagnosticResult error(DiagId id, const QString& msg);
    // 5WHY: group was a redundant parameter — diagGroup(id) is the single source of truth.
    // API now consistent with skipped() and error().
    static DiagnosticResult timeout(DiagId id, qint64 durationMs);
};
