#include "engine/diagnostics/G5/G5Common.h"
DiagnosticResult httpRedirect(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5HttpRedirect, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "http" && u.scheme() != "https")
        return g5Result(DiagId::G5HttpRedirect, "Not HTTP(S)", DiagStatus::Skipped);
    auto cr = curlHttp(u, 15000, true);
    if (!cr.ok) return g5Result(DiagId::G5HttpRedirect, cr.error, DiagStatus::Fail);
    auto r = g5Result(DiagId::G5HttpRedirect,
        cr.statusCode >= 300 && cr.statusCode < 400
            ? QStringLiteral("Redirect %1 → %2").arg(cr.statusCode).arg(cr.redirectLocation)
            : QStringLiteral("No redirect (HTTP %1)").arg(cr.statusCode),
        cr.statusCode >= 200 && cr.statusCode < 300 ? DiagStatus::Pass : DiagStatus::Warning);
    r.durationMs = cr.totalMs;
    // Show redirect chain info — not raw curl dump
    QStringList redirectLines;
    redirectLines.append(QStringLiteral("Redirect Check:"));
    redirectLines.append(QStringLiteral("  HTTP Status:      %1").arg(cr.statusCode));
    redirectLines.append(QStringLiteral("  Redirect Target:  %1").arg(cr.redirectLocation.isEmpty() ? QStringLiteral("(none)") : cr.redirectLocation));
    redirectLines.append(QStringLiteral("  Response Time:    %1 ms").arg(cr.totalMs, 0, 'f', 1));
    r.rawOutput = redirectLines.join('\n');
    r.details = r.rawOutput;
    return r;

}
} // namespace G5WebsiteUrl
