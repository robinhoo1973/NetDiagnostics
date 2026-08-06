#include "Diagnostics/Model/G5/G5Common.h"
namespace G5WebsiteUrl {
DiagnosticResult ldapDiagnostics(const QString& target) {
    if (target.isEmpty())
        return skipped(DiagId::G5Ldap, "No target");
    QUrl u = validate(target);
    QString scheme = u.scheme().toLower();
    if (scheme != "ldap" && scheme != "ldaps")
        return skipped(DiagId::G5Ldap, "Not LDAP(S)");
    int port = portForUrl(u);
    QElapsedTimer t; t.start();
    QTcpSocket sock;
    sock.connectToHost(u.host(), port);
    if (!sock.waitForConnected(5000))
        return result(DiagId::G5Ldap, "Connection failed", DiagStatus::Fail,
                      {}, t.elapsed());
    // LDAP BindRequest: 0x30 [len] 0x02 0x01 0x03 0x04 [len] [DN] 0x80 [len] [pw]
    // Minimal anonymous bind: { messageID:1, protocolOp:bindRequest(0x60),
    //   version:3, name:"", authentication:simple("") }
    QByteArray ldapMsg;
    ldapMsg.append('\x30'); // SEQUENCE
    ldapMsg.append('\x0c'); // length 12
    ldapMsg.append('\x02'); // INTEGER messageID
    ldapMsg.append('\x01'); // length 1
    ldapMsg.append('\x01'); // value 1
    ldapMsg.append('\x60'); // bindRequest (APPLICATION 0)
    ldapMsg.append('\x07'); // length 7
    ldapMsg.append('\x02'); // INTEGER version
    ldapMsg.append('\x01'); // length 1
    ldapMsg.append('\x03'); // value 3
    ldapMsg.append('\x04'); // OCTET STRING name
    ldapMsg.append('\x00'); // length 0 (anonymous)
    ldapMsg.append('\x80'); // simple auth (CONTEXT 0)
    ldapMsg.append('\x00'); // length 0 (empty password)
    sock.write(ldapMsg);
    sock.waitForBytesWritten(2000);
    sock.waitForReadyRead(3000);
    QByteArray resp = sock.readAll();
    sock.disconnectFromHost();
    if (resp.isEmpty())
        return result(DiagId::G5Ldap, "No response", DiagStatus::Warning,
                      {}, t.elapsed());
    // 5WHY: resp.contains('\x61') matched 0x61 anywhere in the payload —
    // 0x61 is also ASCII 'a', so a garbage/error response could be reported
    // as a successful bind.  Parse the BER structure instead:
    //   LDAPMessage ::= SEQUENCE(0x30) { messageID INTEGER, protocolOp }
    //   bindResponse ::= [APPLICATION 1](0x61) SEQUENCE {
    //       resultCode ENUMERATED(0x0A) ... }
    // resultCode 0 = success.  Only accept the 0x61 tag near the top level
    // (bind replies are tiny — the tag sits within the first ~40 bytes).
    bool hasBindResp = false;
    bool bindOk = false;
    if (!resp.isEmpty() && (unsigned char)resp[0] == 0x30) {
        int tagIdx = resp.indexOf('\x61', 1);
        if (tagIdx >= 0 && tagIdx < 40) {
            hasBindResp = true;
            int p = tagIdx + 1;  // skip 0x61 tag
            // Skip the SEQUENCE length (short form 1 byte; long form 0x81/0x82)
            if (p < resp.size()) {
                unsigned char lenByte = (unsigned char)resp[p];
                if (lenByte == 0x81) p += 2;
                else if (lenByte == 0x82) p += 3;
                else p += 1;
            }
            // resultCode: ENUMERATED (0x0A) <len> <value>
            if (p + 2 < resp.size() && (unsigned char)resp[p] == 0x0A) {
                int rcLen = (unsigned char)resp[p + 1];
                if (rcLen == 1 && p + 3 <= resp.size())
                    bindOk = ((unsigned char)resp[p + 2] == 0);
                else if (rcLen == 4 && p + 6 <= resp.size())
                    bindOk = (resp[p + 2] == 0 && resp[p + 3] == 0
                              && resp[p + 4] == 0 && resp[p + 5] == 0);
            }
        }
    }
    return result(DiagId::G5Ldap,
        !hasBindResp ? QString::fromUtf8(resp).left(200)
                     : (bindOk ? QStringLiteral("LDAP bind succeeded")
                               : QStringLiteral("LDAP bind failed (resultCode != success)")),
        !hasBindResp ? DiagStatus::Warning
                     : (bindOk ? DiagStatus::Pass : DiagStatus::Warning),
        QString::fromUtf8(resp.toHex(' ')), t.elapsed());
}

// ── MQTT (port 1883) / MQTTS (port 8883) — CONNECT/CONNACK ───────────
} // namespace G5WebsiteUrl
