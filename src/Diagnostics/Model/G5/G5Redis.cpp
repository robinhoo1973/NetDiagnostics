#include "Diagnostics/Model/G5/G5Common.h"
DiagnosticResult redisDiagnostics(const QString& target) {
    if (target.isEmpty())
        return skipped(DiagId::G5Redis, "No target");
    QUrl u = validate(target);
    if (u.scheme() != "redis")
        return skipped(DiagId::G5Redis, "Not Redis");
    int port = portForUrl(u);
        auto probe = NetworkProbe::tcpProbe(u.host(), port, 5000, 2000);


    if (!probe.connected)
        return result(DiagId::Redis, "Connection failed", DiagStatus::Fail,
                      {}, probe.elapsedMs);
    sock.write("PING\r\n");
    sock.waitForBytesWritten(2000);
    QByteArray resp = sock.readAll();
    sock.write("QUIT\r\n");

    bool pong = resp.trimmed().contains("PONG");
    return result(DiagId::G5Redis,
        pong ? "Redis: PONG" : (resp.isEmpty() ? "No response" : QString::fromUtf8(resp).trimmed().left(200)),
        pong ? DiagStatus::Pass : DiagStatus::Warning,
        resp.isEmpty() ? QString() : QString::fromUtf8(resp), t.elapsed());
}

// ── MongoDB (port 27017) — isMaster handshake ─────────────────────────
} // namespace G5WebsiteUrl
