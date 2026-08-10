#include "Diagnostics/Model/G5/G5Common.h"
namespace G5WebsiteUrl {
DiagnosticResult postgresDiagnostics(const QString& target) {
    if (target.isEmpty())
        return skipped(DiagId::G5Postgres, "No target");
    QUrl u = validate(target);
    if (u.scheme() != "postgresql")
        return skipped(DiagId::G5Postgres, "Not PostgreSQL");
    int port = portForUrl(u);
    QElapsedTimer t; t.start();
    QTcpSocket sock;
    sock.connectToHost(u.host(), port);
    if (!sock.waitForConnected(5000)) {
        auto r = result(DiagId::G5Postgres, "Connection failed", DiagStatus::Fail,
                      {}, t.elapsed());
        r.data["host"] = u.host();
        r.data["port"] = port;
        r.data["connected"] = false;
        r.data["responseType"] = QString();
        r.data["responseInfo"] = QString();
        r.data["authOk"] = false;
        r.data["latencyMs"] = t.elapsed();
        return r;
    }
    // Send a minimal StartupMessage (protocol 3.0, user "diagnostic", no DB)
    // Format: [4-byte length][4-byte protocol 3.0]
    // + [string "user\0diagnostic\0\0"] (terminator byte)
    QByteArray startup;
    // protocol version 3.0 (196608 = 0x00030000)
    startup.append(static_cast<char>(0x00));
    startup.append(static_cast<char>(0x00));
    startup.append(static_cast<char>(0x03));
    startup.append(static_cast<char>(0x00));
    // key "user"
    startup.append("user");
    startup.append('\0');
    // value "diagnostic"
    startup.append("diagnostic");
    startup.append('\0');
    // terminator
    startup.append('\0');
    // prepend length (4 bytes, network byte order, including itself)
    QByteArray packet;
    quint32 len = startup.size() + 4;
    // Big-endian 4-byte length (network byte order)
    packet.append(static_cast<char>((len >> 24) & 0xFF));
    packet.append(static_cast<char>((len >> 16) & 0xFF));
    packet.append(static_cast<char>((len >> 8) & 0xFF));
    packet.append(static_cast<char>(len & 0xFF));
    packet.append(startup);
    sock.write(packet);
    sock.waitForBytesWritten(2000);
    sock.waitForReadyRead(3000);
    QByteArray resp = sock.readAll();
    sock.disconnectFromHost();
    if (resp.isEmpty()) {
        auto r = result(DiagId::G5Postgres, "No response", DiagStatus::Warning,
                      {}, t.elapsed());
        r.data["host"] = u.host();
        r.data["port"] = port;
        r.data["connected"] = true;
        r.data["responseType"] = QString();
        r.data["responseInfo"] = QString();
        r.data["authOk"] = false;
        r.data["latencyMs"] = t.elapsed();
        return r;
    }
    // First byte: 'E'=Error, 'R'=Authentication, 'N'=Notice
    char type = resp.size() > 0 ? resp.at(0) : 0;
    QString info;
    switch (type) {
        case 'R': info = "Authentication request"; break;
        case 'E': info = "Error response"; break;
        case 'N': info = "Notice"; break;
        default:  info = QString("Response type '%1'").arg(type); break;
    }
    auto r = result(DiagId::G5Postgres,
        QString("PostgreSQL: %1").arg(info),
        (type == 'R') ? DiagStatus::Pass : DiagStatus::Warning,
        QString::fromUtf8(resp.toHex(' ')), t.elapsed());
    r.data["host"] = u.host();
    r.data["port"] = port;
    r.data["connected"] = true;
    r.data["responseType"] = QString(QChar(type));
    r.data["responseInfo"] = info;
    r.data["authOk"] = (type == 'R');
    r.data["latencyMs"] = t.elapsed();
    return r;
}

// ── Redis (port 6379) — PING/PONG ─────────────────────────────────────
} // namespace G5WebsiteUrl
