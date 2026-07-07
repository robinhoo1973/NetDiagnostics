#include "engine/diagnostics/G5/G5Proto.h"
namespace G5WebsiteUrl {
DiagnosticResult redisDiagnostics(const QString& target) {
    if (target.isEmpty())
        return skipped(DiagId::G5Redis, "No target");
    QUrl u = G5WebsiteUrl::validate(target);
    if (u.scheme() != "redis")
        return skipped(DiagId::G5Redis, "Not Redis");
    int port = G5WebsiteUrl::portForUrl(u);
    QElapsedTimer t; t.start();
    QTcpSocket sock;
    sock.connectToHost(u.host(), port);
    if (!sock.waitForConnected(5000))
        return result(DiagId::G5Redis, "Connection failed", DiagStatus::Fail,
                      {}, t.elapsed());
    sock.write("PING\r\n");
    sock.waitForBytesWritten(2000);
    sock.waitForReadyRead(2000);
    QByteArray resp = sock.readAll();
    sock.write("QUIT\r\n");
    sock.disconnectFromHost();
    bool pong = resp.trimmed().contains("PONG");
    return result(DiagId::G5Redis,
        pong ? "Redis: PONG" : (resp.isEmpty() ? "No response" : QString::fromUtf8(resp).trimmed().left(200)),
        pong ? DiagStatus::Pass : DiagStatus::Warning,
        resp.isEmpty() ? QString() : QString::fromUtf8(resp), t.elapsed());
}

// ── MongoDB (port 27017) — isMaster handshake ─────────────────────────
}
} // namespace G5WebsiteUrl
