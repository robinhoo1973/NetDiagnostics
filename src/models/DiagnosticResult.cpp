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
    // Populate details so the detail page explains *why* the test was skipped
    // (the popup shows `details`; leaving it empty would show blank output).
    r.details = QStringLiteral(
        "\nThis diagnostic was skipped \u2014 it is not supported on the current platform.\n\n"
        "Reason:\n  %1\n").arg(reason);
    r.rawOutput = r.details;
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
