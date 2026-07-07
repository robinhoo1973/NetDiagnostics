#include "engine/diagnostic/G5/g5proto.h"
namespace G5WebsiteUrl {
DiagnosticResult ldapDiagnostics(const QString& target) {
    if (target.isEmpty())
        return skipped(DiagId::G5Ldap, "No target");
    QUrl u = G5WebsiteUrl::validate(target);
    QString scheme = u.scheme().toLower();
    if (scheme != "ldap" && scheme != "ldaps")
        return skipped(DiagId::G5Ldap, "Not LDAP(S)");
    int port = G5WebsiteUrl::portForUrl(u);
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
    // LDAP result: bindResponse tag 0x61 → success
    bool hasBindResp = resp.contains('\x61');
    return result(DiagId::G5Ldap,
        hasBindResp ? "LDAP bind response received"
                    : QString::fromUtf8(resp).left(200),
        hasBindResp ? DiagStatus::Pass : DiagStatus::Warning,
        QString::fromUtf8(resp.toHex(' ')), t.elapsed());
}

// ── MQTT (port 1883) / MQTTS (port 8883) — CONNECT/CONNACK ───────────
}
