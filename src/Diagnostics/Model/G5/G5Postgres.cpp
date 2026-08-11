#include "Diagnostics/Model/G5/G5Common.h"
namespace G5WebsiteUrl {
DiagnosticResult postgresDiagnostics(const QString& target) {
    if (target.isEmpty()) return skipped(DiagId::G5Postgres, "No target");
    QUrl u = validate(target);
    if (u.scheme() != "postgresql") return skipped(DiagId::G5Postgres, "Not PostgreSQL");
    // Minimal StartupMessage (protocol 3.0, user "diagnostic", no DB):
    // [4-byte length][4-byte protocol 3.0][string "user\0diagnostic\0\0"]
    QByteArray startup;
    startup.append(static_cast<char>(0x00));
    startup.append(static_cast<char>(0x00));
    startup.append(static_cast<char>(0x03));
    startup.append(static_cast<char>(0x00));
    startup.append("user"); startup.append('\0');
    startup.append("diagnostic"); startup.append('\0');
    startup.append('\0');
    QByteArray packet;
    quint32 len = startup.size() + 4;  // Big-endian length incl. itself
    packet.append(static_cast<char>((len >> 24) & 0xFF));
    packet.append(static_cast<char>((len >> 16) & 0xFF));
    packet.append(static_cast<char>((len >> 8) & 0xFF));
    packet.append(static_cast<char>(len & 0xFF));
    packet.append(startup);
    // 5WHY: was the LAST G5 test still hand-rolling raw QTcpSocket — the
    // shared g5Probe() sends the StartupMessage and reads the response.
    auto p = g5Probe(u, packet, 5000, 3000);
    auto r = g5ProbeResult(DiagId::G5Postgres, u, p);
    if (!p.connected) {
        r.data["responseType"] = QString();
        r.data["responseInfo"] = QString();
        r.data["authOk"] = false;
        return r;
    }
    QByteArray resp = p.banner;
    if (resp.isEmpty()) {
        r.summary = "No response";
        r.status = DiagStatus::Warning;
        r.data["responseType"] = QString();
        r.data["responseInfo"] = QString();
        r.data["authOk"] = false;
        autoErrorOutput(r);
        return r;
    }
    // First byte: 'R'=Authentication, 'E'=Error, 'N'=Notice
    char type = resp.at(0);
    QString info;
    switch (type) {
        case 'R': info = "Authentication request"; break;
        case 'E': info = "Error response"; break;
        case 'N': info = "Notice"; break;
        default:  info = QString("Response type '%1'").arg(type); break;
    }
    r.summary = QString("PostgreSQL: %1").arg(info);
    r.status = (type == 'R') ? DiagStatus::Pass : DiagStatus::Warning;
    r.rawOutput = r.details = QString::fromUtf8(resp.toHex(' '));
    r.data["responseType"] = QString(QChar(type));
    r.data["responseInfo"] = info;
    r.data["authOk"] = (type == 'R');
    autoErrorOutput(r);
    return r;
}
} // namespace G5WebsiteUrl
