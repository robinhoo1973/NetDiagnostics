#include "Diagnostics/Model/G5/G5Common.h"
namespace G5WebsiteUrl {
DiagnosticResult urlParsing(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5UrlParsing, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (!u.isValid()) return g5Result(DiagId::G5UrlParsing, "Invalid URL", DiagStatus::Fail);
    auto r = g5Result(DiagId::G5UrlParsing, QStringLiteral("Scheme=%1 Host=%2 Port=%3").arg(u.scheme(), u.host()).arg(portForUrl(u)));
    r.rawOutput = QStringLiteral("Scheme: %1\nHost: %2\nPort: %3\nPath: %4\nQuery: %5")
        .arg(u.scheme(), u.host()).arg(portForUrl(u)).arg(u.path(), u.query());
    r.details = r.rawOutput;
    r.data[QStringLiteral("scheme")] = u.scheme();
    r.data[QStringLiteral("host")] = u.host();
    r.data[QStringLiteral("port")] = portForUrl(u);
    r.data[QStringLiteral("path")] = u.path();
    r.data[QStringLiteral("query")] = u.query();
    return r;

// ── G5.2 TCP Connect ─────────────────────────────────────────────────────
}
} // namespace G5WebsiteUrl
