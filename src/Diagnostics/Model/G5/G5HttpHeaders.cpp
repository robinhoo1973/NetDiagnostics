#include "Diagnostics/Model/G5/G5Common.h"
#if !defined(NO_CURL)
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
    r.durationMs = cr.totalMs;
    r.properties.append({QStringLiteral("HTTP Status"), QString::number(cr.statusCode)});
    r.properties.append({QStringLiteral("Response Time"), QStringLiteral("%1 ms").arg(cr.totalMs, 0, 'f', 1)});
    // Build header table from curl output
    QStringList headerLines;
    headerLines.append(QStringLiteral("%1  %2")
        .arg(QStringLiteral("Header"), -32).arg(QStringLiteral("Value")));
    headerLines.append(QStringLiteral("%1  %2")
        .arg(QString(32, '-')).arg(QString(48, '-')));
    int headerCount = 0;
    for (const auto& line : cr.lines) {
        if (line.startsWith('<') && line.contains(':')) {
            ++headerCount;
            auto colon = line.indexOf(':');
            QString name  = line.mid(2, colon - 2).trimmed();
            QString value = line.mid(colon + 1).trimmed();
            headerLines.append(QStringLiteral("%1  %2").arg(name, -32).arg(value.left(48)));
        }
    }
    r.rawOutput = headerLines.join('\n');
    r.details = r.rawOutput;
    r.properties.append({QStringLiteral("Response Headers"), QString::number(headerCount)});
    return r;
}
#endif // NO_CURL

// ── G5.5 Curl Verbose (full GET, curl -v style complete output) ──────────
} // namespace G5WebsiteUrl
