#include "engine/diagnostics/G5/G5Common.h"
#ifndef NO_CURL
DiagnosticResult securityHeaders(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5SecurityHeaders, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "http" && u.scheme() != "https")
        return g5Result(DiagId::G5SecurityHeaders, "Not HTTP(S)", DiagStatus::Skipped);
    auto cr = curlHttp(u, 15000, false);
    if (!cr.ok) return g5Result(DiagId::G5SecurityHeaders, cr.error, DiagStatus::Fail);
    // Parse headers from curl output for security headers
    QStringList found;
    for (const auto& line : cr.lines) {
        if (!line.startsWith('<') || line.startsWith("< HTTP")) continue;
        auto colon = line.indexOf(':');
        if (colon > 2) found.append(line.mid(2, colon - 2).toLower().trimmed());
    }
    QStringList required = {"strict-transport-security","content-security-policy",
        "x-frame-options","x-content-type-options","x-xss-protection",
        "referrer-policy","permissions-policy"};
    QStringList missing;
    for (const auto& h : required)
        if (!found.contains(h)) missing.append(h);
    auto r = g5Result(DiagId::G5SecurityHeaders,
        missing.isEmpty() ? "All 7 present" : QStringLiteral("%1 missing").arg(missing.size()),
        missing.isEmpty() ? DiagStatus::Pass :
        missing.size() <= 4 ? DiagStatus::Warning : DiagStatus::Fail);
    // Show security header analysis — structured table
    QStringList secLines;
    secLines.append(QStringLiteral("Security Header Analysis:"));
    secLines.append(QString());
    secLines.append(QStringLiteral("  %1  %2")
        .arg(QStringLiteral("Header"), -30).arg(QStringLiteral("Status")));
    secLines.append(QStringLiteral("  %1  %2")
        .arg(QString(30, '-')).arg(QString(10, '-')));
    for (const auto& h : required) {
        bool present = found.contains(h);
        secLines.append(QStringLiteral("  %1  %2")
            .arg(h, -30)
            .arg(present ? QStringLiteral("✓ Present") : QStringLiteral("✗ Missing")));
    }
    secLines.append(QString());
    secLines.append(QStringLiteral("  Result: %1 of 7 security headers present").arg(7 - missing.size()));
    r.rawOutput = secLines.join('\n');
    r.details = r.rawOutput;
    r.durationMs = cr.totalMs;
    return r;
}
#endif // NO_CURL

// ── G5.7 SSL Certificate ─────────────────────────────────────────────────
}
