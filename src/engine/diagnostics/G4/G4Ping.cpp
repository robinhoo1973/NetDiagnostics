#include "engine/diagnostics/G4/g4common.h"
namespace G4RemoteHost {
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
#elif !defined(__linux__)
        if (resolvedIp)
            ms = icmpEchoRttMs(resolvedIp, i + 1, 2000);
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
                .arg(displayTarget).arg(ms));
        } else {
            lines.append(QStringLiteral("Request timed out."));
        }
    }
    r.durationMs = t.elapsed();
    double loss = sent>0 ? (sent-rcvd)*100.0/sent : 100.0;
    double avg = rcvd>0 ? sumMs/rcvd : 0;
    if (rcvd==0) { minMs=0; maxMs=0; }

    // Blank line → "Ping statistics for <IP>:" (Windows always uses IP here)
    lines.append(QString());
    lines.append(QStringLiteral("Ping statistics for %1:").arg(displayTarget));
    lines.append(QStringLiteral("    Packets: Sent = %1, Received = %2, Lost = %3 (%4% loss),")
        .arg(sent).arg(rcvd).arg(sent-rcvd).arg(loss,0,'f',1));
    if (rcvd > 0) {
        lines.append(QStringLiteral("Approximate round trip times in milli-seconds:"));
        lines.append(QStringLiteral("    Minimum = %1ms, Maximum = %2ms, Average = %3ms")
            .arg(minMs,0,'f',1).arg(maxMs,0,'f',1).arg(avg,0,'f',1));
    }
    r.rawOutput = lines.join('\n');
    r.details   = lines.join('\n');
    if (loss>=100.0) { r.status=DiagStatus::Fail; r.summary=QStringLiteral("100%% packet loss"); }
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
#ifdef _WIN32
static int tcpTraceHop(const QString& host, int ttl, int& rttMs, QString& hopIp) {
    quint32 ip = resolveIPv4(host);
    if (!ip) { rttMs = 0; hopIp.clear(); return -2; }

    HANDLE icmp = IcmpCreateFile();
    if (icmp == INVALID_HANDLE_VALUE) { rttMs = 0; hopIp.clear(); return -2; }

    IP_OPTION_INFORMATION opts;
    memset(&opts, 0, sizeof(opts));
    opts.Ttl = (UCHAR)ttl;

    char sendData[32] = "trace";
    char replyBuf[sizeof(ICMP_ECHO_REPLY) + sizeof(sendData) + 8];
    memset(replyBuf, 0, sizeof(replyBuf));

    QElapsedTimer t; t.start();
    DWORD result = IcmpSendEcho(icmp, htonl(ip),
                                 sendData, sizeof(sendData),
                                 &opts, replyBuf, sizeof(replyBuf), 2000);
    rttMs = (int)t.elapsed();

    if (result > 0) {
        PICMP_ECHO_REPLY echoReply = (PICMP_ECHO_REPLY)replyBuf;
        struct in_addr a; a.S_un.S_addr = echoReply->Address;
        hopIp = ip4ToStr(a);
        rttMs = (int)echoReply->RoundTripTime;
        IcmpCloseHandle(icmp);
        // Only IP_TTL_EXPIRED_TRANSIT (13) means an intermediate hop.
        // IP_DEST_*_UNREACHABLE, IP_SOURCE_QUENCH, etc. are terminal errors.
        if (echoReply->Status == IP_TTL_EXPIRED_TRANSIT)
            return 1;  // intermediate hop
        return 0;      // reached target (IP_SUCCESS) or terminal error
    }
    IcmpCloseHandle(icmp);
    rttMs = 0;
    return -1;
}
#elif defined(__linux__)
static int tcpTraceHop(const QString& host, int ttl, int& rttMs, QString& hopIp) {
    // ── ICMP Echo traceroute (requires CAP_NET_RAW, like traceroute -I) ──
    // Uses raw ICMP socket to send Echo Request with TTL=N. Receives:
    //  - ICMP Echo Reply → reached target
    //  - ICMP Time Exceeded → intermediate router (extract IP)
    //  - Timeout → no response
    //
    // Fallback: if raw socket fails (no CAP_NET_RAW), use TCP connect with TTL.
    // TCP TTL is not honored by some kernels — in that case all hops show as
    // reached target. This is a kernel limitation, not a code bug.
    int icmpSock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (icmpSock < 0) s_rawIcmpAvailable = false;
    if (icmpSock >= 0 && icmpSock >= FD_SETSIZE) { close(icmpSock); icmpSock = -1; s_rawIcmpAvailable = false; }
    if (icmpSock >= 0) {
        // ── Raw ICMP method (traceroute -I) ─────────────────────────────
        quint32 targetIp = resolveIPv4(host);
        if (!targetIp) { close(icmpSock); rttMs = 0; hopIp.clear(); return -2; }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(targetIp);

        // Build ICMP Echo Request
        unsigned char packet[64] = {};
        packet[0] = 8; // ICMP Echo
        packet[1] = 0; // Code
        // Checksum at [2:3], computed below
        { uint16_t v = htons((uint16_t)getpid()); memcpy(packet+4, &v, 2); } // ID
        { uint16_t v = htons((uint16_t)ttl);      memcpy(packet+6, &v, 2); } // Seq

        // ICMP checksum
        uint32_t sum = 0;
        for (int i = 0; i < (int)sizeof(packet); i += 2)
            sum += (packet[i] << 8) | packet[i + 1];
        while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
        { uint16_t v = htons(~sum); memcpy(packet+2, &v, 2); }

        // Set TTL on the raw socket
        setsockopt(icmpSock, IPPROTO_IP, IP_TTL, (const char*)&ttl, sizeof(ttl));

        QElapsedTimer tm; tm.start();
        ssize_t nsent = ::sendto(icmpSock, packet, sizeof(packet), 0, (struct sockaddr*)&addr, sizeof(addr));
        if (nsent < 0) { close(icmpSock); rttMs=0; hopIp.clear(); return -2; }

        // Wait for reply (up to 2 s)
        unsigned char recvBuf[1024];
        while ((int)tm.elapsed() < 2000) {
            fd_set rfds; FD_ZERO(&rfds); FD_SET(icmpSock, &rfds);
            struct timeval tv = {1, 0};
            int sel = select(icmpSock + 1, &rfds, nullptr, nullptr, &tv);
            if (sel < 0) break;
            if (sel == 0) continue;

            struct sockaddr_in from; socklen_t fromLen = sizeof(from);
            ssize_t n = recvfrom(icmpSock, recvBuf, sizeof(recvBuf), 0, (struct sockaddr*)&from, &fromLen);
            if (n < 0) continue;

            // Parse IP header (20 bytes) + ICMP header
            if (n < 28) continue;
            int ipHdrLen = (recvBuf[0] & 0x0f) * 4;
            if (n < ipHdrLen + 8) continue;
            unsigned char* icmp = recvBuf + ipHdrLen;
            int icmpType = icmp[0];
            int icmpCode = icmp[1];

            if (icmpType == 0) {
                // Echo Reply — reached target
                hopIp = ip4ToStr(from.sin_addr);
                rttMs = (int)tm.elapsed();
                close(icmpSock);
                return 0;
            } else if (icmpType == 11 && icmpCode == 0) {
                // Time Exceeded — intermediate router
                // The ICMP payload contains the original IP header + 8 bytes,
                // from which we extract the router's IP from the `from` address.
                hopIp = ip4ToStr(from.sin_addr);
                rttMs = (int)tm.elapsed();
                close(icmpSock);
                return 1;
            } else if (icmpType == 3) {
                // Destination Unreachable — terminal, no further hops
                // Code 3 = Port Unreachable (reached target)
                hopIp = ip4ToStr(from.sin_addr);
                rttMs = (int)tm.elapsed();
                close(icmpSock);
                return (icmpCode == 3) ? 0 : -1;
            }
            // Other ICMP: ignore, try again
        }
        close(icmpSock);
        rttMs = 0; hopIp.clear();
        return -1; // Timeout
    }

    // ── Fallback: TCP connect with TTL (no raw socket) ─────────────────
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { rttMs = 0; hopIp.clear(); return -2; }

    setsockopt(sock, IPPROTO_IP, IP_TTL, (const char*)&ttl, sizeof(ttl));

    quint32 targetIp = resolveIPv4(host);
    if (!targetIp) { closeSocket(sock); rttMs = 0; hopIp.clear(); return -2; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    addr.sin_addr.s_addr = htonl(targetIp);

    setNonblockWin(sock);
    QElapsedTimer tm; tm.start();
    ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));

    fd_set wfds; FD_ZERO(&wfds); FD_SET(sock, &wfds);
    struct timeval tv = {2, 0};
    int sel = select(sock + 1, nullptr, &wfds, nullptr, &tv);
    rttMs = (int)tm.elapsed();

    if (sel > 0 && FD_ISSET(sock, &wfds)) {
        int err = 0; socklen_t len = sizeof(err);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
        if (err == 0 || err == ECONNREFUSED) {
            hopIp = ip4ToStr(addr.sin_addr);
            closeSocket(sock);
            return 0;
        }
    }
    closeSocket(sock);
    rttMs = 0; hopIp.clear();
    return -1;
}
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
static int tcpTraceHop(const QString& host, int ttl, int& rttMs, QString& hopIp) {
    quint32 targetIp = resolveIPv4(host);
    if(!targetIp){rttMs=0;hopIp.clear();return -2;}

    // Datagram ICMP socket — allowed on iOS/macOS without root (SimplePing pattern)
    int icmpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    if (icmpSock >= 0 && icmpSock >= FD_SETSIZE) { closeSocket(icmpSock); rttMs=0; hopIp.clear(); return -1; }
    if (icmpSock < 0) {
        s_rawIcmpAvailable = false;
        // Fallback: TCP connect with TTL (used only if ICMP is somehow unavailable)
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) { rttMs=0; hopIp.clear(); return -2; }
        if (sock >= FD_SETSIZE) { closeSocket(sock); rttMs=0; hopIp.clear(); return -1; }
        setsockopt(sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
        struct sockaddr_in addr; memset(&addr,0,sizeof(addr));
        addr.sin_family=AF_INET; addr.sin_port=htons(80);
        addr.sin_addr.s_addr=htonl(targetIp);
        setNonblockWin(sock);
        QElapsedTimer tm; tm.start();
        ::connect(sock,(struct sockaddr*)&addr,sizeof(addr));
        fd_set wfds; FD_ZERO(&wfds); FD_SET(sock,&wfds);
        struct timeval tv={2,0};
        if(select(sock+1,nullptr,&wfds,nullptr,&tv)>0){
            int err=0; socklen_t len=sizeof(err);
            getsockopt(sock,SOL_SOCKET,SO_ERROR,(char*)&err,&len);
            rttMs=(int)tm.elapsed();
            if(err==0||err==ECONNREFUSED){hopIp=ip4ToStr(*(struct in_addr*)&addr.sin_addr);closeSocket(sock);return 0;}
        }
        closeSocket(sock); rttMs=0; hopIp.clear(); return -1;
    }

    // Per-hop outgoing TTL + a receive timeout guard on the ICMP socket.
    setsockopt(icmpSock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
    struct timeval rcvTo = {2, 0};
    setsockopt(icmpSock, SOL_SOCKET, SO_RCVTIMEO, &rcvTo, sizeof(rcvTo));

    // Build an 8-byte ICMP Echo Request header (+ zero payload). With SOCK_DGRAM
    // the kernel rewrites the identifier and recomputes the checksum, but we fill
    // a valid checksum anyway so the same code is correct on plain BSD too.
    unsigned char packet[16];
    memset(packet, 0, sizeof(packet));
    packet[0] = 8; // Type = Echo Request
    packet[1] = 0; // Code = 0
    uint16_t ident = static_cast<uint16_t>(getpid() & 0xFFFF);
    uint16_t seq   = static_cast<uint16_t>(ttl);
    packet[4] = static_cast<unsigned char>(ident >> 8); packet[5] = static_cast<unsigned char>(ident & 0xFF);
    packet[6] = static_cast<unsigned char>(seq   >> 8); packet[7] = static_cast<unsigned char>(seq   & 0xFF);
    uint16_t ck = icmpEchoChecksum(packet, sizeof(packet));
    memcpy(&packet[2], &ck, sizeof(ck));

    struct sockaddr_in dst; memset(&dst,0,sizeof(dst));
    dst.sin_family=AF_INET;
    dst.sin_addr.s_addr=htonl(targetIp);

    QElapsedTimer tm; tm.start();
    if (::sendto(icmpSock, packet, sizeof(packet), 0, (struct sockaddr*)&dst, sizeof(dst)) < 0) {
        close(icmpSock); rttMs=0; hopIp.clear(); return -2;
    }

    // Listen for the matching ICMP response from either the target or a router.
    while((int)tm.elapsed()<2000){
        fd_set rfds; FD_ZERO(&rfds); FD_SET(icmpSock,&rfds);
        struct timeval tv={2,0};
        int sel=select(icmpSock+1,&rfds,nullptr,nullptr,&tv);
        if(sel<0)break; if(sel==0)continue;
        unsigned char buf[1024]; struct sockaddr_in from; socklen_t fl=sizeof(from);
        ssize_t n=recvfrom(icmpSock,buf,sizeof(buf),0,(struct sockaddr*)&from,&fl);
        // Darwin delivers SOCK_DGRAM ICMP replies WITH the leading IPv4 header
        // (Apple SimplePing: icmpHeaderOffsetInIPv4Packet). Skip it before reading
        // the ICMP type. The router/target IP is the reply's IPv4 source address.
        int off = icmpOffsetIn(buf, n);
        if(n < off + 8) continue;
        int type = buf[off];
        QString srcIp;
        if (off >= 20) { struct in_addr s; memcpy(&s.s_addr, buf + 12, 4); srcIp = ip4ToStr(s); }
        else           srcIp = ip4ToStr(from.sin_addr);
        // Did the TARGET itself send this reply, or a router along the way?
        struct in_addr tgt; tgt.s_addr = htonl(targetIp);
        const bool fromTarget = (srcIp == ip4ToStr(tgt));
        if(type==0){ // Echo Reply — only the destination answers Echo → reached
            hopIp=srcIp; rttMs=(int)tm.elapsed(); close(icmpSock); return 0;
        }
        if(type==11){ // Time Exceeded — an intermediate router on the path
            hopIp=srcIp; rttMs=(int)tm.elapsed(); close(icmpSock); return 1;
        }
        if(type==3){ // Destination Unreachable. Only the TARGET saying "unreachable"
                     // (port/proto closed) counts as reached; a middle router/firewall
                     // replying (e.g. admin-prohibited, code 13) is a FILTERING hop that
                     // blocks the path. Treating every type-3 as "reached" stopped the
                     // trace at the first filtering router and mislabelled it as the
                     // destination (the "only first + last hop" symptom).
            hopIp=srcIp; rttMs=(int)tm.elapsed(); close(icmpSock);
            return fromTarget ? 0 : 2;
        }
    }
    close(icmpSock); rttMs=0; hopIp.clear(); return -1;
}
#endif

// ── Traceroute (TCP TTL probing, cross-platform) — Windows tracert format ───
}
