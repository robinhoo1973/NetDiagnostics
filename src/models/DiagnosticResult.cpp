// =============================================================================
// DiagnosticResult.cpp — Factory helper implementations
// =============================================================================
#include "models/DiagnosticResult.h"

DiagnosticResult DiagnosticResult::skipped(DiagId id, const QString& reason) {
    DiagnosticResult r;
    r.id = id;
    r.group = diagGroup(id);
    r.status = DiagStatus::Skipped;
    r.summary = reason;
    r.timestamp = QDateTime::currentDateTime();
    return r;
}

DiagnosticResult DiagnosticResult::error(DiagId id, const QString& msg) {
    DiagnosticResult r;
    r.id = id;
    r.group = diagGroup(id);
    r.status = DiagStatus::Error;
    r.summary = msg;
    r.errorOutput = msg;
    r.timestamp = QDateTime::currentDateTime();
    return r;
}

DiagnosticResult DiagnosticResult::timeout(DiagId id, DiagGroup group, qint64 durationMs) {
    DiagnosticResult r;
    r.id = id;
    r.group = group;
    r.status = DiagStatus::Error;
    r.summary = QStringLiteral("Timeout (%1s)").arg(durationMs / 1000);
    r.durationMs = durationMs;
    r.timestamp = QDateTime::currentDateTime();
    return r;
}
