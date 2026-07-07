#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif
#include "engine/diagnostics/G4/G4RemoteHost.h"
#include "util/DebugSwitch.h"
#include "engine/diagnostics/G4/G4PingParser.h"
#include "util/Logger.h"
#include "util/DnsResolver.h"
#include "util/NetUtil.h"
#include <QHostInfo>
#include <QElapsedTimer>
#include <atomic>
#include <QFile>
#include <QDir>
#include <cstring>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <process.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <windns.h>
#define close closesocket
#define getpid _getpid
#ifndef ECONNREFUSED
#define ECONNREFUSED WSAECONNREFUSED
#endif
#ifndef EHOSTUNREACH
#define EHOSTUNREACH WSAEHOSTUNREACH
#endif
#ifndef ENETUNREACH
#define ENETUNREACH WSAENETUNREACH
#endif
#define socklen_t int
#define SHUT_RDWR SD_BOTH
inline int setNonblockWin(int sock) { u_long mode=1; return ioctlsocket(sock, FIONBIO, &mode); }
inline int setSockOptRcvTimeout(int sock, int sec) { int t=sec*1000; return setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(const char*)&t,sizeof(t)); }
#define fcntl dont_use_fcntl_on_windows
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <resolv.h>
#include <arpa/nameser.h>
// macOS Apple Clang compatibility: C_IN may not be exposed by default
#ifndef C_IN
#define C_IN ns_c_in
#endif
inline int setNonblockWin(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}
inline int setSockOptRcvTimeout(int, int) { return 0; }
#endif

