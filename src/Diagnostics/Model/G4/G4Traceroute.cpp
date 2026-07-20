#include "Diagnostics/Model/G4/G4Common.h"
DiagnosticResult traceroute(const QString& target) {
    DiagnosticResult r;
    r.id = DiagId::G4Traceroute; r.group = DiagGroup::G4;
    r.timestamp = QDateTime::currentDateTime();
    if (target.isEmpty()) return noTargetResult(r.id, r.group);
    QString host = extractHostname(target);
    quint32 targetIp = resolveIPv4(host);
    if (!targetIp) { r.status=DiagStatus::Fail; r.summary=QStringLiteral("DNS resolution failed"); return r; }

    struct in_addr ta; ta.s_addr = htonl(targetIp);
    QString targetIpStr = ip4ToStr(ta);

    // Build output 鈥?strict Windows tracert.exe format
    // "Tracing route to example.com [93.184.216.34]"
    // "over a maximum of 30 hops:"
    // ""
    // "  1    <1 ms    <1 ms    <1 ms  192.168.1.1"
    // "  2     5 ms     5 ms     5 ms  10.0.0.1"
    QStringList lines;
    lines.append(QString());
    lines.append(QStringLiteral("Tracing route to %1 [%2]").arg(host, targetIpStr));
    lines.append(QStringLiteral("over a maximum of 30 hops:"));
    lines.append(QString());

    // TCP TTL probing via tcpTraceHop() 鈥?uses IP_RECVERR on Linux to capture
    // ICMP Time Exceeded from intermediate routers without root privileges.
    TRACE(" traceroute: using tcpTraceHop\n");

    QElapsedTimer t; t.start();
    int hopCount = 0, timeoutHops = 0; bool reached = false; bool blocked = false;

    // RTT formatter 鈥?fixed 8-char width for consistent column alignment.
    // "<1 ms" case uses 3 leading spaces (= 8 chars total) to match the
    // "%1 ms".arg(ms, 5) format which produces "  NNN ms" (also 8 chars).
    // This ensures timeout * * * placeholders (8-char) align with all RTT values.
    auto fmtRtt = [](int ms) -> QString {
        if (ms < 1) return QStringLiteral("   <1 ms");  // 3 spaces + "<1 ms" = 8 chars
        return QStringLiteral("%1 ms").arg(ms, 5);      // 5-char right-padded number + " ms" = 8 chars
    };
    // Hop line builder 鈥?shared format eliminates duplicated format strings
    auto fmtHop = [](int ttl, const QString& rtt, const QString& name, const QString& ip) -> QString {
        return QStringLiteral(" %1  %2  %3  %4  %5 [%6]")
            .arg(ttl, 2).arg(rtt).arg(rtt).arg(rtt).arg(name).arg(ip);
    };

    for (int ttl = 1; ttl <= 30 && !reached; ++ttl) {
        int rttMs = 0; QString hopIp;
        int res = tcpTraceHop(host, ttl, rttMs, hopIp);
        ++hopCount;

        if (res == 0) {
            // Reached target
            reached = true;
            QString rttStr = fmtRtt(rttMs);
            lines.append(fmtHop(ttl, rttStr, host, targetIpStr));
            TRACE(" traceroute TTL=%d: REACHED %s [%s] %dms\n",
                ttl, host.toUtf8().constData(), hopIp.toUtf8().constData(), rttMs);
        } else if (res == 1 || res == 2) {
            // res 1 = intermediate router (ICMP Time Exceeded).
            // res 2 = a non-target router replied Destination Unreachable and is
            //         filtering the path (e.g. corporate proxy / admin-prohibited).
            QString rttStr = fmtRtt(rttMs);
            // Resolve reverse DNS for the hop IP
            QString hopName = hopIp;
            struct sockaddr_in sa; memset(&sa, 0, sizeof(sa));
            sa.sin_family = AF_INET;
            sa.sin_addr.s_addr = inet_addr(hopIp.toUtf8().constData());
            char hbuf[NI_MAXHOST] = {};
            if (getnameinfo(reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa), hbuf, sizeof(hbuf),
                            nullptr, 0, 0) == 0 && hbuf[0])
                hopName = QString::fromLatin1(hbuf);
            lines.append(fmtHop(ttl, rttStr, hopName, hopIp));
            TRACE(" traceroute TTL=%d: hop %s [%s] %dms%s\n",
                ttl, hopName.toUtf8().constData(), hopIp.toUtf8().constData(), rttMs,
                res == 2 ? " (filtered)" : "");
            if (res == 2) {
                // The path is administratively blocked at this router. Probing
                // deeper TTLs would only repeat this router or time out, so stop
                // here and say so (instead of the misleading "reached target").
                lines.append(QStringLiteral("       ^ this router filtered the probe"
                    " (Destination Unreachable) - the path is blocked here."));
                blocked = true;
                break;
            }
        } else {
            // Timeout 鈥?use fixed-width RTT columns (8 chars each + 2-space gap)
            // to align * markers with RTT values in normal hop lines.
            ++timeoutHops;
            QString star = QStringLiteral("       *");  // 7 spaces + "*" = 8 chars
            lines.append(QStringLiteral(" %1  %2  %3  %4     Request timed out.")
                .arg(ttl, 2).arg(star).arg(star).arg(star));
            TRACE(" traceroute TTL=%d: timeout (total=%d)\n", ttl, timeoutHops);
            if (timeoutHops > 15) {
                lines.append(QStringLiteral(" ... (firewall may be blocking probes after hop %1)").arg(ttl));
                break;
            }
        }
    }

    r.durationMs = t.elapsed();

    // If all ICMP probes failed (no hop responded), try TCP to verify whether the
    // target is actually reachable. Networks and device sandboxes (e.g. iOS) often
    // filter ICMP while allowing TCP 鈥?ping works via its TCP fallback, but ICMP
    // traceroute shows all "* * * *". The TCP check surfaces this distinction.
    bool tcpReachable = false;
    if (!reached && !blocked) {
        const int ports[] = {443, 80, 22, 8080};
        for (int p : ports) {
            int rtt = tcpRttMs(host, p);
            if (rtt >= 0) {
                tcpReachable = true;
                lines.append(QString());
                lines.append(QStringLiteral("NOTE: All ICMP probes timed out."));
                lines.append(QStringLiteral("  Target %1 [%2] is reachable via TCP port %3 (%4 ms).")
                    .arg(host, targetIpStr).arg(p).arg(rtt));
                lines.append(QStringLiteral("  ICMP may be filtered by the network or device 鈥?route discovery unavailable."));
                break;
            }
        }
    }

    lines.append(QString());
    if (reached) {
        lines.append(QStringLiteral("Trace complete."));
    } else if (blocked) {
        lines.append(QStringLiteral("Trace stopped - a router/firewall filtered the probes (path blocked)."));
    } else if (tcpReachable) {
        lines.append(QStringLiteral("Trace incomplete 鈥?ICMP filtered."));
    } else {
        lines.append(QStringLiteral("Trace incomplete 鈥?target may be firewalled."));
    }
    // Hint if raw ICMP sockets are unavailable (no CAP_NET_RAW)
    if (!s_rawIcmpAvailable) {
        lines.append(QString());
        lines.append(QStringLiteral("NOTE: Raw ICMP sockets unavailable (requires CAP_NET_RAW)."));
        lines.append(QStringLiteral("  Only the target hop is visible; intermediate routers cannot be detected."));
        lines.append(QStringLiteral("  To enable full traceroute, run:"));
        lines.append(QStringLiteral("    sudo setcap cap_net_raw=ep <path-to-binary>"));
        lines.append(QStringLiteral("  Or launch with:  sudo <path-to-binary>"));
    }
    r.rawOutput = lines.join('\n');
    r.details   = lines.join('\n');
    if (reached) { r.status = DiagStatus::Pass; r.summary = QStringLiteral("Target Reached in %1 Hops").arg(hopCount); }
    else if (blocked) { r.status = DiagStatus::Warning; r.summary = QStringLiteral("Path Filtered by a Router at Hop %1").arg(hopCount); }
    else if (tcpReachable) { r.status = DiagStatus::Warning; r.summary = QStringLiteral("ICMP filtered 鈥?%1 reachable via TCP").arg(host); }
    else if (hopCount > 0) { r.status = DiagStatus::Warning; r.summary = QStringLiteral("Partial Path (%1 Hops)").arg(hopCount); }
    else { r.status = DiagStatus::Fail; r.summary = QStringLiteral("No Hops Discovered"); }
    return r;
}

// 鈹€鈹€ PathPing 鈥?Windows pathping.exe format with TCP-based traceroute 鈹€鈹€鈹€鈹€鈹€鈹€鈹€
}
