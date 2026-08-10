#include "Diagnostics/Model/G5/G5Common.h"
namespace G5WebsiteUrl {
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
    if (!sock.waitForConnected(5000)) {
        auto r = result(DiagId::G5Mongodb, "Connection failed", DiagStatus::Fail,
                      {}, t.elapsed());
        r.data["host"] = u.host();
        r.data["port"] = port;
        r.data["connected"] = false;
        r.data["version"] = QString();
        r.data["responded"] = false;
        r.data["latencyMs"] = t.elapsed();
        return r;
    }
    // MongoDB wire protocol: send isMaster command
    // Simple OP_QUERY: { isMaster: 1 } on admin.$cmd
    QByteArray msg;
    msg.append('\x3f', 1); // bit flags (SlaveOk)
    msg.append('\0', 3);   // reserved, cursor ID
    msg.append('\0', 4);   // namespace "$cmd" prefix
    msg.append("admin.$cmd");
    msg.append('\0');
    // 5WHY: numberToSkip was 1 — for an OP_QUERY against admin.$cmd the skip
    // field must be 0 (commands never skip).  Servers tolerate it, but the
    // field as written (skip=1, return=1) is not the canonical command query.
    msg.append('\0', 4); // numberToSkip=0
    msg.append('\x01', 4); // numberToReturn=1
    // BSON: { isMaster: 1 } → \x13\x00\x00\x00\x10 isMaster\0\x01\x00\x00\x00\0
    QByteArray bson;
    bson.append('\x13', 1);                           // total size
    bson.append('\0', 3);                              // padding
    bson.append('\x10');                               // int32 type
    bson.append("isMaster");
    bson.append('\0');
    bson.append('\x01'); bson.append('\0', 3);         // value 1
    bson.append('\0');                                 // terminator
    // Full OP_QUERY header — MongoDB wire protocol is LITTLE-endian for all
    // header int32 fields. 5WHY: the old header appended messageLength twice
    // (once big-endian byte-by-byte, then again host-order via
    // reinterpret_cast) and built opCode as "\xd4\x07\x07\x07" — so the
    // declared length (16+bson) never matched the bytes sent and the opcode
    // was 0x070707d4 instead of 2004. Servers misparse the frame and the
    // handshake never succeeds. Build exactly 16 bytes with a single LE writer.
    QByteArray header;
    const quint32 kMsgLen = 16 + bson.size();
    auto appendLE32 = [&header](quint32 v) {
        header.append(static_cast<char>(v & 0xFF));
        header.append(static_cast<char>((v >> 8) & 0xFF));
        header.append(static_cast<char>((v >> 16) & 0xFF));
        header.append(static_cast<char>((v >> 24) & 0xFF));
    };
    appendLE32(kMsgLen);   // messageLength
    appendLE32(1);         // requestID
    appendLE32(0);         // responseTo
    appendLE32(2004);      // OP_QUERY
    sock.write(header + msg + bson);
    sock.waitForBytesWritten(2000);
    // 5WHY: single waitForReadyRead can return a partial frame on slow links —
    // read until the full response body (16-byte header + declared length) is
    // available or the deadline expires.
    QByteArray resp;
    QElapsedTimer readTimer; readTimer.start();
    while (readTimer.elapsed() < 3000) {
        if (!sock.waitForReadyRead(3000 - (int)readTimer.elapsed())) break;
        resp.append(sock.readAll());
        if (resp.size() >= 16) {
            quint32 declared = (quint32)(unsigned char)resp[0]
                             | ((quint32)(unsigned char)resp[1] << 8)
                             | ((quint32)(unsigned char)resp[2] << 16)
                             | ((quint32)(unsigned char)resp[3] << 24);
            if (resp.size() >= (int)declared) break;  // full frame received
        }
    }
    sock.disconnectFromHost();
    if (resp.size() < 16) { // MongoDB header is 16 bytes + doc
        auto r = result(DiagId::G5Mongodb, "No response", DiagStatus::Warning,
                      {}, t.elapsed());
        r.data["host"] = u.host();
        r.data["port"] = port;
        r.data["connected"] = true;
        r.data["version"] = QString();
        r.data["responded"] = false;
        r.data["latencyMs"] = t.elapsed();
        return r;
    }
    // Look for version string in response (BSON document — binary, not text).
    // 5WHY: QString::fromUtf8(resp) + searching for "version" then quote
    // delimiters never matched because BSON stores strings as <len>\0x02\0
    // <key>\0 <strlen><utf8>\0, not JSON quotes.  Scan the BSON bytes for a
    // "\x02version\x00" element and read its length-prefixed UTF-8 value.
    QString version;
    int vidx = resp.indexOf("\x02version\x00", 0);
    if (vidx >= 0) {
        int lenPos = vidx + 1 + 8;  // type byte + "version\0"
        if (lenPos + 4 <= resp.size()) {
            quint32 slen = (quint32)(unsigned char)resp[lenPos]
                         | ((quint32)(unsigned char)resp[lenPos + 1] << 8)
                         | ((quint32)(unsigned char)resp[lenPos + 2] << 16)
                         | ((quint32)(unsigned char)resp[lenPos + 3] << 24);
            if (slen > 0 && slen < 256 && lenPos + 4 + (int)slen <= resp.size())
                version = QString::fromUtf8(resp.mid(lenPos + 4, (int)slen - 1));
        }
    }
    auto r = result(DiagId::G5Mongodb,
        version.isEmpty() ? "MongoDB (responded)" : QString("MongoDB %1").arg(version).left(200),
        DiagStatus::Pass,
        resp.left(500), t.elapsed());
    r.data["host"] = u.host();
    r.data["port"] = port;
    r.data["connected"] = true;
    r.data["version"] = version;
    r.data["responded"] = true;
    r.data["latencyMs"] = t.elapsed();
    return r;
}

// ── LDAP (port 389) / LDAPS (port 636) — bind request ─────────────────
} // namespace G5WebsiteUrl
