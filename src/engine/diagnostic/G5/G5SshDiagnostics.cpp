#include "engine/diagnostic/G5/g5common.h"
namespace G5WebsiteUrl {
DiagnosticResult sshDiagnostics(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5SshDiagnostics, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "ssh" && u.scheme() != "sftp")
        return g5Result(DiagId::G5SshDiagnostics, "Not SSH", DiagStatus::Skipped);
    int port = portForUrl(u);
    QTcpSocket sock;
    sock.connectToHost(u.host(), port);
    if (!sock.waitForConnected(5000))
        return g5Result(DiagId::G5SshDiagnostics, "Connection failed", DiagStatus::Fail);
    sock.waitForReadyRead(3000);
    QByteArray banner = sock.readAll();
    sock.disconnectFromHost();
    QString bstr = QString::fromUtf8(banner).trimmed().left(200);
    QString version;
    if (bstr.startsWith("SSH-")) version = bstr.section(' ', 0, 0);
    return g5Result(DiagId::G5SshDiagnostics,
        version.isEmpty() ? "No SSH banner" : version,
        version.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass);
}

}
