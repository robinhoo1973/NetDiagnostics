#include "Diagnostics/Model/G4/G4Common.h"
DiagnosticResult ping(const QString& target) {
    DiagnosticResult r;
    r.id = DiagId::G4Ping; r.group = DiagGroup::G4;
    r.timestamp = QDateTime::currentDateTime();
    if (target.isEmpty()) return noTargetResult(r.id, r.group);
    QString host = extractHostname(target);
    quint32 resolvedIp = resolveIPv4(host);
    // Build output -- strict Windows ping.exe format
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
    // 5WHY: On Linux, raw ICMP sockets require CAP_NET_RAW (root), so we
    // always fall back to TCP connect.  Users see "ping" output but the
    // measurement is TCP handshake RTT, not ICMP echo -- significantly
    // different latency characteristics (TCP has 3-way handshake overhead).
    // Flag this so the output clearly distinguishes TCP pings from real ICMP.
    // 5WHY: On Linux and Android, raw ICMP requires CAP_NET_RAW (root),
    // so we always fall back to TCP connect.  Android's NDK defines both
    // __ANDROID__ and __linux__, but icmpEchoRttMs() is only compiled for
    // non-Linux platforms (Apple/BSD).  Including Android here avoids both
    // a compile error and a misleading "0% loss" summary without the (TCP)
    // qualifier when ICMP inevitably fails at runtime.
    bool tcpFallback = false;
#if defined(__linux__) || defined(__ANDROID__)
    tcpFallback = true;
#endif
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
#if !defined(_WIN32) && !defined(__linux__)
        // icmpEchoRttMs is only compiled on Apple/BSD (not Linux/Android).
        // Android defines __linux__ via the NDK, so this gate correctly
        // skips ICMP there — Android apps lack CAP_NET_RAW anyway.
        if (resolvedIp)
            ms = icmpEchoRttMs(resolvedIp, i + 1, 2000);
#endif
#endif
        if (ms < 0) {
            int ports[] = {443, 80, 22, 8080, 8443};
            for (int p : ports) { ms = tcpRttMs(host, p); if (ms >= 0) break; }
            if (ms >= 0 && !tcpFallback)
                tcpFallback = true; // ICMP failed but TCP worked
        }
        if (ms >= 0) {
            ++rcvd; sumMs += ms;
            if (ms<minMs) minMs=ms;
            if (ms>maxMs) maxMs=ms;
            // 5WHY: TTL was hardcoded to 128 (Windows default) regardless of
            // actual platform or whether we used ICMP or TCP fallback.
            // Linux/macOS default TTL is 64, and TCP fallback has no TTL.
            // Drop the misleading fake value.  If ICMP TTL parsing is added
            // later, re-introduce with actual measured values.
            lines.append(QStringLiteral("Reply from %1: bytes=32 time=%2ms")
                .arg(displayTarget).arg(ms, 3));
        } else {
            lines.append(QStringLiteral("Request timed out."));
        }
    }
    r.durationMs = t.elapsed();
    double loss = sent>0 ? (sent-rcvd)*100.0/sent : 100.0;
    double avg = rcvd>0 ? sumMs/rcvd : 0;

    // 5WHY: When TCP fallback is used, the user sees "ping" semantics but
    // the measurement is TCP handshake RTT.  This is misleading without a
    // disclaimer -- TCP connect latency includes SYN→SYN-ACK round-trip
    // plus kernel overhead, typically 2-5ms higher than true ICMP echo.
    // Add a warning so users interpret the numbers correctly.
    if (tcpFallback && rcvd > 0) {
        lines.append(QString());
        lines.append(QStringLiteral(
            "Note: ICMP ping unavailable on this platform (requires root/CAP_NET_RAW). "
            "Measurements are TCP connect RTT, which includes TCP handshake overhead "
            "and may be 2-5ms higher than true ICMP echo latency."));
    }

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
    else if (loss>0) { r.status=DiagStatus::Warning; r.summary=tcpFallback ? QStringLiteral("%1%% loss, avg %2ms (TCP)").arg(loss,0,'f',1).arg(avg,0,'f',1) : QStringLiteral("%1%% loss, avg %2ms").arg(loss,0,'f',1).arg(avg,0,'f',1); }
    else if (tcpFallback) { r.status=DiagStatus::Pass; r.summary=QStringLiteral("0%% loss, avg %1ms (TCP)").arg(avg,0,'f',1); }
    else { r.status=DiagStatus::Pass; r.summary=QStringLiteral("0%% loss, avg %1ms").arg(avg,0,'f',1); }
    return r;
}

} // namespace G4RemoteHost
