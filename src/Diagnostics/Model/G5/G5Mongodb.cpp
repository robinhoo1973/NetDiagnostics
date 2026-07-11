#include "Diagnostics/Model/G5/G5Common.h"
DiagnosticResult mongodbDiagnostics(const QString& target) {
    if (target.isEmpty())
        return skipped(DiagId::G5Mongodb, "No target");
    QUrl u = validate(target);
    if (u.scheme() != "mongodb")
        return skipped(DiagId::G5Mongodb, "Not MongoDB");
    int port = portForUrl(u);
    QElapsedTimer t; t.start();
    QTcpSocket sock;
    sock.connectToHost(u.host(), port);
    if (!sock.waitForConnected(5000))
        return result(DiagId::G5Mongodb, "Connection failed", DiagStatus::Fail,
                      {}, t.elapsed());
    // MongoDB wire protocol: send isMaster command
    // Simple OP_QUERY: { isMaster: 1 } on admin.$cmd
    QByteArray msg;
    msg.append('\x3f', 1); // bit flags (SlaveOk)
    msg.append('\0', 3);   // reserved, cursor ID
    msg.append('\0', 4);   // namespace "$cmd" prefix
    msg.append("admin.$cmd");
    msg.append('\0');
    msg.append('\x01', 4); // numberToSkip=1, numberToReturn=1
    // BSON: { isMaster: 1 } → \x13\x00\x00\x00\x10 isMaster\0\x01\x00\x00\x00\0
    QByteArray bson;
    bson.append('\x13', 1);                           // total size
    bson.append('\0', 3);                              // padding
    bson.append('\x10');                               // int32 type
    bson.append("isMaster");
    bson.append('\0');
    bson.append('\x01'); bson.append('\0', 3);         // value 1
    bson.append('\0');                                 // terminator
    // Full OP_QUERY header
    QByteArray header;
    quint32 msgLen = 16 + bson.size();
    // Big-endian 4-byte message length
    header.append(static_cast<char>((msgLen >> 24) & 0xFF));
    header.append(static_cast<char>((msgLen >> 16) & 0xFF));
    header.append(static_cast<char>((msgLen >> 8) & 0xFF));
    header.append(static_cast<char>(msgLen & 0xFF));
    header.append(reinterpret_cast<const char*>(&msgLen), 4);
    header.append('\0', 4);    // requestID
    header.append('\0', 4);    // responseTo
    header.append('\xd4', 1);  // OP_QUERY (2004) little-endian
    header.append('\x07', 3);
    sock.write(header + msg + bson);
    sock.waitForBytesWritten(2000);
    sock.waitForReadyRead(3000);
    QByteArray resp = sock.readAll();
    sock.disconnectFromHost();
    if (resp.size() < 36) // MongoDB header is 16 bytes + doc
        return result(DiagId::G5Mongodb, "No response", DiagStatus::Warning,
                      {}, t.elapsed());
    // Look for version string in response (BSON document)
    QString raw = QString::fromUtf8(resp);
    // Extract "version" field if present
    int vidx = raw.indexOf("version");
    QString version;
    if (vidx >= 0) {
        int vstart = raw.indexOf('"', vidx + 10);
        int vend = vstart >= 0 ? raw.indexOf('"', vstart + 1) : -1;
        if (vend > vstart) version = raw.mid(vstart + 1, vend - vstart - 1);
    }
    return result(DiagId::G5Mongodb,
        version.isEmpty() ? "MongoDB (responded)" : QString("MongoDB %1").arg(version).left(200),
        DiagStatus::Pass,
        raw.left(500), t.elapsed());

// ── LDAP (port 389) / LDAPS (port 636) — bind request ─────────────────
} // namespace G5WebsiteUrl
} // namespace G5WebsiteUrl
