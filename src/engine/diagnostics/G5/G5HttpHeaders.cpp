#include "engine/diagnostics/G5/G5Common.h"
DiagnosticResult httpHeaders(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5HttpHeaders, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "http" && u.scheme() != "https")
        return g5Result(DiagId::G5HttpHeaders, "Not HTTP(S)", DiagStatus::Skipped);
    auto cr = curlHttp(u, 15000, false);
    if (!cr.ok) return g5Result(DiagId::G5HttpHeaders, cr.error, DiagStatus::Fail);
    auto r = g5Result(DiagId::G5HttpHeaders,
        QStringLiteral("HTTP %1").arg(cr.statusCode),
        cr.statusCode >= 200 && cr.statusCode < 300 ? DiagStatus::Pass :
        cr.statusCode >= 300 && cr.statusCode < 400 ? DiagStatus::Warning :
        DiagStatus::Fail);
    r.rawOutput = cr.lines.join('\n');
    r.details = r.rawOutput;
    r.durationMs = cr.totalMs;
    r.properties.append({QStringLiteral("HTTP Status"), QString::number(cr.statusCode)});
    r.properties.append({QStringLiteral("Response Time"), QStringLiteral("%1 ms").arg(cr.totalMs, 0, 'f', 1)});
    // Count response headers from < -prefixed lines
    int headerCount = 0;
    for (const auto& line : cr.lines)
        if (line.startsWith('<') && line.contains(':')) ++headerCount;
    r.properties.append({QStringLiteral("Response Headers"), QString::number(headerCount)});
    return r;

// ── G5.5 Curl Verbose (full GET, curl -v style complete output) ──────────
}
} // namespace G5WebsiteUrl
