#include "engine/diagnostics/G5/G5Common.h"
DiagnosticResult curlVerbose(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5CurlVerbose, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "http" && u.scheme() != "https")
        return g5Result(DiagId::G5CurlVerbose, "Not HTTP(S)", DiagStatus::Skipped);
    auto cr = curlHttp(u, 60000, true); // GET with body
    if (!cr.ok) return g5Result(DiagId::G5CurlVerbose, cr.error, DiagStatus::Fail);
    auto r = g5Result(DiagId::G5CurlVerbose,
        QStringLiteral("HTTP %1, %2ms total").arg(cr.statusCode).arg(cr.totalMs),
        cr.statusCode >= 200 && cr.statusCode < 400 ? DiagStatus::Pass : DiagStatus::Fail);
    r.rawOutput = cr.lines.join('\n');
    r.details = r.rawOutput;
    r.durationMs = cr.totalMs;
    return r;

// ── G5.6 Security Headers (HEAD + check security header presence) ───────
}
} // namespace G5WebsiteUrl
