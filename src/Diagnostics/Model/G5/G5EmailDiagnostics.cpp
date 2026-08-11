#include "Diagnostics/Model/G5/G5Common.h"
namespace G5WebsiteUrl {
DiagnosticResult emailDiagnostics(const QString& target) {
    if (target.isEmpty()) return skipped(DiagId::G5EmailDiagnostics, "No target");
    QUrl u = validate(target);
    QString scheme = u.scheme();
    if (scheme != "smtp" && scheme != "imap" && scheme != "pop3"
        && scheme != "smtps" && scheme != "imaps" && scheme != "pop3s")
        return skipped(DiagId::G5EmailDiagnostics,
                       "Not email protocol (smtp/smtps/imap/imaps/pop3/pop3s)");
    auto p = g5Probe(u);
    auto r = g5ProbeResult(DiagId::G5EmailDiagnostics, u, p);
    r.data["protocol"] = scheme;  // present on both success and failure
    if (!p.connected) return r;
    QString banner = QString::fromUtf8(p.banner).trimmed().left(200);
    r.data["banner"] = banner;
    r.summary = banner.isEmpty() ? QStringLiteral("No banner") : banner;
    r.status = banner.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass;
    if (!p.banner.isEmpty())
        r.rawOutput = r.details = QString::fromUtf8(p.banner);
    autoErrorOutput(r);
    return r;
}
} // namespace G5WebsiteUrl
