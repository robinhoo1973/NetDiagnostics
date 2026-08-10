#include "Diagnostics/Model/G5/G5Common.h"
namespace G5WebsiteUrl {
DiagnosticResult telnetDiagnostics(const QString& target) {
    if (target.isEmpty()) return skipped(DiagId::G5Telnet, "No target");
    QUrl u = validate(target);
    if (u.scheme() != "telnet") return skipped(DiagId::G5Telnet, "Not Telnet");
    auto p = g5Probe(u, {}, 5000, 2000);
    auto r = g5ProbeResult(DiagId::G5Telnet, u, p);
    if (!p.connected) return r;
    QString banner = QString::fromUtf8(p.banner).trimmed().left(200);
    r.data["banner"] = banner;
    r.summary = banner.isEmpty() ? QStringLiteral("Connected (no banner)") : banner;
    r.status = banner.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass;
    if (!p.banner.isEmpty()) r.rawOutput = r.details = QString::fromUtf8(p.banner);
    return r;
}
} // namespace G5WebsiteUrl
