// =============================================================================
// DiagnosticResult.cpp — Factory helpers
// =============================================================================
#include "Common/Model/DiagnosticResult.h"
#include "Common/Model/DiagNames.h"

DiagnosticResult DiagnosticResult::skipped(DiagId id, const QString& reason) {
    DiagnosticResult r;
    r.id = id;
    r.displayName = diagDisplayName(id);
    r.group = diagGroup(id);
    r.status = DiagStatus::Skipped;
    r.summary = reason;
    r.timestamp = QDateTime::currentDateTime();
    return r;
}

DiagnosticResult DiagnosticResult::error(DiagId id, const QString& msg) {
    DiagnosticResult r;
    r.id = id;
    r.displayName = diagDisplayName(id);
    r.group = diagGroup(id);
    r.status = DiagStatus::Error;
    r.summary = msg;
    r.errorOutput = msg;
    r.timestamp = QDateTime::currentDateTime();
    return r;
}

DiagnosticResult DiagnosticResult::timeout(DiagId id, qint64 durationMs) {
    DiagnosticResult r;
    r.id = id;
    r.displayName = diagDisplayName(id);
    r.group = diagGroup(id);
    r.status = DiagStatus::Error;
    r.summary = QStringLiteral("Timed out after %1s").arg(durationMs / 1000);
    r.errorOutput = r.summary;
    r.durationMs = durationMs;
    r.timestamp = QDateTime::currentDateTime();
    return r;
}

DiagnosticResult DiagnosticResult::cancelled(DiagId id, const QString& reason) {
    DiagnosticResult r;
    r.id = id;
    r.displayName = diagDisplayName(id);
    r.group = diagGroup(id);
    r.status = DiagStatus::Cancelled; // NEW-17: deadline/cancel 中止项
    r.summary = reason;
    r.timestamp = QDateTime::currentDateTime();
    return r;
}
