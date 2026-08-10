#include "Diagnostics/Model/G5/G5Common.h"
namespace G5WebsiteUrl {
#if !defined(NO_CURL)
DiagnosticResult httpCompression(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5HttpCompression, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (!isHttpScheme(u.scheme()))
        return g5Result(DiagId::G5HttpCompression, "Not HTTP(S)", DiagStatus::Skipped);
    auto cr = curlHttp(u, 15000, true);
    if (!cr.ok) return g5Result(DiagId::G5HttpCompression, cr.error, DiagStatus::Fail);
    bool compressed = false; QString enc;
    for (const auto& line : cr.lines) {
        if (line.startsWith("< ") && line.contains("content-encoding", Qt::CaseInsensitive)) {
            auto colon = line.indexOf(':');
            if (colon > 2) enc = line.mid(colon + 1).trimmed();
            compressed = true;
        }
    }
    auto r = g5Result(DiagId::G5HttpCompression,
        compressed ? QStringLiteral("Compressed: %1").arg(enc) : "Uncompressed", DiagStatus::Info);
    // Show compression analysis — not raw curl dump
    QStringList compLines;
    compLines.append(QStringLiteral("Content-Encoding Check:"));
    compLines.append(QStringLiteral("  Encoding:   %1").arg(compressed ? enc : QStringLiteral("none (uncompressed)")));
    compLines.append(QStringLiteral("  HTTP Status:%1").arg(cr.statusCode));
    compLines.append(QStringLiteral("  Response:   %1 ms").arg(cr.totalMs, 0, 'f', 1));
    r.rawOutput = compLines.join('\n');
    r.details = r.rawOutput;
    r.durationMs = cr.totalMs;
    r.data[QStringLiteral("compressed")] = compressed;
    r.data[QStringLiteral("contentEncoding")] = enc;
    r.data[QStringLiteral("statusCode")] = cr.statusCode;
    // Waterfall timing for the Request-template chart.
    // 5WHY: same pattern as G5HttpHeaders — only totalMs was stored.
    r.data[QStringLiteral("dnsMs")] = cr.dnsMs;
    r.data[QStringLiteral("connectMs")] = cr.connectMs;
    r.data[QStringLiteral("sslMs")] = cr.appConnectMs;
    r.data[QStringLiteral("firstByteMs")] = cr.firstByteMs;
    r.data[QStringLiteral("totalMs")] = cr.totalMs;
    return r;
}
#endif // NO_CURL
} // namespace G5WebsiteUrl
