#include "engine/diagnostics/G5/G5Proto.h"
namespace G5WebsiteUrl {
DiagnosticResult mysqlDiagnostics(const QString& target) {
    if (target.isEmpty())
        return skipped(DiagId::G5Mysql, "No target");
    QUrl u = G5WebsiteUrl::validate(target);
    if (u.scheme() != "mysql")
        return skipped(DiagId::G5Mysql, "Not MySQL");
    int port = G5WebsiteUrl::portForUrl(u);
    QElapsedTimer t; t.start();
    QTcpSocket sock;
    sock.connectToHost(u.host(), port);
    if (!sock.waitForConnected(5000))
        return result(DiagId::G5Mysql, "Connection failed", DiagStatus::Fail,
                      {}, t.elapsed());
    sock.waitForReadyRead(2000);
    QByteArray data = sock.readAll();
    sock.disconnectFromHost();
    if (data.size() < 5)
        return result(DiagId::G5Mysql, "No handshake packet", DiagStatus::Warning,
                      {}, t.elapsed());
    // MySQL handshake: [4-byte length][1-byte seq][1-byte protocol][null-term version][4-byte threadid]...
    int verStart = 5; // skip length(3)+seq(1)+protocol(1)
    int verEnd = data.indexOf('\0', verStart);
    QString version = (verEnd > verStart)
        ? QString::fromUtf8(data.mid(verStart, verEnd - verStart))
        : QString();
    return result(DiagId::G5Mysql,
        version.isEmpty() ? "MySQL (version unknown)" : QString("MySQL %1").arg(version),
        version.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass,
        QString::fromUtf8(data.toHex(' ')), t.elapsed());
}

// ── PostgreSQL (port 5432) — read StartupMessage response ─────────────
}
} // namespace G5WebsiteUrl
