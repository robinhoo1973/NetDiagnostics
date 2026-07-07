#include "engine/diagnostic/G5/g5common.h"
namespace G5WebsiteUrl {
DiagnosticResult urlParsing(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5UrlParsing, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (!u.isValid()) return g5Result(DiagId::G5UrlParsing, "Invalid URL", DiagStatus::Fail);
    auto r = g5Result(DiagId::G5UrlParsing, QStringLiteral("Scheme=%1 Host=%2 Port=%3").arg(u.scheme(), u.host()).arg(portForUrl(u)));
    r.rawOutput = QStringLiteral("Scheme: %1\nHost: %2\nPort: %3\nPath: %4\nQuery: %5")
        .arg(u.scheme(), u.host()).arg(portForUrl(u)).arg(u.path(), u.query());
    r.details = r.rawOutput;
    return r;
}

// ── G5.2 TCP Connect ─────────────────────────────────────────────────────
}
