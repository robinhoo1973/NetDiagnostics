#include "engine/diagnostics/G5/G5Proto.h"
namespace G5WebsiteUrl {
DiagnosticResult telnetDiagnostics(const QString& target) {
    if (target.isEmpty())
        return skipped(DiagId::G5Telnet, "No target");
    QUrl u = G5WebsiteUrl::validate(target);
    if (u.scheme() != "telnet")
        return skipped(DiagId::G5Telnet, "Not Telnet");
    int port = G5WebsiteUrl::portForUrl(u);
    QElapsedTimer t; t.start();
    QTcpSocket sock;
    sock.connectToHost(u.host(), port);
    if (!sock.waitForConnected(5000))
        return result(DiagId::G5Telnet, "Connection failed", DiagStatus::Fail,
                      {}, t.elapsed());
    sock.waitForReadyRead(2000);
    QByteArray banner = sock.readAll();
    sock.write("QUIT\r\n");
    sock.disconnectFromHost();
    return result(DiagId::G5Telnet,
        banner.isEmpty() ? "Connected (no banner)" : QString::fromUtf8(banner).trimmed().left(200),
        banner.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass,
        QString::fromUtf8(banner), t.elapsed());
}

// ── MySQL (port 3306) — read handshake packet for server version ──────
}
} // namespace G5WebsiteUrl
} // namespace G5WebsiteUrl
