#include "Diagnostics/Model/G4/G4Common.h"
DiagnosticResult pathPing(const QString& target) {
    DiagnosticResult r;
    r.id = DiagId::G4PathPing; r.group = DiagGroup::G4;
    r.timestamp = QDateTime::currentDateTime();
    if (target.isEmpty()) return noTargetResult(r.id, r.group);
    QString host = extractHostname(target);
    quint32 targetIp = resolveIPv4(host);
    if (!targetIp) { r.status = DiagStatus::Fail; r.summary = QStringLiteral("DNS resolution failed"); return r; }
    struct in_addr a; a.s_addr = htonl(targetIp);
    QString targetIpStr = ip4ToStr(a);

    QElapsedTimer totalTimer; totalTimer.start();

    // ── Phase 1: Traceroute ───────────────────────────────────────────
    // Windows pathping format:
    // "Tracing route to example.com [93.184.216.34]"
    // "over a maximum of 30 hops:"
    // "  0  mypc.example.com [192.168.1.100]"
    // "  1  192.168.1.1"
    // ...
    auto tr = traceroute(target);

    // Parse hop data from traceroute output.
    // Our traceroute format: " %1  %2  %3  %4  %5 [%6]"
    //   → "  N   RTT   RTT   RTT   hostname [IP]"
    // We use a targeted regex to extract TTL (first number) and IP (in [brackets])
    // rather than PingParser::parseTraceroute() which was designed for external
    // Windows tracert.exe output that lacks per-hop hostname resolution.
    struct HopEntry { int ttl; QString ip; QString name; bool reached; };
    QVector<HopEntry> hops;
    hops.append({0, QString(), QStringLiteral("localhost"), false});

    static const QRegularExpression hopRe(
        R"(^\s*(\d+)\s+.*\[([\d.]+)\])",
        QRegularExpression::MultilineOption
    );
    int hopCount = 0; bool reached = false;
    auto hopMatches = hopRe.globalMatch(tr.rawOutput);
    while (hopMatches.hasNext()) {
        auto m = hopMatches.next();
        int ttlNum = m.captured(1).toInt();
        QString hopIp = m.captured(2);
        bool isTarget = (hopIp == targetIpStr);
        if (isTarget) reached = true;
        hops.append({ttlNum, hopIp, QString(), isTarget});
        hopCount = ttlNum;
    }

    // ── Phase 2: Ping each hop for per-hop statistics ──────────────────
    // ping() uses TCP connect (ports 443,80,22,8080,8443) to measure RTT.
    // Intermediate routers do NOT run TCP servers — pinging them wastes
    // ~60 s per hop (5 ports × 4 iterations × 3 s timeout). We only ping
    // the FINAL hop (target), which typically does run a TCP service.
    // A future improvement would implement ICMP Echo (raw socket) for
    // full per-hop statistics matching Windows pathping.exe behaviour.
    struct HopStats { int sent; int rcvd; double loss; int avgMs; };
    QVector<HopStats> hopStats;
    // Pre-fill with N/A for all intermediate hops
    for (int i = 1; i < hops.size(); ++i)
        hopStats.append({4, 0, 100.0, 0});
    // Only ping the final (target) hop
    if (hops.size() > 1 && !hops.last().ip.isEmpty()) {
        int lastIdx = hopStats.size() - 1;
        auto& hs = hopStats[lastIdx];
        auto pr = ping(hops.last().ip);
        // Use PingParser for robust, locale-independent parsing instead of
        // fragile indexOf()/mid() string scraping.
        PingResult parsed = PingParser::parse(pr.rawOutput);
        if (parsed.valid) {
            hs.sent  = parsed.sent;
            hs.rcvd  = parsed.received;
            hs.loss  = parsed.lossPercent;
            hs.avgMs = static_cast<int>(parsed.avgMs);
        }
    }

    // ── Build output — strict Windows pathping.exe format ──────────────
    QStringList lines;
    lines.append(QString());
    lines.append(QStringLiteral("Tracing route to %1 [%2]").arg(host, targetIpStr));
    lines.append(QStringLiteral("over a maximum of 30 hops:"));
    lines.append(QString());

    // Phase 1 output: route table — use resolved names from traceroute
    for (const auto& hop : hops) {
        if (hop.ttl == 0)
            lines.append(QStringLiteral("  %1  %2").arg(hop.ttl).arg(hop.name));
        else if (!hop.ip.isEmpty())
            lines.append(QStringLiteral("  %1  %2").arg(hop.ttl).arg(hop.ip));
        else
            lines.append(QStringLiteral("  %1  (unreachable)").arg(hop.ttl));
    }

    lines.append(QString());
    int statsSec = (int)totalTimer.elapsed() / 1000;
    if (statsSec < 1) statsSec = 1;
    lines.append(QStringLiteral("Computing statistics for %1 seconds...").arg(statsSec));

    // ── Build statistics table with interleaved link lines ────────────
    // Columns: Hop | RTT | Source to Here | This Node/Link | Address
    // Compute column widths from headers and all data rows.
    QStringList headers = {
        QStringLiteral("Hop"), QStringLiteral("RTT"),
        QStringLiteral("Source to Here"), QStringLiteral("This Node/Link"),
        QStringLiteral("Address")
    };
    QVector<int> colW = {3, 5, 16, 16, 7};  // min widths
    for (int c = 0; c < 5; ++c)
        colW[c] = qMax(colW[c], headers[c].length());

    // Build data rows and track max column widths
    struct TableRow { QStringList cells; int hopIdx; };
    QVector<TableRow> rows;
    for (int i = 0; i < hops.size(); ++i) {
        const auto& hop = hops[i];
        QString addr = hop.ttl == 0
            ? hop.name
            : (hop.ip.isEmpty() ? QStringLiteral("(unreachable)") : hop.ip);
        QStringList cells;
        if (i == 0) {
            cells = {QString::number(hop.ttl), QString(), QString(), QString(), addr};
        } else {
            HopStats& hs = hopStats[i - 1];
            cells = {
                QString::number(hop.ttl),
                hop.reached ? QStringLiteral("%1ms").arg(hs.avgMs) : QStringLiteral("N/A"),
                hop.reached ? QStringLiteral("%1/%2 = %3%").arg(hs.sent - hs.rcvd).arg(hs.sent).arg((int)hs.loss) : QStringLiteral("N/A"),
                hop.reached ? QStringLiteral("%1/%2 = %3%").arg(hs.sent - hs.rcvd).arg(hs.sent).arg((int)hs.loss) : QStringLiteral("N/A"),
                addr
            };
        }
        rows.append({cells, i});
        for (int c = 0; c < 5; ++c)
            colW[c] = qMax(colW[c], cells[c].length());
    }

    // Helper: format a cell with alignment
    auto fmtCell = [&](const QString& val, int c) -> QString {
        int pad = colW[c] - val.length();
        bool right = (c == 0 || c == 1);  // Hop + RTT right-aligned
        return right ? QString(pad, ' ') + val : val + QString(pad, ' ');
    };

    // Emit sub-header (Source to Here / This Node/Link)
    lines.append(QStringLiteral("  %1%2%3%4  %5")
        .arg(fmtCell(QString(), 0)).arg(fmtCell(QString(), 1))
        .arg(fmtCell(QStringLiteral("Source to Here"), 2))
        .arg(fmtCell(QStringLiteral("This Node/Link"), 3))
        .arg(fmtCell(QString(), 4)));
    // Emit main header
    lines.append(QStringLiteral("  %1  %2  %3  %4  %5")
        .arg(fmtCell(headers[0], 0)).arg(fmtCell(headers[1], 1))
        .arg(fmtCell(headers[2], 2)).arg(fmtCell(headers[3], 3))
        .arg(fmtCell(headers[4], 4)));
    // Separator
    lines.append(QStringLiteral("  %1  %2  %3  %4  %5")
        .arg(fmtCell(QString(colW[0], '-'), 0))
        .arg(fmtCell(QString(colW[1], '-'), 1))
        .arg(fmtCell(QString(colW[2], '-'), 2))
        .arg(fmtCell(QString(colW[3], '-'), 3))
        .arg(fmtCell(QString(colW[4], '-'), 4)));

    // Emit data rows with interleaved link lines (Windows pathping format)
    for (int ri = 0; ri < rows.size(); ++ri) {
        const auto& row = rows[ri];
        lines.append(QStringLiteral("  %1  %2  %3  %4  %5")
            .arg(fmtCell(row.cells[0], 0)).arg(fmtCell(row.cells[1], 1))
            .arg(fmtCell(row.cells[2], 2)).arg(fmtCell(row.cells[3], 3))
            .arg(fmtCell(row.cells[4], 4)));

        // Inter-hop link line (between this hop and the next)
        int i = row.hopIdx;
        if (i < hops.size() - 1) {
            const auto& nextHop = hops[i + 1];
            int nextIdx = i;
            QString linkLoss;
            if (nextIdx < hopStats.size() && !nextHop.ip.isEmpty() && nextHop.reached) {
                auto& nhs = hopStats[nextIdx];
                linkLoss = QStringLiteral("%1/%2 = %3%")
                    .arg(nhs.sent - nhs.rcvd).arg(nhs.sent).arg((int)nhs.loss);
            } else {
                linkLoss = QStringLiteral("N/A");
            }
            // Position the | under the "This Node/Link" column
            QString linkIndent = fmtCell(QString(), 0) + QStringLiteral("  ")
                               + fmtCell(QString(), 1) + QStringLiteral("  ")
                               + fmtCell(QString(), 2) + QStringLiteral("  ");
            lines.append(QStringLiteral("  %1%2   |").arg(linkIndent).arg(linkLoss));
        }
    }

    lines.append(QString());
    lines.append(reached ? QStringLiteral("Trace complete.") : QStringLiteral("Trace incomplete."));
    if (!s_rawIcmpAvailable) {
        lines.append(QString());
        lines.append(QStringLiteral("NOTE: Raw ICMP sockets unavailable (requires CAP_NET_RAW)."));
        lines.append(QStringLiteral("  Per-hop statistics may be incomplete."));
        lines.append(QStringLiteral("  To enable full pathPing, run:  sudo setcap cap_net_raw=ep <binary>"));
    }
    r.rawOutput = lines.join('\n');
    r.details   = lines.join('\n');
    r.durationMs = totalTimer.elapsed();

    if (reached) { r.status = DiagStatus::Pass; r.summary = QStringLiteral("%1 hops, target reached").arg(hopCount); }
    else if (hopCount > 0) { r.status = DiagStatus::Warning; r.summary = QStringLiteral("Partial: %1 hops").arg(hopCount); }
    else { r.status = DiagStatus::Fail; r.summary = QStringLiteral("Target unreachable"); }
    return r;
}

// ── MTU Discovery (custom ping, no system command) ────────────────────
}
