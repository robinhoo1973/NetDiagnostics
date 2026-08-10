#include "Diagnostics/Model/G5/G5Common.h"
namespace G5WebsiteUrl {
DiagnosticResult ftpDiagnostics(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5FtpDiagnostics, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "ftp" && u.scheme() != "ftps")
        return g5Result(DiagId::G5FtpDiagnostics, "Not FTP", DiagStatus::Skipped);
    int port = portForUrl(u);
    QElapsedTimer t; t.start();
    QTcpSocket sock;
    sock.connectToHost(u.host(), port);
    if (!sock.waitForConnected(5000)) {
        auto r = g5Result(DiagId::G5FtpDiagnostics, "Connection failed", DiagStatus::Fail);
        r.durationMs = t.elapsed();
        r.data["host"] = u.host();
        r.data["port"] = port;
        r.data["connected"] = false;
        r.data["banner"] = QString();
        r.data["latencyMs"] = t.elapsed();
        return r;
    }
    sock.waitForReadyRead(3000);
    QByteArray banner = sock.readAll();
    sock.write("QUIT\r\n");
    sock.disconnectFromHost();
    auto r = g5Result(DiagId::G5FtpDiagnostics,
        banner.isEmpty() ? "No banner" : QString::fromUtf8(banner).trimmed().left(200),
        banner.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass);
    r.durationMs = t.elapsed();
    r.data["host"] = u.host();
    r.data["port"] = port;
    r.data["connected"] = true;
    r.data["banner"] = QString::fromUtf8(banner).trimmed().left(200);
    r.data["latencyMs"] = t.elapsed();
    return r;

}
} // namespace G5WebsiteUrl
