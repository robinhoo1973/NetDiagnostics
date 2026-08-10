#include "Diagnostics/Model/G5/G5Common.h"
namespace G5WebsiteUrl {
DiagnosticResult mqttDiagnostics(const QString& target) {
    if (target.isEmpty()) return skipped(DiagId::G5Mqtt, "No target");
    QUrl u = validate(target);
    QString scheme = u.scheme().toLower();
    if (scheme != "mqtt" && scheme != "mqtts")
        return skipped(DiagId::G5Mqtt, "Not MQTT(S)");
    // MQTT 3.1.1 CONNECT packet:
    // Fixed header: 0x10 [remaining length]; protocol name "MQTT" level 4;
    // flags 0x02 (clean session); keep alive 60s; zero-length Client ID.
    QByteArray connect;
    connect.append('\x10');
    connect.append('\x10');
    connect.append('\x00'); connect.append('\x04');
    connect.append("MQTT");
    connect.append('\x04');
    connect.append('\x02');
    connect.append('\x00'); connect.append('\x3c');
    connect.append('\x00'); connect.append('\x00');
    // 5WHY: connect/read lifecycle + result scaffold are shared
    // g5Probe()/g5ProbeResult(); only CONNACK parsing stays here.
    auto p = g5Probe(u, connect, 5000, 3000);
    auto r = g5ProbeResult(DiagId::G5Mqtt, u, p);
    if (!p.connected) {
        r.data["isConnack"] = false;
        r.data["resultCode"] = -1;
        r.data["returnDescription"] = QString();
        r.data["accepted"] = false;
        return r;
    }
    QByteArray resp = p.banner;
    if (resp.size() < 2) {
        r.summary = "No CONNACK";
        r.status = DiagStatus::Warning;
        r.data["isConnack"] = false;
        r.data["resultCode"] = -1;
        r.data["returnDescription"] = QString();
        r.data["accepted"] = false;
        return r;
    }
    // CONNACK: 0x20 [remaining length] [session present] [return code]
    bool isConnack = (static_cast<unsigned char>(resp.at(0)) == 0x20);
    quint8 retCode = resp.size() >= 4 ? static_cast<quint8>(resp.at(3)) : 255;
    static const char* retDesc[] = {
        "Accepted","Protocol version refused","Identifier rejected",
        "Server unavailable","Bad credentials","Not authorized"
    };
    QString desc = (retCode <= 5) ? QString::fromLatin1(retDesc[retCode])
                                  : QString("Unknown code %1").arg(retCode);
    r.summary = isConnack ? QString("MQTT CONNACK: %1").arg(desc)
                          : "No CONNACK received";
    r.status = (isConnack && retCode == 0) ? DiagStatus::Pass : DiagStatus::Warning;
    r.rawOutput = r.details = QString::fromUtf8(resp.toHex(' '));
    r.data["isConnack"] = isConnack;
    r.data["resultCode"] = static_cast<int>(retCode);
    r.data["returnDescription"] = desc;
    r.data["accepted"] = (isConnack && retCode == 0);
    return r;
}
} // namespace G5WebsiteUrl
