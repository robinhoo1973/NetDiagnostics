// =============================================================================
// DiagnosticFormatter.cpp — Shared output formatting (extracted from G1G2G3Native)
// =============================================================================
// UX alignment principles:
//   • Numeric data → RIGHT-aligned (easy visual magnitude comparison)
//   • Text data   → LEFT-aligned  (natural reading flow)
//   • Headers     → match the alignment of their column's data
//   • Content longer than column width → truncated with "…" suffix
//   • CJK / emoji characters counted as 2 display columns
// =============================================================================
#include "engine/diagnostics/DiagnosticFormatter.h"
#include <QDateTime>

static const QString kTblGap = QStringLiteral("  ");

// ── Display-width helper: CJK / fullwidth / emoji = 2, ASCII = 1 ──
static int displayWidth(const QString& s) {
    int w = 0;
    for (const QChar& ch : s) {
        ushort uc = ch.unicode();
        // Hangul Jamo, CJK Radicals–Yi, Hangul Syllables, CJK Compat,
        // Fullwidth Forms, Emoji, CJK Ext B–G
        if ((uc >= 0x1100 && uc <= 0x115F)
            || (uc >= 0x2329 && uc <= 0x232A)
            || (uc >= 0x2E80 && uc <= 0xA4CF)
            || (uc >= 0xAC00 && uc <= 0xD7A3)
            || (uc >= 0xF900 && uc <= 0xFAFF)
            || (uc >= 0xFE10 && uc <= 0xFE19)
            || (uc >= 0xFE30 && uc <= 0xFE6F)
            || (uc >= 0xFF00 && uc <= 0xFF60)
            || (uc >= 0xFFE0 && uc <= 0xFFE6)
            || (uc >= 0x1F300 && uc <= 0x1F64F)
            || (uc >= 0x1F900 && uc <= 0x1F9FF))
            w += 2;
        else
            w += 1;
    }
    return w;
}

// ── Trim a value to fit within display-width budget ──
// Returns the original string if it fits; otherwise truncates and appends "…"
static QString trimToWidth(const QString& val, int maxDisplayWidth) {
    if (displayWidth(val) <= maxDisplayWidth)
        return val;
    // Walk until we exceed budget, then add ellipsis
    int dw = 0;
    for (int i = 0; i < val.length(); ++i) {
        int cdw = displayWidth(val.mid(i, 1));
        if (dw + cdw + 1 > maxDisplayWidth) // +1 for "…"
            return val.left(i) + QStringLiteral("…"); // …
        dw += cdw;
    }
    return val;
}

// ── Pad a string to target display width ──
// leftPad=false → left-justify (append spaces); leftPad=true → right-justify (prepend spaces)
static QString padToWidth(const QString& val, int targetDisplayWidth, bool rightAlign) {
    int dw = displayWidth(val);
    int padNeeded = qMax(0, targetDisplayWidth - dw);
    if (rightAlign)
        return QString(padNeeded, ' ') + val;
    else
        return val + QString(padNeeded, ' ');
}

// ── Aligned-column table ───────────────────────────────────────────
QStringList DiagnosticFormatter::formatTable(const QVector<ColSpec>& cols,
                                               const QList<QStringList>& rows) {
    QStringList out;
    QVector<int> w(cols.size());
    for (int i = 0; i < cols.size(); ++i) {
        w[i] = qMax(cols[i].minWidth, displayWidth(QString::fromLatin1(cols[i].header)));
        for (const auto& row : rows)
            if (i < row.size()) w[i] = qMax(w[i], displayWidth(row[i]));
    }
    // Header row: pad to display width, matching column alignment
    QStringList hdrParts;
    for (int i = 0; i < cols.size(); ++i)
        hdrParts.append(padToWidth(QString::fromLatin1(cols[i].header), w[i], cols[i].rightAlign));
    out.append(hdrParts.join(kTblGap));
    // Separator (dashes matching display width)
    QStringList sepParts;
    for (int i = 0; i < cols.size(); ++i)
        sepParts.append(QString(w[i], '-'));
    out.append(sepParts.join(kTblGap));
    // Data rows — trim to fit, pad to display width
    for (const auto& row : rows) {
        QStringList parts;
        for (int i = 0; i < cols.size(); ++i) {
            QString val = (i < row.size()) ? trimToWidth(row[i], w[i]) : QString();
            parts.append(padToWidth(val, w[i], cols[i].rightAlign));
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

