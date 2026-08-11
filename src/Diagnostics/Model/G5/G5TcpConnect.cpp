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
    // empty for TCP connect tests.  Populate both fields with the connection
    // outcome so the terminal section renders diagnostic text.
    QString outcome = cr.connected
        ? QStringLiteral("Connected to %1:%2 in %3ms")
              .arg(u.host()).arg(port).arg(cr.latencyMs)
        : QStringLiteral("Failed to connect to %1:%2 — %3")
              .arg(u.host()).arg(port).arg(cr.error);
    auto r = g5Result(DiagId::G5TcpConnect,
        cr.connected ? QStringLiteral("Connected in %1ms").arg(cr.latencyMs)
                     : QStringLiteral("Failed: %1").arg(cr.error),
        cr.connected ? DiagStatus::Pass : DiagStatus::Fail);
    r.details = outcome;
    r.rawOutput = outcome;
    // 5WHY: errorOutput was never set — the DetailPage error section was
    // invisible even for connection failures.  Populate it so the red
    // error block renders on the detail page for failed connections.
    if (!cr.connected)
        r.errorOutput = QStringLiteral("TCP connect to %1:%2 failed: %3")
                            .arg(u.host()).arg(port).arg(cr.error);
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
