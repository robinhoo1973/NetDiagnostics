#include "engine/diagnostics/G5/g5common.h"
namespace G5WebsiteUrl {
DiagnosticResult ftpDiagnostics(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5FtpDiagnostics, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "ftp" && u.scheme() != "ftps")
        return g5Result(DiagId::G5FtpDiagnostics, "Not FTP", DiagStatus::Skipped);
    int port = portForUrl(u);
    QTcpSocket sock;
    sock.connectToHost(u.host(), port);
    if (!sock.waitForConnected(5000))
        return g5Result(DiagId::G5FtpDiagnostics, "Connection failed", DiagStatus::Fail);
    sock.waitForReadyRead(3000);
    QByteArray banner = sock.readAll();
    sock.write("QUIT\r\n");
    sock.disconnectFromHost();
    return g5Result(DiagId::G5FtpDiagnostics,
        banner.isEmpty() ? "No banner" : QString::fromUtf8(banner).trimmed().left(200),
        banner.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass);
}

}
