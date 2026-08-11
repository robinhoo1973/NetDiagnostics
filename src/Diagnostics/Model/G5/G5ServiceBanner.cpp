#include "Diagnostics/Model/G5/G5Common.h"
namespace G5WebsiteUrl {
DiagnosticResult serviceBanner(const QString& target) {
    if (target.isEmpty()) return skipped(DiagId::G5ServiceBanner, "No target");
    QUrl u = validate(target);
    if (!u.isValid() || u.host().isEmpty())
        return g5Result(DiagId::G5ServiceBanner, "Invalid target", DiagStatus::Fail);
    // 5WHY: connect+read lifecycle + result scaffold are shared g5Probe()/
    // g5ProbeResult().  sendData is empty — we want the server's greeting
    // banner, not a protocol handshake.
    auto p = g5Probe(u, {}, 5000, 2000);
    auto r = g5ProbeResult(DiagId::G5ServiceBanner, u, p);
    if (!p.connected) return r;
    QString banner = QString::fromUtf8(p.banner).left(500);
    r.summary = p.banner.isEmpty() ? QStringLiteral("No banner received")
                                   : QStringLiteral("Banner received");
    r.status = p.banner.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass;
    r.rawOutput = banner;
    r.details = banner;  // truncate terminal output to 500 chars for display (g5ProbeResult sets full banner)
    r.data["banner"] = banner;
    r.data["bannerLength"] = p.banner.size();
    return r;
}
} // namespace G5WebsiteUrl
