#include "engine/diagnostics/G5/G5Common.h"
namespace G5WebsiteUrl {
DiagnosticResult tcpConnect(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5TcpConnect, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (!u.isValid() || u.host().isEmpty())
        return g5Result(DiagId::G5TcpConnect, "Invalid target", DiagStatus::Fail);
    int port = portForUrl(u);
    auto cr = NetworkProbe::tcpConnect(u.host(), port, 5000);
    auto r = g5Result(DiagId::G5TcpConnect,
        cr.connected ? QStringLiteral("Connected in %1ms").arg(cr.latencyMs)
                     : QStringLiteral("Failed: %1").arg(cr.error),
        cr.connected ? DiagStatus::Pass : DiagStatus::Fail);
    r.properties.append(ResultProperty("Host", u.host()));
    r.properties.append(ResultProperty("Port", QString::number(port)));
    r.durationMs = cr.latencyMs;
    return r;
}

// ── G5.3 Service Banner ──────────────────────────────────────────────────
}
} // namespace G5WebsiteUrl
