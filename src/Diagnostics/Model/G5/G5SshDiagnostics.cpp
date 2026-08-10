#include "Diagnostics/Model/G5/G5Common.h"
namespace G5WebsiteUrl {
DiagnosticResult sshDiagnostics(const QString& target) {
    if (target.isEmpty()) return skipped(DiagId::G5SshDiagnostics, "No target");
    QUrl u = validate(target);
    if (u.scheme() != "ssh" && u.scheme() != "sftp")
        return skipped(DiagId::G5SshDiagnostics, "Not SSH");
    auto p = g5Probe(u);
    auto r = g5ProbeResult(DiagId::G5SshDiagnostics, u, p);
    if (!p.connected) return r;
    QString bstr = QString::fromUtf8(p.banner).trimmed().left(200);
    QString version = bstr.startsWith("SSH-") ? bstr.section(' ', 0, 0) : QString();
    r.data["sshVersion"] = version;
    r.data["banner"] = bstr;
    r.summary = version.isEmpty() ? QStringLiteral("No SSH banner") : version;
    r.status = version.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass;
    return r;
}
} // namespace G5WebsiteUrl
