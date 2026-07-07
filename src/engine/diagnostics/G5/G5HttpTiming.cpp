#include "engine/diagnostics/G5/G5Common.h"
#ifndef NO_CURL
namespace G5WebsiteUrl {
DiagnosticResult httpTiming(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5HttpTiming, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "http" && u.scheme() != "https")
        return g5Result(DiagId::G5HttpTiming, "Not HTTP(S)", DiagStatus::Skipped);
    auto cr = curlHttp(u, 30000, true);
    if (!cr.ok) return g5Result(DiagId::G5HttpTiming, cr.error, DiagStatus::Fail);
    auto r = g5Result(DiagId::G5HttpTiming,
        QStringLiteral("DNS=%1ms Connect=%2ms SSL=%3ms FirstByte=%4ms Total=%5ms")
            .arg(cr.dnsMs, 0, 'f', 1).arg(cr.connectMs, 0, 'f', 1)
            .arg(cr.appConnectMs, 0, 'f', 1).arg(cr.firstByteMs, 0, 'f', 1)
            .arg(cr.totalMs, 0, 'f', 1),
        cr.totalMs < 1000 ? DiagStatus::Pass :
        cr.totalMs < 3000 ? DiagStatus::Warning : DiagStatus::Fail);
    r.rawOutput = cr.lines.join('\n'); r.details = r.rawOutput;
    r.durationMs = cr.totalMs;
    // Expose per-phase curl timing as structured properties
    r.properties.append({QStringLiteral("DNS Lookup"),    QStringLiteral("%1 ms").arg(cr.dnsMs, 0, 'f', 1)});
    r.properties.append({QStringLiteral("TCP Connect"),   QStringLiteral("%1 ms").arg(cr.connectMs, 0, 'f', 1)});
    r.properties.append({QStringLiteral("SSL Handshake"), QStringLiteral("%1 ms").arg(cr.appConnectMs, 0, 'f', 1)});
    r.properties.append({QStringLiteral("Time to First Byte"), QStringLiteral("%1 ms").arg(cr.firstByteMs, 0, 'f', 1)});
    r.properties.append({QStringLiteral("Total Time"),    QStringLiteral("%1 ms").arg(cr.totalMs, 0, 'f', 1)});
    r.properties.append({QStringLiteral("HTTP Status"),   QString::number(cr.statusCode)});
    if (!cr.redirectLocation.isEmpty())
        r.properties.append({QStringLiteral("Redirect"),  cr.redirectLocation});
    return r;
}
#endif // NO_CURL

}
#endif // NO_CURL
