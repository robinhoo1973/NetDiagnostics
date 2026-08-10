#include "Diagnostics/Model/G5/G5Common.h"
namespace G5WebsiteUrl {
DiagnosticResult telnetDiagnostics(const QString& target) {
    if (target.isEmpty())
        return skipped(DiagId::G5Telnet, "No target");
    QUrl u = validate(target);
    if (u.scheme() != "telnet")
        return skipped(DiagId::G5Telnet, "Not Telnet");
    int port = portForUrl(u);
    QElapsedTimer t; t.start();
    QTcpSocket sock;
    sock.connectToHost(u.host(), port);
    if (!sock.waitForConnected(5000)) {
        auto r = result(DiagId::G5Telnet, "Connection failed", DiagStatus::Fail,
                      {}, t.elapsed());
        r.data["host"] = u.host();
        r.data["port"] = port;
        r.data["connected"] = false;
        r.data["banner"] = QString();
        r.data["latencyMs"] = t.elapsed();
        return r;
    }
    sock.waitForReadyRead(2000);
    QByteArray banner = sock.readAll();
    sock.write("QUIT\r\n");
    sock.disconnectFromHost();
    auto r = result(DiagId::G5Telnet,
        banner.isEmpty() ? "Connected (no banner)" : QString::fromUtf8(banner).trimmed().left(200),
        banner.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass,
        QString::fromUtf8(banner), t.elapsed());
    r.data["host"] = u.host();
    r.data["port"] = port;
    r.data["connected"] = true;
    r.data["banner"] = QString::fromUtf8(banner).trimmed().left(200);
    r.data["latencyMs"] = t.elapsed();
    return r;
}

} // namespace G5WebsiteUrl
