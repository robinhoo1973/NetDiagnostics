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
    r.details = msg;      // 5WHY: consumers read `details` for the detail overlay;
    r.errorOutput = msg;  // `errorOutput` is a secondary field. Both must be set
                          // so the detail overlay, report, and export all show the error.
    r.timestamp = QDateTime::currentDateTime();
    return r;
}

DiagnosticResult DiagnosticResult::timeout(DiagId id, qint64 durationMs) {
    DiagnosticResult r;
    r.id = id;
    // 5WHY: group was passed as a parameter (API inconsistency with skipped/error
    // which derive group from id). Now derived from id — single source of truth.
    r.group = diagGroup(id);
    r.status = DiagStatus::Error;
    r.summary = QStringLiteral("Timeout (%1s)").arg(durationMs / 1000);
    r.durationMs = durationMs;
    // 5WHY: timeout() did not set `details` or `errorOutput` — the detail overlay,
    // report HTML/PDF, and exports showed blank output for timed-out tests.
    // Now populated so the user sees *why* the test failed.
    r.details = QStringLiteral("The diagnostic timed out after %1 seconds.\n\n"
        "This usually means the target is unreachable, a firewall is blocking "
        "the connection, or the network is too slow to respond within the time limit.")
        .arg(durationMs / 1000);
    r.errorOutput = r.summary;
    r.timestamp = QDateTime::currentDateTime();
    return r;
}
