#include "Diagnostics/Model/G5/G5Common.h"
namespace G5WebsiteUrl {
DiagnosticResult emailDiagnostics(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5EmailDiagnostics, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    QString scheme = u.scheme();
    if (scheme != "smtp" && scheme != "imap" && scheme != "pop3"
        && scheme != "smtps" && scheme != "imaps" && scheme != "pop3s")
        return g5Result(DiagId::G5EmailDiagnostics,
                         "Not email protocol (smtp/smtps/imap/imaps/pop3/pop3s)", DiagStatus::Skipped);
    int port = portForUrl(u);
    QElapsedTimer t; t.start();
    QTcpSocket sock;
    sock.connectToHost(u.host(), port);
    if (!sock.waitForConnected(5000)) {
        auto r = g5Result(DiagId::G5EmailDiagnostics, "Connection failed", DiagStatus::Fail);
        r.durationMs = t.elapsed();
        r.data["protocol"] = scheme;
        r.data["host"] = u.host();
        r.data["port"] = port;
        r.data["connected"] = false;
        r.data["banner"] = QString();
        r.data["latencyMs"] = t.elapsed();
        return r;
    }
    sock.waitForReadyRead(3000);
    QByteArray banner = sock.readAll();
    sock.disconnectFromHost();
    auto r = g5Result(DiagId::G5EmailDiagnostics,
        banner.isEmpty() ? "No banner" : QString::fromUtf8(banner).trimmed().left(200),
        banner.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass);
    r.durationMs = t.elapsed();
    r.data["protocol"] = scheme;
    r.data["host"] = u.host();
    r.data["port"] = port;
    r.data["connected"] = true;
    r.data["banner"] = QString::fromUtf8(banner).trimmed().left(200);
    r.data["latencyMs"] = t.elapsed();
    return r;
}

} // namespace G5WebsiteUrl
