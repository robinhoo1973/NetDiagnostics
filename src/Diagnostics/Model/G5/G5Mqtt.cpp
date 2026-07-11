#include "Diagnostics/Model/G5/G5Common.h"
DiagnosticResult mqttDiagnostics(const QString& target) {
    if (target.isEmpty())
        return skipped(DiagId::G5Mqtt, "No target");
    QUrl u = validate(target);
    QString scheme = u.scheme().toLower();
    if (scheme != "mqtt" && scheme != "mqtts")
        return skipped(DiagId::G5Mqtt, "Not MQTT(S)");
    int port = portForUrl(u);
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
