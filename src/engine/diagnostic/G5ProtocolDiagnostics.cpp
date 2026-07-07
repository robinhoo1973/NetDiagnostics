// =============================================================================
// G5ProtocolDiagnostics.cpp — per-scheme TCP protocol diagnostics.
//
// All diagnostics use QTcpSocket (available on ALL platforms including
// iOS/Android).  No libcurl dependency.  Works in NO_CURL builds.
// =============================================================================
#include "engine/diagnostic/G5WebsiteUrl.h"   // validate, portForUrl + declarations
#include "models/ResultProperty.h"
#include <QTcpSocket>
#include <QUrl>
#include <QElapsedTimer>

namespace G5WebsiteUrl {

namespace {

DiagId inferDiagFromScheme(const QUrl& u) {
    const QString s = u.scheme().toLower();
    if (s == "telnet")        return DiagId::G5Telnet;
    if (s == "mysql")         return DiagId::G5Mysql;
    if (s == "postgresql")    return DiagId::G5Postgres;
    if (s == "redis")         return DiagId::G5Redis;
    if (s == "mongodb")       return DiagId::G5Mongodb;
    if (s == "ldap" || s == "ldaps") return DiagId::G5Ldap;
    if (s == "mqtt" || s == "mqtts") return DiagId::G5Mqtt;
    return DiagId::G5ServiceBanner; // fallback
}

DiagnosticResult result(DiagId id, const QString& summary,
                         DiagStatus status, const QString& raw = {},
                         int durationMs = 0) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G5;
    r.summary = summary; r.status = status;
    r.rawOutput = raw; r.details = raw; r.durationMs = durationMs;
    return r;
}

DiagnosticResult skipped(DiagId id, const QString& reason) {
    return result(id, reason, DiagStatus::Skipped);
}

} // anonymous namespace

// ── Telnet (port 23) ──────────────────────────────────────────────────
DiagnosticResult telnetDiagnostics(const QString& target) {
    if (target.isEmpty())
        return skipped(DiagId::G5Telnet, "No target");
    QUrl u = G5WebsiteUrl::validate(target);
    if (u.scheme() != "telnet")
        return skipped(DiagId::G5Telnet, "Not Telnet");
    int port = G5WebsiteUrl::portForUrl(u);
    QElapsedTimer t; t.start();
    QTcpSocket sock;
    sock.connectToHost(u.host(), port);
    if (!sock.waitForConnected(5000))
        return result(DiagId::G5Telnet, "Connection failed", DiagStatus::Fail,
                      {}, t.elapsed());
    sock.waitForReadyRead(2000);
    QByteArray banner = sock.readAll();
    sock.write("QUIT\r\n");
    sock.disconnectFromHost();
    return result(DiagId::G5Telnet,
        banner.isEmpty() ? "Connected (no banner)" : QString::fromUtf8(banner).trimmed().left(200),
        banner.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass,
        QString::fromUtf8(banner), t.elapsed());
}

// ── MySQL (port 3306) — read handshake packet for server version ──────
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
DiagnosticResult postgresDiagnostics(const QString& target) {
    if (target.isEmpty())
        return skipped(DiagId::G5Postgres, "No target");
    QUrl u = G5WebsiteUrl::validate(target);
    if (u.scheme() != "postgresql")
        return skipped(DiagId::G5Postgres, "Not PostgreSQL");
    int port = G5WebsiteUrl::portForUrl(u);
    QElapsedTimer t; t.start();
    QTcpSocket sock;
    sock.connectToHost(u.host(), port);
    if (!sock.waitForConnected(5000))
        return result(DiagId::G5Postgres, "Connection failed", DiagStatus::Fail,
                      {}, t.elapsed());
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
    if (resp.isEmpty())
        return result(DiagId::G5Postgres, "No response", DiagStatus::Warning,
                      {}, t.elapsed());
    // First byte: 'E'=Error, 'R'=Authentication, 'N'=Notice
    char type = resp.size() > 0 ? resp.at(0) : 0;
    QString info;
    switch (type) {
        case 'R': info = "Authentication request"; break;
        case 'E': info = "Error response"; break;
        case 'N': info = "Notice"; break;
        default:  info = QString("Response type '%1'").arg(type); break;
    }
    return result(DiagId::G5Postgres,
        QString("PostgreSQL: %1").arg(info),
        (type == 'R') ? DiagStatus::Pass : DiagStatus::Warning,
        QString::fromUtf8(resp.toHex(' ')), t.elapsed());
}

