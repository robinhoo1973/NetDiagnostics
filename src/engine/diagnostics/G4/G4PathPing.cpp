#include "engine/diagnostics/G4/G4Common.h"
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

    // Parse hop IPs from traceroute rawOutput
    // Lines look like: " %1  %2  %3  %4  %5 [%6]" (Windows tracert format)
    // or: " %1     *        *        *     Request timed out."
    struct HopEntry { int ttl; QString ip; QString name; int rttMs; bool reached; };
    QVector<HopEntry> hops;
    // Hop 0 = local (not in traceroute output)
    hops.append({0, QString(), QStringLiteral("localhost"), 0, false});

    int hopCount = 0; bool reached = false;
    for (const QString& line : tr.rawOutput.split('\n')) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith("Tracing") || trimmed.startsWith("over") ||
            trimmed.startsWith("Trace") || trimmed.startsWith("...") || trimmed.startsWith("Target")) continue;
        // Match "[IP]" at end of non-timeout lines
        auto rb = line.indexOf('[');
        if (rb > 0) {
            auto re = line.indexOf(']', rb);
            if (re > rb) {
                QString hopIp = line.mid(rb + 1, re - rb - 1);
                // TTL is the first number on the line
                QString ttlStr = line.mid(1, 2).trimmed(); // " %1..." → chars 1-2
                int ttlNum = ttlStr.toInt();
                // RTT from first ms field
                auto msPos = line.indexOf("ms");
                auto msStart = msPos > 0 ? line.lastIndexOf(' ', msPos - 1) + 1 : 0;
                int rttVal = msPos > 0 ? line.mid(msStart, msPos - msStart).trimmed().toInt() : 0;
                bool isTarget = (hopIp == targetIpStr);
                if (isTarget) reached = true;
                hops.append({ttlNum, hopIp, QString(), rttVal, isTarget});
                hopCount = ttlNum;
            }
        }
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
        for (const QString& pline : pr.rawOutput.split('\n')) {
            if (pline.contains(QStringLiteral("Packets:"))) {
                auto si = pline.indexOf(QStringLiteral("Sent = "));
                auto ri = pline.indexOf(QStringLiteral("Received = "));
                auto pi = pline.indexOf('(');
                if (si > 0 && ri > 0) {
                    hs.sent = pline.mid(si + 7, pline.indexOf(',', si) - si - 7).trimmed().toInt();
                    hs.rcvd = pline.mid(ri + 11, pline.indexOf(',', ri) - ri - 11).trimmed().toInt();
                }
                if (pi > 0) {
                    auto pe = pline.indexOf('%', pi);
                    if (pe > pi) hs.loss = pline.mid(pi + 1, pe - pi - 1).toDouble();
                }
            }
            if (pline.contains(QStringLiteral("Average = "))) {
                auto ai = pline.indexOf(QStringLiteral("Average = "));
                auto ae = pline.indexOf(QStringLiteral("ms"), ai);
                if (ai > 0 && ae > ai) hs.avgMs = pline.mid(ai + 10, ae - ai - 10).trimmed().toInt();
            }
        }
    }

    // ── Build output — strict Windows pathping.exe format ──────────────
    QStringList lines;
    lines.append(QString());
    lines.append(QStringLiteral("Tracing route to %1 [%2]").arg(host, targetIpStr));
    lines.append(QStringLiteral("over a maximum of 30 hops:"));
    lines.append(QString());

    // Phase 1 output: route table
    for (const auto& hop : hops) {
        if (hop.ttl == 0)
            lines.append(QStringLiteral("  %1  %2").arg(hop.ttl).arg(hop.name));
        else if (!hop.ip.isEmpty())
            lines.append(QStringLiteral("  %1  %2 [%3]").arg(hop.ttl).arg(hop.ip).arg(hop.ip));
        else
            lines.append(QStringLiteral("  %1  (unreachable)").arg(hop.ttl));
    }

    lines.append(QString());
    int statsSec = (int)totalTimer.elapsed() / 1000;
    if (statsSec < 1) statsSec = 1;
    lines.append(QStringLiteral("Computing statistics for %1 seconds...").arg(statsSec));
    lines.append(QStringLiteral("  %1%2%3%4%5")
        .arg(QString(4, ' '))  // indent + TTL
        .arg(QString(8, ' '))  // RTT
        .arg(QStringLiteral("Source to Here"), -16, ' ')
        .arg(QStringLiteral("This Node/Link"), -16, ' ')
        .arg(QStringLiteral("Address")));
    lines.append(QStringLiteral("  %1  %2   %3   %4  %5")
        .arg(QStringLiteral("Hop"), 2)
        .arg(QStringLiteral("RTT"), 5)
        .arg(QStringLiteral("Lost/Sent = Pct"), -16)
        .arg(QStringLiteral("Lost/Sent = Pct"), -16)
        .arg(QStringLiteral("Address")));
    lines.append(QStringLiteral("  %1  %2   %3   %4  %5")
        .arg(QString(2, '-'))
        .arg(QString(5, '-'))
        .arg(QString(16, '-'))
        .arg(QString(16, '-'))
        .arg(QString(7, '-')));

    // Phase 2 output: per-hop statistics
    for (int i = 0; i < hops.size(); ++i) {
        const auto& hop = hops[i];
        QString addr = hop.ttl == 0
            ? hop.name
            : (hop.ip.isEmpty() ? QStringLiteral("(unreachable)") : hop.ip);

        if (i == 0) {
            // Hop 0 — only address, stats are on inter-hop lines
            lines.append(QStringLiteral("  %1                                         %2")
                .arg(hop.ttl).arg(addr));
        } else {
            HopStats& hs = hopStats[i - 1];
            QString rttField = hop.reached
                ? QStringLiteral("%1ms").arg(hs.avgMs, 4)
                : QStringLiteral("  N/A");
            QString srcLoss = hop.reached
                ? QStringLiteral("  %1/%2 = %3%").arg(hs.sent - hs.rcvd, 2).arg(hs.sent, 2).arg((int)hs.loss, 2)
                : QStringLiteral("   N/A");
            // This-node/link — for target hop, same as source-to-here; for intermediate, show as source loss
            QString nodeLoss = hop.reached
                ? QStringLiteral("  %1/%2 = %3%").arg(hs.sent - hs.rcvd, 2).arg(hs.sent, 2).arg((int)hs.loss, 2)
                : QStringLiteral("   N/A");
            lines.append(QStringLiteral("  %1  %2   %3   %4  %5")
                .arg(hop.ttl, 2).arg(rttField).arg(srcLoss).arg(nodeLoss).arg(addr));
        }

        // Inter-hop link line (the "|" line in Windows pathping)
        if (i < hops.size() - 1) {
            const auto& nextHop = hops[i + 1];
            int nextIdx = i; // index into hopStats for the next hop (hopStats[0] = hop 1)
            QString linkLoss;
            // Only show real stats for the target hop; intermediate routers
            // are not pinged (TCP doesn't reach them), so show N/A.
            if (nextIdx < hopStats.size() && !nextHop.ip.isEmpty() && nextHop.reached) {
                auto& nhs = hopStats[nextIdx];
                linkLoss = QStringLiteral("  %1/%2 = %3%")
                    .arg(nhs.sent - nhs.rcvd, 2).arg(nhs.sent, 2).arg((int)nhs.loss, 2);
            } else {
                linkLoss = QStringLiteral("   N/A");
            }
            lines.append(QStringLiteral("                               %1   |").arg(linkLoss));
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
