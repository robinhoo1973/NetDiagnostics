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

// 5WHY (2026-09-26 五份复制收敛): makeResult 曾在 G1-G5 各写一份且已漂移——
// 仅 G4/G5 对 Fail/Warning/Error 回填 errorOutput，G1/G2/G3 的失败结果错误
// 区恒空（PageErrorSection 门控于 errorOutput，meta 却声明 showErrorOutput
// true）。单一工厂统一契约：终端错误区块契约对五组一致。
DiagnosticResult DiagnosticResult::makeResult(DiagId id, DiagStatus status,
                                              const QString& summary,
                                              const QVector<ResultProperty>& props,
                                              const QString& details) {
    DiagnosticResult r;
    r.id = id;
    r.displayName = diagDisplayName(id);
    r.group = diagGroup(id);
    r.status = status;
    r.summary = summary;
    r.properties = props;
    r.details = details;
    r.rawOutput = details;
    if ((status == DiagStatus::Fail || status == DiagStatus::Warning
         || status == DiagStatus::Error) && r.errorOutput.isEmpty())
        r.errorOutput = summary;
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