namespace G4RemoteHost {

// Set to false when raw ICMP socket creation fails (no CAP_NET_RAW / root).
// Used by traceroute()/pathPing() to display a helpful fix hint.
static std::atomic<bool> s_rawIcmpAvailable{true};

// Thread-safe IPv4 formatting (inet_ntoa uses a shared static buffer and is
// unsafe in the concurrent diagnostic thread pool).
static QString ip4ToStr(struct in_addr a) {
    char buf[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &a, buf, sizeof(buf));
    return QString::fromLatin1(buf);
}

static ResultProperty prop(const QString& label, const QString& value) {
    return ResultProperty(label, value);
}

// ── Extract hostname from target (URL or hostname) ─────────────────────
QString extractHostname(const QString& target) {
    QString t = target.trimmed();
    // If it's a URL (contains ://), parse out the hostname
    if (t.contains("://")) {
        QString afterScheme = t.section("://", 1);      // "example.com:8080/path"
        auto slash = afterScheme.indexOf('/');
        if (slash >= 0) afterScheme = afterScheme.left(slash); // "example.com:8080"
        // Strip IPv6 bracket notation
        if (afterScheme.startsWith('[')) {
            auto closing = afterScheme.indexOf(']');
            if (closing > 0) afterScheme = afterScheme.mid(1, closing - 1); // "::1"
        } else {
            // Strip port
            auto colon = afterScheme.lastIndexOf(':');
            if (colon > 0) afterScheme = afterScheme.left(colon); // "example.com"
        }
        return afterScheme;
    }
    // Plain hostname — strip port if present
    if (t.contains(':')) {
        auto colon = t.lastIndexOf(':');
        // IPv6 has multiple colons, port uses single colon after hostname
        if (t.count(':') == 1) t = t.left(colon);
    }
    return t;
}

// Derive the TCP port to probe from a target that may be a URL or host[:port].
// Returns an explicit port if present, else the scheme's default
// (443 https / 80 http / 21 ftp / 990 ftps), else 443 — which is far more
// universally reachable than 80 for MTU/MSS probing of modern hosts.
static int extractProbePort(const QString& target) {
    QString t = target.trimmed();
    QString scheme;
    QString rest = t;
    if (t.contains("://")) {
        scheme = t.section("://", 0, 0).toLower();
        rest = t.section("://", 1);
    }
    auto slash = rest.indexOf('/');
    if (slash >= 0) rest = rest.left(slash);   // strip path
    if (rest.startsWith('[')) {                 // [IPv6]:port
        auto closing = rest.indexOf(']');
        if (closing > 0 && closing + 1 < rest.size() && rest.at(closing + 1) == ':') {
            int p = rest.mid(closing + 2).toInt();
            if (p > 0) return p;
        }
    } else if (rest.count(':') == 1) {          // host:port
        int p = rest.section(':', 1, 1).toInt();
        if (p > 0) return p;
    }
    if (scheme == QLatin1String("https")) return 443;
    if (scheme == QLatin1String("http"))  return 80;
    if (scheme == QLatin1String("ftp"))   return 21;
    if (scheme == QLatin1String("ftps"))  return 990;
    return 443;
}

// ── DNS Resolution — full dig-like output ─────────────────────────────
#ifndef _WIN32
// ── DNS wire query + full section dump helper ──────────────────────────────
static void dnsDumpSection(ns_msg& handle, ns_sect section, const QString& title,
                           const QString& host, QStringList& out, bool& gotCname, QString& cnameTarget) {
    int count = ns_msg_count(handle, section);
    if (count <= 0) return;
    bool header = false;
    for (int i = 0; i < count; i++) {
        ns_rr rr;
        if (ns_parserr(&handle, section, i, &rr) < 0) continue;
        if (!header) { out.append(title); header = true; }

        // Get owner name from the RR (rr.name points into the message buffer)
        char ownBuf[256] = {};
        if (rr.name)
            ns_name_uncompress(ns_msg_base(handle), ns_msg_end(handle), (const unsigned char*)rr.name, ownBuf, sizeof(ownBuf));
        if (ownBuf[0] == '\0') strcpy(ownBuf, host.toUtf8().constData());

        uint32_t ttl = ns_rr_ttl(rr);
        int rtype = ns_rr_type(rr);
        const unsigned char* rd = ns_rr_rdata(rr);

        if (rtype == ns_t_a) {
            struct in_addr a; memcpy(&a, rd, 4);
            out.append(QStringLiteral("%1.  %2  IN  A  %3")
                .arg(QString::fromLatin1(ownBuf), -30).arg(ttl, 6).arg(ip4ToStr(a)));
        } else if (rtype == ns_t_aaaa) {
            char ip6[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, rd, ip6, sizeof(ip6));
            out.append(QStringLiteral("%1.  %2  IN  AAAA  %3")
                .arg(QString::fromLatin1(ownBuf), -30).arg(ttl, 6).arg(QString::fromLatin1(ip6)));
        } else if (rtype == ns_t_cname) {
            char cname[256];
            ns_name_uncompress(ns_msg_base(handle), ns_msg_end(handle), rd, cname, sizeof(cname));
            out.append(QStringLiteral("%1.  %2  IN  CNAME  %3")
                .arg(QString::fromLatin1(ownBuf), -30).arg(ttl, 6).arg(QString::fromLatin1(cname)));
            gotCname = true; cnameTarget = QString::fromLatin1(cname);
        } else if (rtype == ns_t_mx) {
            uint16_t pref = (rd[0] << 8) | rd[1];
            char mx[256];
            ns_name_uncompress(ns_msg_base(handle), ns_msg_end(handle), rd + 2, mx, sizeof(mx));
            out.append(QStringLiteral("%1.  %2  IN  MX  %3 %4")
                .arg(QString::fromLatin1(ownBuf), -30).arg(ttl, 6).arg(pref).arg(QString::fromLatin1(mx)));
        } else if (rtype == ns_t_ns) {
            char ns[256];
            ns_name_uncompress(ns_msg_base(handle), ns_msg_end(handle), rd, ns, sizeof(ns));
            out.append(QStringLiteral("%1.  %2  IN  NS  %3")
                .arg(QString::fromLatin1(ownBuf), -30).arg(ttl, 6).arg(QString::fromLatin1(ns)));
        } else if (rtype == ns_t_soa) {
            char mname[256], rname[256];
            const unsigned char* p = rd;
            int len1 = ns_name_uncompress(ns_msg_base(handle), ns_msg_end(handle), p, mname, sizeof(mname));
            if (len1 < 0) continue; // malformed SOA mname — skip
            p += len1;
            int len2 = ns_name_uncompress(ns_msg_base(handle), ns_msg_end(handle), p, rname, sizeof(rname));
            if (len2 < 0) continue; // malformed SOA rname — skip
            p += len2;
            // Verify we have enough RDATA for the 5 fixed 32-bit fields (20 bytes)
            if (p + 20 > ns_msg_end(handle)) continue;
            uint32_t serial = (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];
            uint32_t refresh = (p[4]<<24)|(p[5]<<16)|(p[6]<<8)|p[7];
            uint32_t retry = (p[8]<<24)|(p[9]<<16)|(p[10]<<8)|p[11];
            uint32_t expire = (p[12]<<24)|(p[13]<<16)|(p[14]<<8)|p[15];
            uint32_t minimum = (p[16]<<24)|(p[17]<<16)|(p[18]<<8)|p[19];
            out.append(QStringLiteral("%1.  %2  IN  SOA  %3 %4")
                .arg(QString::fromLatin1(ownBuf), -30).arg(ttl, 6)
                .arg(QString::fromLatin1(mname)).arg(QString::fromLatin1(rname)));
            out.append(QStringLiteral("        (serial %1  refresh %2  retry %3  expire %4  minimum %5)")
                .arg(serial).arg(refresh).arg(retry).arg(expire).arg(minimum));
        } else if (rtype == ns_t_txt) {
            // TXT records: rd[0] = length, rd[1..] = data
            int len = rd[0];
            QByteArray txtData((const char*)(rd + 1), len);
            out.append(QStringLiteral("%1.  %2  IN  TXT  \"%3\"")
                .arg(QString::fromLatin1(ownBuf), -30).arg(ttl, 6).arg(QString::fromLatin1(txtData)));
        } else {
            out.append(QStringLiteral("%1.  %2  IN  %3  (type %4)")
                .arg(QString::fromLatin1(ownBuf), -30).arg(ttl, 6).arg(QStringLiteral("UNKNOWN")).arg(rtype));
        }
    }
}

static QString rcodeStr(int rcode) {
    switch (rcode) {
        case ns_r_noerror: return QStringLiteral("NOERROR");
        case ns_r_formerr: return QStringLiteral("FORMERR");
        case ns_r_servfail: return QStringLiteral("SERVFAIL");
        case ns_r_nxdomain: return QStringLiteral("NXDOMAIN");
        case ns_r_notimpl: return QStringLiteral("NOTIMP");
        case ns_r_refused: return QStringLiteral("REFUSED");
        default: return QStringLiteral("UNKNOWN");
    }
}
#endif

// DNS resolution helper — wraps DnsResolver with 3s timeout
static quint32 resolveIPv4(const QString& host) {
    return DnsResolver::resolveIPv4(host, 3000);
}

static DiagnosticResult noTargetResult(DiagId id, DiagGroup group);


// Single TCP connect — returns RTT in ms, or -1 on failure
static int tcpRttMs(const QString& host, int port) {
    QElapsedTimer t; t.start();
    int sock = tcpConnect(host, port, 3000);
    if (sock < 0) return -1;
    int ms = static_cast<int>(t.elapsed()); closeSocket(sock); return ms;
}

static DiagnosticResult noTargetResult(DiagId id, DiagGroup group) {
    DiagnosticResult r; r.id = id; r.group = group;
    r.timestamp = QDateTime::currentDateTime();
    r.status = DiagStatus::Skipped; r.summary = QStringLiteral("No target");
    return r;
}

#ifdef _WIN32
// ── Windows ICMP Echo (IcmpSendEcho — no admin required) ──────────────────
static int icmpEchoRttMsWindows(quint32 resolvedIp, int seq, int timeoutMs) {
    HANDLE hIcmp = IcmpCreateFile();
    if (hIcmp == INVALID_HANDLE_VALUE) return -1;

    DWORD ipAddr = htonl(resolvedIp);
    // IcmpSendEcho minimum buffer for reply data + IO_STATUS_BLOCK
    const DWORD replySize = sizeof(ICMP_ECHO_REPLY) + 8 + 256;
    QByteArray replyBuf(replySize, '\0');

    // Send single ICMP echo
    DWORD dwRet = IcmpSendEcho(hIcmp, ipAddr, nullptr, 0, nullptr,
                               replyBuf.data(), replySize, timeoutMs);
    int rtt = -1;
    if (dwRet > 0) {
        auto* pReply = (ICMP_ECHO_REPLY*)replyBuf.data();
        if (pReply->Status == IP_SUCCESS)
            rtt = (int)pReply->RoundTripTime;
    }
    IcmpCloseHandle(hIcmp);
    (void)seq; // silently unused — IcmpSendEcho has no sequence param
    return rtt;
}
#endif

#if !defined(_WIN32) && !defined(__linux__)
// ── ICMP Echo helpers (Apple/BSD) ──────────────────────────────────────────
// SOCK_DGRAM + IPPROTO_ICMP is permitted on iOS/macOS WITHOUT root and WITHOUT
// any special entitlement (Apple's SimplePing pattern), so real ICMP ping works
// in the sandbox. Shared by ping() and the traceroute hop prober below.

// Internet checksum (RFC 1071).
static uint16_t icmpEchoChecksum(const void* data, int len) {
    const uint16_t* w = static_cast<const uint16_t*>(data);
    uint32_t sum = 0;
    while (len > 1) { sum += *w++; len -= 2; }
    if (len == 1) sum += *reinterpret_cast<const uint8_t*>(w);
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return static_cast<uint16_t>(~sum);
}

// Locate the ICMP header inside a datagram-ICMP-socket reply. On Darwin (iOS/
// macOS) the kernel delivers SOCK_DGRAM ICMP replies WITH the leading IPv4 header
// — exactly what Apple's SimplePing skips via -icmpHeaderOffsetInIPv4Packet:
// (offset = (versionAndHeaderLength & 0x0F) * 4). The IPv4 version nibble (0x4X)
// never collides with the ICMP types we read (0 EchoReply / 3 Unreach / 11 TTL),
// so this also works unchanged on stacks that deliver no IP header (returns 0).
static int icmpOffsetIn(const unsigned char* buf, ssize_t n) {
    if (n >= 20 && (buf[0] & 0xF0) == 0x40) {
        int ihl = (buf[0] & 0x0F) * 4;
        if (ihl >= 20 && n >= ihl + 8) return ihl;
    }
    return 0;
}

// Single ICMP Echo probe to an IPv4 address (host byte order). Returns RTT in ms,
// or -1 on timeout/failure. On Darwin the kernel rewrites the identifier and
// recomputes the checksum, but we fill a valid checksum for BSD correctness.
static int icmpEchoRttMs(quint32 ipHostOrder, int seq, int timeoutMs) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    if (sock < 0) return -1;
    // fd_set is a fixed FD_SETSIZE bitmap; FD_SET with fd >= FD_SETSIZE overflows
    // the stack. Under heavy concurrency the descriptor can climb; bail out safely.
    if (sock >= FD_SETSIZE) { closeSocket(sock); return -1; }
    struct timeval rcvTo;
    rcvTo.tv_sec = timeoutMs / 1000;
    rcvTo.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &rcvTo, sizeof(rcvTo));

    unsigned char packet[16];
    memset(packet, 0, sizeof(packet));
    packet[0] = 8; // Type = Echo Request
    packet[1] = 0; // Code = 0
    uint16_t ident = static_cast<uint16_t>(getpid() & 0xFFFF);
    uint16_t s     = static_cast<uint16_t>(seq);
    packet[4] = static_cast<unsigned char>(ident >> 8); packet[5] = static_cast<unsigned char>(ident & 0xFF);
    packet[6] = static_cast<unsigned char>(s     >> 8); packet[7] = static_cast<unsigned char>(s     & 0xFF);
    uint16_t ck = icmpEchoChecksum(packet, sizeof(packet));
    memcpy(&packet[2], &ck, sizeof(ck));

    struct sockaddr_in dst; memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = htonl(ipHostOrder);

    QElapsedTimer tm; tm.start();
    if (::sendto(sock, packet, sizeof(packet), 0, (struct sockaddr*)&dst, sizeof(dst)) < 0) {
        closeSocket(sock); return -1;
    }
    while ((int)tm.elapsed() < timeoutMs) {
        fd_set rfds; FD_ZERO(&rfds); FD_SET(sock, &rfds);
        struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 200000; // 200ms poll
        int sel = select(sock + 1, &rfds, nullptr, nullptr, &tv);
        if (sel < 0) break;
        if (sel == 0) continue;
        unsigned char buf[1024]; struct sockaddr_in from; socklen_t fl = sizeof(from);
        ssize_t n = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fl);
        // Darwin delivers SOCK_DGRAM ICMP replies WITH the leading IPv4 header
        // (Apple SimplePing: icmpHeaderOffsetInIPv4Packet). Skip it, then read the
        // ICMP type + echoed sequence number to match our own probe.
        int off = icmpOffsetIn(buf, n);
        if (n < off + 8) continue;
        int type = buf[off];
        uint16_t rseq = static_cast<uint16_t>((buf[off + 6] << 8) | buf[off + 7]);
        if (type == 0 && rseq == static_cast<uint16_t>(seq)) { // Echo Reply for our seq
            int ms = (int)tm.elapsed(); closeSocket(sock); return ms;
        }
        if (type == 3) { closeSocket(sock); return -1; } // Destination Unreachable → loss
    }
    closeSocket(sock); return -1;
}
#endif // Apple/BSD ICMP Echo helpers

// ── Ping — ICMP Echo on iOS/macOS (permission-safe), TCP connect elsewhere ───
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
