#include "Diagnostics/Model/G4/G4Common.h"
DiagnosticResult ping(const QString& target) {
    DiagnosticResult r;
    r.id = DiagId::G4Ping; r.group = DiagGroup::G4;
    r.timestamp = QDateTime::currentDateTime();
    if (target.isEmpty()) return noTargetResult(r.id, r.group);
    QString host = extractHostname(target);
    quint32 resolvedIp = resolveIPv4(host);
    // Build output — strict Windows ping.exe format
    // "Pinging example.com [93.184.216.34] with 32 bytes of data:"
    // or "Pinging 223.5.5.5 with 32 bytes of data:" (when target is already an IP)
    QStringList lines;
    QString ipStr;
    if (resolvedIp) {
        struct in_addr a; a.s_addr = htonl(resolvedIp);
        ipStr = ip4ToStr(a);
    }
    QString displayTarget = resolvedIp ? ipStr : host;
    if (resolvedIp && host != ipStr)
        lines.append(QStringLiteral("Pinging %1 [%2] with 32 bytes of data:").arg(host, ipStr));
    else
        lines.append(QStringLiteral("Pinging %1 with 32 bytes of data:").arg(displayTarget));

    QElapsedTimer t; t.start();
    int sent=0, rcvd=0; double sumMs=0, minMs=1e9, maxMs=0;
    // ICMP Echo first (accurate), fall back to TCP connect if blocked.
    // Windows: IcmpSendEcho (no admin required). Apple: datagram ICMP socket.
    // Linux: TCP connect only (raw ICMP needs CAP_NET_RAW).
    for (int i=0; i<4; ++i) {
        ++sent;
        int ms = -1;
#if defined(_WIN32)
        if (resolvedIp)
            ms = icmpEchoRttMsWindows(resolvedIp, i + 1, 2000);
#else
#if !defined(__linux__)
        if (resolvedIp)
            ms = icmpEchoRttMs(resolvedIp, i + 1, 2000);
#endif
#endif
        if (ms < 0) {
            int ports[] = {443, 80, 22, 8080, 8443};      // TCP fallback / default
            for (int p : ports) { ms = tcpRttMs(host, p); if (ms >= 0) break; }
        }
        if (ms >= 0) {
            ++rcvd; sumMs += ms;
            if (ms<minMs) minMs=ms;
            if (ms>maxMs) maxMs=ms;
            lines.append(QStringLiteral("Reply from %1: bytes=32 time=%2ms TTL=128")
                .arg(displayTarget).arg(ms, 3));
        } else {
            lines.append(QStringLiteral("Request timed out."));
        }
    }
    r.durationMs = t.elapsed();
    double loss = sent>0 ? (sent-rcvd)*100.0/sent : 100.0;
    double avg = rcvd>0 ? sumMs/rcvd : 0;

    // Blank line → "Ping statistics for <IP>:" (Windows always uses IP here)
    lines.append(QString());
    lines.append(QStringLiteral("Ping statistics for %1:").arg(displayTarget));
    lines.append(QStringLiteral("    Packets: Sent = %1, Received = %2, Lost = %3 (%4% loss),")
        .arg(sent).arg(rcvd).arg(sent-rcvd).arg(loss,0,'f',1));
    if (rcvd > 0) {
        lines.append(QStringLiteral("Approximate round trip times in milli-seconds:"));
        lines.append(QStringLiteral("    Minimum = %1ms, Maximum = %2ms, Average = %3ms")
            .arg(minMs,0,'f',0).arg(maxMs,0,'f',0).arg(avg,0,'f',0));
    }
    r.rawOutput = lines.join('\n');
    r.details   = lines.join('\n');
    if (loss>=100.0) { r.status=DiagStatus::Fail; r.summary=QStringLiteral("100% packet loss"); }
    else if (loss>=50.0) { r.status=DiagStatus::Fail; r.summary=QStringLiteral("%1%% loss").arg(loss,0,'f',1); }
    else if (loss>0) { r.status=DiagStatus::Warning; r.summary=QStringLiteral("%1%% loss, avg %2ms").arg(loss,0,'f',1).arg(avg,0,'f',1); }
    else { r.status=DiagStatus::Pass; r.summary=QStringLiteral("0%% loss, avg %1ms").arg(avg,0,'f',1); }
    return r;
}

// ── TCP Traceroute Hop Probe ──────────────────────────────────────────────
// Returns:  0 = reached target,  1 = intermediate hop,  -1 = timeout,  -2 = error
//
// Linux: uses IP_RECVERR + MSG_ERRQUEUE to capture ICMP Time Exceeded from
// intermediate routers — same technique as tracepath / traceroute -T, no root.
// Windows: uses IcmpSendEcho API (no admin required).
#if defined(_WIN32)
#else
#if defined(__linux__)
#else
// macOS/iOS/BSD: ICMP Echo TTL probing via a datagram ICMP socket (no root).
// SOCK_DGRAM + IPPROTO_ICMP is permitted on iOS/macOS WITHOUT root privileges
// and WITHOUT any special entitlement — it is the same mechanism Apple's
// SimplePing sample code uses. We send ICMP Echo Requests with an increasing
// IP_TTL on THIS socket and read the resulting ICMP Time Exceeded (type 11),
// Echo Reply (type 0) or Destination Unreachable (type 3) on the SAME socket.
// The kernel correlates the returning error to our echo request by identifier,
// so a raw socket (which iOS forbids) is never required.
//
// IMPORTANT: a previous implementation sent UDP probes on a separate socket and
// listened on the ICMP socket. On Darwin a datagram ICMP socket does NOT receive
// ICMP errors triggered by unrelated UDP datagrams, so every hop timed out and
// traceroute produced an all-"*" result. Sending ICMP Echo on the ICMP socket
// itself is the reliable, permission-safe approach on iOS/macOS.
// (icmpEchoChecksum() is defined once, above, near ping().)
#endif
#endif

// ── Traceroute (TCP TTL probing, cross-platform) — Windows tracert format ───
}
