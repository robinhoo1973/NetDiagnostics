#include "Diagnostics/Model/G5/G5Common.h"
DiagnosticResult mysqlDiagnostics(const QString& target) {
    if (target.isEmpty())
        return skipped(DiagId::G5Mysql, "No target");
    QUrl u = validate(target);
    if (u.scheme() != "mysql")
        return skipped(DiagId::G5Mysql, "Not MySQL");
    int port = portForUrl(u);
    auto probe = NetworkProbe::tcpProbe(u.host(), port, 5000, 2000);
    if (!probe.connected)
        return result(DiagId::G5Mysql, "Connection failed", DiagStatus::Fail,
                      {}, probe.elapsedMs);
    QByteArray data = probe.data;
    if (data.size() < 5)
        return result(DiagId::G5Mysql, "No handshake packet", DiagStatus::Warning,
                      {}, probe.elapsedMs);
    // MySQL handshake: [4-byte length][1-byte seq][1-byte protocol][null-term version][4-byte threadid]...
    int verStart = 5; // skip length(3)+seq(1)+protocol(1)
    int verEnd = data.indexOf('\0', verStart);
    QString version = (verEnd > verStart)
        ? QString::fromUtf8(data.mid(verStart, verEnd - verStart))
        : QString();
    return result(DiagId::G5Mysql,
        version.isEmpty() ? "MySQL (version unknown)" : QString("MySQL %1").arg(version),
        version.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass,
        QString::fromUtf8(data.toHex(' ')), probe.elapsedMs);
}

// ── PostgreSQL (port 5432) — read StartupMessage response ─────────────
} // namespace G5WebsiteUrl
