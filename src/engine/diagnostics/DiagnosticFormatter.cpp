// =============================================================================
// DiagnosticFormatter.cpp — Shared output formatting (extracted from G1G2G3Native)
// =============================================================================
#include "engine/diagnostics/DiagnosticFormatter.h"
#include <QDateTime>

static const QString kTblGap = QStringLiteral("  ");

// ── Aligned-column table (extracted from G1G2G3Native::tblFmt) ──────
QStringList DiagnosticFormatter::formatTable(const QVector<ColSpec>& cols,
                                               const QList<QStringList>& rows) {
    QStringList out;
    QVector<int> w(cols.size());
    for (int i = 0; i < cols.size(); ++i) {
        w[i] = qMax(cols[i].minWidth, (int)strlen(cols[i].header));
        for (const auto& row : rows)
            if (i < row.size()) w[i] = qMax(w[i], row[i].length());
    }
    // Header
    QStringList hdrParts;
    for (int i = 0; i < cols.size(); ++i)
        hdrParts.append(cols[i].rightAlign
            ? QString::fromLatin1(cols[i].header).rightJustified(w[i], ' ')
            : QString::fromLatin1(cols[i].header).leftJustified(w[i], ' '));
    out.append(hdrParts.join(kTblGap));
    // Separator
    QStringList sepParts;
    for (int i = 0; i < cols.size(); ++i)
        sepParts.append(QString(w[i], '-'));
    out.append(sepParts.join(kTblGap));
    // Data rows
    for (const auto& row : rows) {
        QStringList parts;
        for (int i = 0; i < cols.size(); ++i) {
            QString val = (i < row.size()) ? row[i] : QString();
            parts.append(cols[i].rightAlign
                ? val.rightJustified(w[i], ' ')
                : val.leftJustified(w[i], ' '));
        }
        out.append(parts.join(kTblGap));
    }
    return out;
}

// ── dig-style DNS ─────────────────────────────────────────────────
QStringList DiagnosticFormatter::formatDnsHeader(const QString& host,
                                                   const QString& rcode,
                                                   uint16_t id, int anCount) {
    return {
        QString(),
        QStringLiteral("; <<>> NetDiagnostics DNS <<>> %1").arg(host),
        QStringLiteral(";; global options: +cmd"),
        QStringLiteral(";; Got answer:"),
        QStringLiteral(";; ->>HEADER<<- opcode: QUERY, status: %1, id: %2")
            .arg(rcode).arg(id),
        QStringLiteral(";; flags: qr rd ra; QUERY: 1, ANSWER: %1, AUTHORITY: 0, ADDITIONAL: 0")
            .arg(anCount),
        QString(),
    };
}

QString DiagnosticFormatter::formatDnsQuestion(const QString& host, const QString& type) {
    return QStringLiteral(";%1.\t\t\tIN\t%2").arg(host, type);
}

QString DiagnosticFormatter::formatDnsRecord(const QString& owner, int ttl,
                                               const QString& type, const QString& value) {
    return QStringLiteral("%1.\t%2\tIN\t%3\t%4")
        .arg(owner, -30).arg(ttl, 6).arg(type).arg(value);
}

QStringList DiagnosticFormatter::formatDnsFooter(qint64 elapsedMs, const QString& server) {
    return {
        QStringLiteral(";; Query time: %1 msec").arg(elapsedMs),
        QStringLiteral(";; SERVER: %1").arg(server),
        QStringLiteral(";; WHEN: %1").arg(
            QDateTime::currentDateTime().toString(QStringLiteral("ddd MMM d HH:mm:ss yyyy"))),
    };
}

// ── Windows ping.exe ──────────────────────────────────────────────
QString DiagnosticFormatter::formatPingReply(const QString& ip, int ms,
                                               int bytes, int ttl) {
    return QStringLiteral("Reply from %1: bytes=%2 time=%3ms TTL=%4")
        .arg(ip).arg(bytes).arg(ms).arg(ttl);
}

QString DiagnosticFormatter::formatPingTimeout() {
    return QStringLiteral("Request timed out.");
}

QStringList DiagnosticFormatter::formatPingStats(const QString& target,
                                                   int sent, int received,
                                                   double lossPct, int minMs,
                                                   int maxMs, double avgMs) {
    QStringList lines;
    lines.append(QString());
    lines.append(QStringLiteral("Ping statistics for %1:").arg(target));
    lines.append(QStringLiteral("    Packets: Sent = %1, Received = %2, Lost = %3 (%4% loss),")
        .arg(sent).arg(received).arg(sent - received).arg(lossPct, 0, 'f', 1));
    if (received > 0) {
        lines.append(QStringLiteral("Approximate round trip times in milli-seconds:"));
        lines.append(QStringLiteral("    Minimum = %1ms, Maximum = %2ms, Average = %3ms")
            .arg(minMs).arg(maxMs).arg(avgMs, 0, 'f', 1));
    }
    return lines;
}

// ── Windows tracert.exe ───────────────────────────────────────────
QString DiagnosticFormatter::formatTracerouteHop(int ttl, int rtt1, int rtt2, int rtt3,
                                                   const QString& name, const QString& ip) {
    auto rttStr = [](int ms) -> QString {
        if (ms < 1) return QStringLiteral("  <1 ms");
        return QStringLiteral("  %1 ms").arg(ms, 3);
    };
    QString display = ip.isEmpty() ? name : QStringLiteral("%1 [%2]").arg(name, ip);
    return QStringLiteral(" %1  %2  %3  %4  %5")
        .arg(ttl, 2).arg(rttStr(rtt1)).arg(rttStr(rtt2)).arg(rttStr(rtt3)).arg(display);
}

