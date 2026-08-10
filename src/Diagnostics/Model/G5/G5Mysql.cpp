#include "Diagnostics/Model/G5/G5Common.h"
namespace G5WebsiteUrl {
DiagnosticResult mysqlDiagnostics(const QString& target) {
    if (target.isEmpty()) return skipped(DiagId::G5Mysql, "No target");
    QUrl u = validate(target);
    if (u.scheme() != "mysql") return skipped(DiagId::G5Mysql, "Not MySQL");
    // 5WHY: connect+read lifecycle and result scaffold are shared
    // g5Probe()/g5ProbeResult() — only handshake parsing stays here.
    auto p = g5Probe(u, {}, 5000, 2000);
    auto r = g5ProbeResult(DiagId::G5Mysql, u, p);
    if (!p.connected) {
        r.data["version"] = QString();
        r.data["protocolVersion"] = 0;
        return r;
    }
    QByteArray data = p.banner;
    if (data.size() < 5) {
        r.summary = "No handshake packet";
        r.status = DiagStatus::Warning;
        r.data["version"] = QString();
        r.data["protocolVersion"] = 0;
        return r;
    }
    // MySQL handshake: [4-byte length][1-byte seq][1-byte protocol][null-term version][4-byte threadid]...
    int verStart = 5; // skip length(3)+seq(1)+protocol(1)
    int verEnd = data.indexOf('\0', verStart);
    QString version = (verEnd > verStart)
        ? QString::fromUtf8(data.mid(verStart, verEnd - verStart)) : QString();
    r.summary = version.isEmpty() ? "MySQL (version unknown)"
                                  : QString("MySQL %1").arg(version);
    r.status = version.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass;
    r.rawOutput = r.details = QString::fromUtf8(data.toHex(' '));
    r.data["version"] = version;
    r.data["protocolVersion"] = static_cast<int>(static_cast<unsigned char>(data.at(4)));
    return r;
}
} // namespace G5WebsiteUrl
