#include "Diagnostics/Model/G5/G5Common.h"
namespace G5WebsiteUrl {
DiagnosticResult ftpDiagnostics(const QString& target) {
    if (target.isEmpty()) return skipped(DiagId::G5FtpDiagnostics, "No target");
    QUrl u = validate(target);
    if (u.scheme() != "ftp" && u.scheme() != "ftps")
        return skipped(DiagId::G5FtpDiagnostics, "Not FTP");
    // 5WHY: connect+read lifecycle and the host/port/connected/latencyMs
    // scaffold are shared g5Probe()/g5ProbeResult() — was ~20 lines of raw
    // QTcpSocket + result boilerplate duplicated across 6 G5 protocol tests.
    auto p = g5Probe(u);
    auto r = g5ProbeResult(DiagId::G5FtpDiagnostics, u, p);
    if (!p.connected) return r;
    QString banner = QString::fromUtf8(p.banner).trimmed().left(200);
    r.data["banner"] = banner;
    r.summary = banner.isEmpty() ? QStringLiteral("No banner") : banner;
    r.status = banner.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass;
    return r;
}
} // namespace G5WebsiteUrl
