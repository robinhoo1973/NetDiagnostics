#include "engine/diagnostics/G5/G5Common.h"
#ifndef NO_CURL
namespace G5WebsiteUrl {
DiagnosticResult sslCertificate(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5SslCertificate, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "https")
        return g5Result(DiagId::G5SslCertificate, "Not HTTPS", DiagStatus::Skipped);
    int port = u.port() > 0 ? u.port() : 443;
    auto cert = NetworkProbe::sslCertInfo(u.host(), port, 10000);
    if (!cert.valid)
        return g5Result(DiagId::G5SslCertificate, "Failed to get certificate", DiagStatus::Fail);
    DiagStatus st = DiagStatus::Pass;
    QString summary = QStringLiteral("%1 days left").arg(cert.daysLeft);
    if (cert.daysLeft < 0) { st = DiagStatus::Fail; summary = "EXPIRED"; }
    else if (cert.daysLeft < 30) { st = DiagStatus::Warning; }
    QStringList lines;
    lines.append(QStringLiteral("* SSL connection established"));

    // Build a 2-column table with auto-width
    QStringList names = {
        QStringLiteral("subject"), QStringLiteral("issuer"),
        QStringLiteral("valid from"), QStringLiteral("valid to"),
        QStringLiteral("days left"), QStringLiteral("SAN count"),
        QStringLiteral("thumbprint")};
    QStringList vals = {
        cert.subject, cert.issuer,
        cert.validFrom.toString("yyyy-MM-dd"), cert.validTo.toString("yyyy-MM-dd"),
        QString::number(cert.daysLeft), QString::number(cert.subjectAltNames.size()),
        cert.thumbprint.left(40)};

    int nw = static_cast<int>(QStringLiteral("thumbprint").length());
    for (const auto& s : names) nw = qMax(nw, static_cast<int>(s.length()));
    int vw = 0;
    for (const auto& s : vals) vw = qMax(vw, static_cast<int>(s.length()));

    lines.append(QStringLiteral("*  %1  %2")
        .arg(QStringLiteral("Property"), -nw)
        .arg(QStringLiteral("Value"), -vw));
    lines.append(QStringLiteral("*  %1  %2")
        .arg(QString(nw, '-'))
        .arg(QString(vw, '-')));
    for (int i = 0; i < names.size(); ++i)
        lines.append(QStringLiteral("*  %1  %2")
            .arg(names[i], -nw)
            .arg(vals[i], -vw));
    auto r = g5Result(DiagId::G5SslCertificate, summary, st);
    r.rawOutput = lines.join('\n'); r.details = r.rawOutput;
    r.properties.append(ResultProperty("Subject", cert.subject));
    r.properties.append(ResultProperty("Issuer", cert.issuer));
    return r;
}

// ── G5.8 HTTP Redirect ───────────────────────────────────────────────────
#ifndef NO_CURL
#endif // NO_CURL
} // namespace G5WebsiteUrl
