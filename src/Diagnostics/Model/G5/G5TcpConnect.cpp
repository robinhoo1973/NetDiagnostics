#include "Diagnostics/Model/G5/G5Common.h"
namespace G5WebsiteUrl {
DiagnosticResult tcpConnect(const QString& target) {
    if (target.isEmpty()) {
        auto r = g5Result(DiagId::G5TcpConnect, "No target", DiagStatus::Skipped);
        r.details = QStringLiteral("No target specified");
        r.rawOutput = r.details;
        return r;
    }
    QUrl u = validate(target);
    if (!u.isValid() || u.host().isEmpty()) {
        auto r = g5Result(DiagId::G5TcpConnect, "Invalid target", DiagStatus::Fail);
        r.details = QStringLiteral("Invalid target URL");
        r.rawOutput = r.details;
        r.errorOutput = QStringLiteral("Invalid target URL");
        return r;
    }
    int port = portForUrl(u);
    auto cr = NetworkProbe::tcpConnect(u.host(), port, 5000);
    // 5WHY (terminal output regression): g5Result() does not set details or
    // rawOutput — the Terminal Output section on the DetailPage was permanently
    // empty for TCP connect tests.  Populate on success only; on failure there
    // is no protocol data — terminal stays empty, error block shows the reason.
    auto r = g5Result(DiagId::G5TcpConnect,
        cr.connected ? QStringLiteral("Connected in %1ms").arg(cr.latencyMs)
                     : QStringLiteral("Failed: %1").arg(cr.error),
        cr.connected ? DiagStatus::Pass : DiagStatus::Fail);
    if (cr.connected) {
        QString outcome = QStringLiteral("Connected to %1:%2 in %3ms")
                              .arg(u.host()).arg(port).arg(cr.latencyMs);
        r.details = outcome;
        r.rawOutput = outcome;
    } else {
        // 5WHY: errorOutput only — terminal stays empty (no protocol data)
        r.errorOutput = QStringLiteral("TCP connect to %1:%2 failed: %3")
                            .arg(u.host()).arg(port).arg(cr.error);
    }
    r.properties.append(ResultProperty("Host", u.host()));
    r.properties.append(ResultProperty("Port", QString::number(port)));
    r.durationMs = cr.latencyMs;
    r.data[QStringLiteral("host")] = u.host();
    r.data[QStringLiteral("port")] = port;
    r.data[QStringLiteral("connected")] = cr.connected;
    r.data[QStringLiteral("latencyMs")] = cr.latencyMs;
    return r;

// ── G5.3 Service Banner ──────────────────────────────────────────────────
}
} // namespace G5WebsiteUrl
