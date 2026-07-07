#include "engine/diagnostics/G5/G5Common.h"
namespace G5WebsiteUrl {
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
    r.rawOutput = cr.lines.join('\n'); r.details = r.rawOutput;
    r.durationMs = cr.totalMs;
    return r;
}

}
} // namespace G5WebsiteUrl
} // namespace G5WebsiteUrl