// ── Redis (port 6379) — PING/PONG ─────────────────────────────────────
DiagnosticResult redisDiagnostics(const QString& target) {
    if (target.isEmpty())
        return skipped(DiagId::G5Redis, "No target");
    QUrl u = G5WebsiteUrl::validate(target);
    if (u.scheme() != "redis")
        return skipped(DiagId::G5Redis, "Not Redis");
    int port = G5WebsiteUrl::portForUrl(u);
    QElapsedTimer t; t.start();
    QTcpSocket sock;
    sock.connectToHost(u.host(), port);
    if (!sock.waitForConnected(5000))
        return result(DiagId::G5Redis, "Connection failed", DiagStatus::Fail,
                      {}, t.elapsed());
    sock.write("PING\r\n");
    sock.waitForBytesWritten(2000);
    sock.waitForReadyRead(2000);
    QByteArray resp = sock.readAll();
    sock.write("QUIT\r\n");
    sock.disconnectFromHost();
    bool pong = resp.trimmed().contains("PONG");
    return result(DiagId::G5Redis,
        pong ? "Redis: PONG" : (resp.isEmpty() ? "No response" : QString::fromUtf8(resp).trimmed().left(200)),
        pong ? DiagStatus::Pass : DiagStatus::Warning,
        resp.isEmpty() ? QString() : QString::fromUtf8(resp), t.elapsed());
}

// ── MongoDB (port 27017) — isMaster handshake ─────────────────────────
DiagnosticResult mongodbDiagnostics(const QString& target) {
    if (target.isEmpty())
        return skipped(DiagId::G5Mongodb, "No target");
    QUrl u = G5WebsiteUrl::validate(target);
    if (u.scheme() != "mongodb")
        return skipped(DiagId::G5Mongodb, "Not MongoDB");
    int port = G5WebsiteUrl::portForUrl(u);
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
}

// ── LDAP (port 389) / LDAPS (port 636) — bind request ─────────────────
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
DiagnosticResult mqttDiagnostics(const QString& target) {
    if (target.isEmpty())
        return skipped(DiagId::G5Mqtt, "No target");
    QUrl u = G5WebsiteUrl::validate(target);
    QString scheme = u.scheme().toLower();
    if (scheme != "mqtt" && scheme != "mqtts")
        return skipped(DiagId::G5Mqtt, "Not MQTT(S)");
    int port = G5WebsiteUrl::portForUrl(u);
    QElapsedTimer t; t.start();
    QTcpSocket sock;
    sock.connectToHost(u.host(), port);
    if (!sock.waitForConnected(5000))
        return result(DiagId::G5Mqtt, "Connection failed", DiagStatus::Fail,
                      {}, t.elapsed());
    // MQTT 3.1.1 CONNECT packet:
    // Fixed header: 0x10 [remaining length]
    // Protocol name: "MQTT" (4 bytes) + protocol level 4
    // Connect flags: 0x02 (clean session)
    // Keep alive: 60 seconds
    // No Client ID (zero-length)
    QByteArray connect;
    connect.append('\x10');           // CONNECT packet type
    connect.append('\x10');           // remaining length = 16
    connect.append('\x00'); connect.append('\x04'); // protocol name length = 4
    connect.append("MQTT");           // protocol name
    connect.append('\x04');           // protocol level (3.1.1)
    connect.append('\x02');           // connect flags (clean session)
    connect.append('\x00'); connect.append('\x3c'); // keep alive = 60s
    connect.append('\x00'); connect.append('\x00'); // client ID length = 0
    sock.write(connect);
    sock.waitForBytesWritten(2000);
    sock.waitForReadyRead(3000);
    QByteArray resp = sock.readAll();
    sock.disconnectFromHost();
    if (resp.size() < 2)
        return result(DiagId::G5Mqtt, "No CONNACK", DiagStatus::Warning,
                      {}, t.elapsed());
    // CONNACK: 0x20 [remaining length] [session present] [return code]
    bool isConnack = (static_cast<unsigned char>(resp.at(0)) == 0x20);
    quint8 retCode = resp.size() >= 4 ? static_cast<quint8>(resp.at(3)) : 255;
    static const char* retDesc[] = {
        "Accepted","Protocol version refused","Identifier rejected",
        "Server unavailable","Bad credentials","Not authorized"
    };
    QString desc = (retCode <= 5) ? QString::fromLatin1(retDesc[retCode])
                                  : QString("Unknown code %1").arg(retCode);
    return result(DiagId::G5Mqtt,
        isConnack ? QString("MQTT CONNACK: %1").arg(desc)
                  : "No CONNACK received",
        (isConnack && retCode == 0) ? DiagStatus::Pass : DiagStatus::Warning,
        QString::fromUtf8(resp.toHex(' ')), t.elapsed());
}

} // namespace G5WebsiteUrl
