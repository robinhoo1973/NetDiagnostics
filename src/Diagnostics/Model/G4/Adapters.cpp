// =============================================================================
// G4/Adapters.cpp — G4 Remote Host adapters (real implementations)
//
// Ported from the archived G4*.cpp behavioral code into the new adapter
// structure (RunContext signature, per-test contract).  Self-contained:
//   · Ping: IcmpSendEcho (Windows, no admin) + TCP-connect fallback
//   · Traceroute/PathPing: IcmpSendEcho with per-probe TTL (Windows);
//     Linux/macOS raw ICMP (needs CAP_NET_RAW, honest fallback to TCP)
//   · MTU Discovery: TCP_MAXSEG getsockopt (Windows), sysfs (Linux),
//     getifaddrs+SIOCGIFMTU (macOS)
//   · IPv6: AAAA resolution + TCP connect over IPv6
// Platforms per NEW-1: all six = All.
// =============================================================================
#if defined(_WIN32)
#ifndef WINAPI_FAMILY
#define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#endif

#include "Common/Services/PlatformAdapter.h"
#include "Common/Services/DnsWire.h"
#include "Common/Model/DiagnosticMeta.h"
#include "Common/Model/DiagNames.h"

#include <QHostInfo>
#include <QElapsedTimer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QUdpSocket>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QRegularExpression>
#include <QDateTime>
#include <QSet>

#include <cmath>
#include <cstring>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>
// MinGW 的 ws2tcpip.h 不定义 TCP_MAXSEG（Windows 上为非文档化选项，取值 0x4，
// 与 Cygwin/ReactOS 头一致）。getsockopt 失败时调用方回退默认 MTU 1500。
#ifndef TCP_MAXSEG
#define TCP_MAXSEG 0x4
#endif
#endif
#if defined(__APPLE__) && !defined(PLATFORM_IOS)
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#if defined(__APPLE__)
// 全 Apple（含 iOS）：datagram ICMP 探针（SimplePing 免 root 模式）
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#endif
#if defined(__linux__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

using namespace PlatformFlag;

namespace g4 {

// ── Result helper ──────────────────────────────────────────────────────────
static DiagnosticResult makeResult(DiagId id, DiagStatus status,
                                   const QString& summary,
                                   const QVector<ResultProperty>& props,
                                   const QString& details) {
    DiagnosticResult r;
    r.id = id; r.displayName = diagDisplayName(id); r.group = diagGroup(id);
    r.status = status; r.summary = summary; r.properties = props;
    r.details = details; r.rawOutput = details;
    r.timestamp = QDateTime::currentDateTime();
    return r;
}

// ── Target parsing (ported from G4Common.h) ────────────────────────────────
static QString extractHostname(const QString& target) {
    QString t = target.trimmed();
    if (t.contains(QLatin1String("://"))) {
        QString after = t.section(QLatin1String("://"), 1);
        const int slash = after.indexOf(QLatin1Char('/'));
        if (slash >= 0) after = after.left(slash);
        if (after.startsWith(QLatin1Char('['))) {
            const int close = after.indexOf(QLatin1Char(']'));
            if (close > 0) after = after.mid(1, close - 1);
        } else {
            const int at = after.lastIndexOf(QLatin1Char('@'));
            if (at >= 0) after = after.mid(at + 1);   // strip userinfo
            const int colon = after.lastIndexOf(QLatin1Char(':'));
            if (colon > 0) after = after.left(colon); // strip port
        }
        return after;
    }
    const int atIdx = t.lastIndexOf(QLatin1Char('@'));
    if (atIdx >= 0) t = t.mid(atIdx + 1);
    if (t.startsWith(QLatin1Char('['))) {
        const int close = t.indexOf(QLatin1Char(']'));
        if (close > 0) {
            if (close + 1 < t.size() && t[close + 1] == QLatin1Char(':'))
                t = t.left(close + 1);
            t = t.mid(1, close - 1);
        }
    } else {
        const int colon = t.indexOf(QLatin1Char(':'));
        if (colon > 0 && t.indexOf(QLatin1Char(':'), colon + 1) == -1)
            t = t.left(colon);
    }
    return t;
}

static int extractProbePort(const QString& target) {
    QString t = target.trimmed();
    QString scheme;
    QString rest = t;
    if (t.contains(QLatin1String("://"))) {
        scheme = t.section(QLatin1String("://"), 0, 0).toLower();
        rest = t.section(QLatin1String("://"), 1);
    }
    const int slash = rest.indexOf(QLatin1Char('/'));
    if (slash >= 0) rest = rest.left(slash);
    const int at = rest.lastIndexOf(QLatin1Char('@'));
    if (at >= 0) rest = rest.mid(at + 1);
    if (rest.startsWith(QLatin1Char('['))) {
        const int close = rest.indexOf(QLatin1Char(']'));
        if (close > 0 && close + 1 < rest.size() && rest.at(close + 1) == QLatin1Char(':')) {
            const int p = rest.mid(close + 2).toInt();
            if (p > 0) return p;
        }
    } else {
        const int colon = rest.indexOf(QLatin1Char(':'));
        if (colon > 0 && rest.indexOf(QLatin1Char(':'), colon + 1) == -1) {
            const int p = rest.mid(colon + 1).toInt();
            if (p > 0) return p;
        }
    }
    if (scheme == QLatin1String("https")) return 443;
    if (scheme == QLatin1String("http"))  return 80;
    if (scheme == QLatin1String("ftp"))   return 21;
    if (scheme == QLatin1String("ftps"))  return 990;
    return 443;
}

// ── Resolution helpers ─────────────────────────────────────────────────────
static quint32 resolveIPv4(const QString& host, int timeoutMs = 3000) {
    QElapsedTimer t; t.start();
    QHostInfo info = QHostInfo::fromName(host);
    if (info.error() != QHostInfo::NoError) return 0;
    for (const QHostAddress& a : info.addresses()) {
        if (a.protocol() == QAbstractSocket::IPv4Protocol)
            return a.toIPv4Address();   // host byte order
    }
    return 0;
}

#if defined(_WIN32)
static QString ip4ToStr(quint32 ipHostOrder) {
    in_addr a; a.S_un.S_addr = htonl(ipHostOrder);
    return QString::fromLatin1(inet_ntoa(a));
}
#elif defined(__APPLE__) || defined(__linux__)
static QString ip4ToStr(quint32 ipHostOrder) {
    in_addr a; a.s_addr = htonl(ipHostOrder);
    return QString::fromLatin1(inet_ntoa(a));
}
#endif

static int tcpRttMs(const QString& host, int port, int timeoutMs = 3000) {
    QTcpSocket sock;
    QElapsedTimer t; t.start();
    sock.connectToHost(host, (quint16)port);
    if (!sock.waitForConnected(timeoutMs)) return -1;
    const int ms = (int)t.elapsed();
    sock.disconnectFromHost();
    return ms;
}

// ── ICMP echo (Windows: IcmpSendEcho, no admin) ───────────────────────────
#if defined(_WIN32)
static int icmpEchoRttMsWindows(quint32 resolvedIp, int /*seq*/, int timeoutMs) {
    HANDLE hIcmp = IcmpCreateFile();
    if (hIcmp == INVALID_HANDLE_VALUE) return -1;
    const DWORD replySize = sizeof(ICMP_ECHO_REPLY) + 8 + 256;
    QByteArray replyBuf((int)replySize, '\0');
    const DWORD ipAddr = htonl(resolvedIp);
    const DWORD dwRet = IcmpSendEcho(hIcmp, ipAddr, nullptr, 0, nullptr,
                                     replyBuf.data(), replySize, (DWORD)timeoutMs);
    int rtt = -1;
    if (dwRet > 0) {
        const auto* pReply = (const ICMP_ECHO_REPLY*)replyBuf.constData();
        if (pReply->Status == IP_SUCCESS)
            rtt = (int)pReply->RoundTripTime;
    }
    IcmpCloseHandle(hIcmp);
    return rtt;
}
#endif

// Traceroute hop probe: 0 = reached target, 1 = intermediate hop,
// 2 = filtered (unreachable from a non-target router), -1 = timeout, -2 = error.
#if defined(_WIN32)
static int traceHopWindows(quint32 ip, int ttl, int& rttMs, QString& hopIp) {
    HANDLE hIcmp = IcmpCreateFile();
    if (hIcmp == INVALID_HANDLE_VALUE) { rttMs = 0; hopIp.clear(); return -2; }

    IP_OPTION_INFORMATION opts;
    std::memset(&opts, 0, sizeof(opts));
    opts.Ttl = (UCHAR)ttl;

    char sendData[32] = "trace";
    QByteArray replyBuf(sizeof(ICMP_ECHO_REPLY) + sizeof(sendData) + 8, '\0');
    QElapsedTimer t; t.start();
    const DWORD result = IcmpSendEcho(hIcmp, htonl(ip), sendData, sizeof(sendData),
                                      &opts, replyBuf.data(), (DWORD)replyBuf.size(), 2000);
    rttMs = (int)t.elapsed();
    if (result > 0) {
        const auto* er = (const ICMP_ECHO_REPLY*)replyBuf.constData();
        // ICMP_ECHO_REPLY::Address is in NETWORK byte order — build in_addr
        // directly (do not go through the host-order ip4ToStr helper).
        in_addr na; na.S_un.S_addr = er->Address;
        hopIp = QString::fromLatin1(inet_ntoa(na));
        rttMs = (int)er->RoundTripTime;
        IcmpCloseHandle(hIcmp);
        if (er->Status == IP_TTL_EXPIRED_TRANSIT) return 1;   // intermediate router
        if (er->Status == IP_SUCCESS) return 0;               // reached target
        return 2;                                             // filtered / unreachable
    }
    IcmpCloseHandle(hIcmp);
    rttMs = 0;
    return -1;
}
#else
// ═════════════════════════════════════════════════════════════════════════
// POSIX 共享辅助：ICMP 校验和 + TCP-TTL 逐跳回退（内核可能不遵从 TTL——诚实局限）
// ═════════════════════════════════════════════════════════════════════════
static uint16_t icmpChecksum16(const void* data, int len) {
    const uint16_t* w = static_cast<const uint16_t*>(data);
    uint32_t sum = 0;
    while (len > 1) { sum += *w++; len -= 2; }
    if (len == 1) sum += *reinterpret_cast<const uint8_t*>(w);
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return static_cast<uint16_t>(~sum);
}

static int tcpTtlHop(quint32 ip, int ttl, int port, int& rttMs, QString& hopIp) {
    const int sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0 || sock >= FD_SETSIZE) {
        if (sock >= 0) ::close(sock);
        rttMs = 0; hopIp.clear(); return -2;
    }
    setsockopt(sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
    const int fl = ::fcntl(sock, F_GETFL, 0);
    ::fcntl(sock, F_SETFL, fl | O_NONBLOCK);
    sockaddr_in addr; std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    addr.sin_addr.s_addr = htonl(ip);
    QElapsedTimer tm; tm.start();
    ::connect(sock, (sockaddr*)&addr, sizeof(addr));
    fd_set wfds; FD_ZERO(&wfds); FD_SET(sock, &wfds);
    timeval tv; tv.tv_sec = 2; tv.tv_usec = 0;
    const int sel = ::select(sock + 1, nullptr, &wfds, nullptr, &tv);
    if (sel > 0 && FD_ISSET(sock, &wfds)) {
        int err = 0; socklen_t elen = sizeof(err);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &elen);
        rttMs = (int)tm.elapsed();
        if (err == 0 || err == ECONNREFUSED) {
            hopIp = ip4ToStr(ip);
            ::close(sock);
            return 0;   // 只有内核遵从 TTL 时才是真跳；否则恒为“目标”。
        }
    }
    ::close(sock);
    rttMs = 0; hopIp.clear();
    return -1;
}
#endif

#if defined(__linux__) && !defined(__ANDROID__)
// ── Linux ping：raw ICMP（需 CAP_NET_RAW），应答按 ID 过滤（归档教训：
//    raw socket 收到本机其他进程的 ICMP 报文，必须验回声标识符）。
static int icmpEchoRttMsLinux(quint32 ip, int seq, int timeoutMs) {
    const int sock = ::socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0 || sock >= FD_SETSIZE) { if (sock >= 0) ::close(sock); return -1; }
    timeval rcvTo; rcvTo.tv_sec = timeoutMs / 1000; rcvTo.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &rcvTo, sizeof(rcvTo));
    unsigned char packet[16];
    std::memset(packet, 0, sizeof(packet));
    packet[0] = 8;                                   // Echo Request
    const uint16_t ident = (uint16_t)(getpid() & 0xFFFF);
    const uint16_t s = (uint16_t)seq;
    packet[4] = (unsigned char)(ident >> 8); packet[5] = (unsigned char)(ident & 0xFF);
    packet[6] = (unsigned char)(s >> 8);     packet[7] = (unsigned char)(s & 0xFF);
    const uint16_t ck = icmpChecksum16(packet, sizeof(packet));
    std::memcpy(&packet[2], &ck, sizeof(ck));
    sockaddr_in dst; std::memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = htonl(ip);
    QElapsedTimer tm; tm.start();
    if (::sendto(sock, packet, sizeof(packet), 0, (sockaddr*)&dst, sizeof(dst)) < 0) {
        ::close(sock); return -1;
    }
    while ((int)tm.elapsed() < timeoutMs) {
        fd_set rfds; FD_ZERO(&rfds); FD_SET(sock, &rfds);
        timeval tv; tv.tv_sec = 0; tv.tv_usec = 200000;
        const int sel = ::select(sock + 1, &rfds, nullptr, nullptr, &tv);
        if (sel < 0) break;
        if (sel == 0) continue;
        unsigned char buf[1024]; sockaddr_in from; socklen_t fl = sizeof(from);
        const ssize_t n = ::recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&from, &fl);
        if (n < 28) continue;
        const int ipHdrLen = (buf[0] & 0x0F) * 4;
        if (n < ipHdrLen + 8) continue;
        const unsigned char* icmp = buf + ipHdrLen;
        const int type = icmp[0];
        const uint16_t rid = (uint16_t)((icmp[4] << 8) | icmp[5]);
        if (type == 0 && rid == ident) {   // Echo Reply for our probe
            const int ms = (int)tm.elapsed(); ::close(sock); return ms;
        }
        if (type == 3) { ::close(sock); return -1; }
    }
    ::close(sock);
    return -1;
}

// ── Linux traceroute：raw ICMP（CAP_NET_RAW）→ TCP-TTL 回退 ──
static int traceHopLinux(quint32 ip, int ttl, int& rttMs, QString& hopIp) {
    const int sock = ::socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0 || sock >= FD_SETSIZE) {
        if (sock >= 0) ::close(sock);
        return tcpTtlHop(ip, ttl, 443, rttMs, hopIp);   // 无 CAP_NET_RAW → TCP-TTL
    }
    if (::setsockopt(sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0) {
        ::close(sock);
        return tcpTtlHop(ip, ttl, 443, rttMs, hopIp);
    }
    unsigned char packet[16];
    std::memset(packet, 0, sizeof(packet));
    packet[0] = 8;
    const uint16_t ident = (uint16_t)(getpid() & 0xFFFF);
    const uint16_t s = (uint16_t)(ttl & 0xFFFF);
    packet[4] = (unsigned char)(ident >> 8); packet[5] = (unsigned char)(ident & 0xFF);
    packet[6] = (unsigned char)(s >> 8);     packet[7] = (unsigned char)(s & 0xFF);
    const uint16_t ck = icmpChecksum16(packet, sizeof(packet));
    std::memcpy(&packet[2], &ck, sizeof(ck));
    sockaddr_in dst; std::memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = htonl(ip);
    QElapsedTimer tm; tm.start();
    if (::sendto(sock, packet, sizeof(packet), 0, (sockaddr*)&dst, sizeof(dst)) < 0) {
        ::close(sock); rttMs = 0; hopIp.clear(); return -2;
    }
    while ((int)tm.elapsed() < 2000) {
        fd_set rfds; FD_ZERO(&rfds); FD_SET(sock, &rfds);
        timeval tv; tv.tv_sec = 0; tv.tv_usec = 200000;
        const int sel = ::select(sock + 1, &rfds, nullptr, nullptr, &tv);
        if (sel < 0) break;
        if (sel == 0) continue;
        unsigned char buf[1024]; sockaddr_in from; socklen_t fl = sizeof(from);
        const ssize_t n = ::recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&from, &fl);
        if (n < 28) continue;
        const int ipHdrLen = (buf[0] & 0x0F) * 4;
        if (n < ipHdrLen + 8) continue;
        const unsigned char* icmp = buf + ipHdrLen;
        const int type = icmp[0];
        const int code = icmp[1];
        // ID 校验：raw socket 共享内核接收路径，必须过滤他进程报文。
        bool idMatch = false;
        if (type == 0) {
            idMatch = (((icmp[4] << 8) | icmp[5]) == ident);
        } else if (type == 11 || type == 3) {
            const int pay = ipHdrLen + 8;
            if (pay + 8 <= n) {
                const int echoIpHdr = (buf[pay] & 0x0F) * 4;
                const int echoIc = pay + echoIpHdr;
                if (echoIc + 8 <= n)
                    idMatch = (((buf[echoIc + 4] << 8) | buf[echoIc + 5]) == ident);
            }
        }
        if (!idMatch) continue;
        hopIp = ip4ToStr(ntohl(from.sin_addr.s_addr));
        rttMs = (int)tm.elapsed();
        if (type == 0) { ::close(sock); return 0; }                  // Echo Reply (target)
        if (type == 11 && code == 0) { ::close(sock); return 1; }    // Time Exceeded (router)
        if (type == 3) { ::close(sock); return (code == 3) ? 0 : 2; } // Unreachable
    }
    ::close(sock);
    rttMs = 0; hopIp.clear();
    return -1;
}
#endif

#if defined(__APPLE__)
// ── Apple（macOS/iOS）ping + traceroute：datagram ICMP（SimplePing 模式，
//    SOCK_DGRAM+IPPROTO_ICMP 免 root/无特殊授权，沙箱内可用）。
static int icmpOffsetIn(const unsigned char* buf, ssize_t n) {
    if (n >= 20 && (buf[0] & 0xF0) == 0x40) {
        const int ihl = (buf[0] & 0x0F) * 4;
        if (ihl >= 20 && n >= ihl + 8) return ihl;
    }
    return 0;
}

static int icmpEchoRttMsApple(quint32 ip, int seq, int timeoutMs) {
    const int sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    if (sock < 0 || sock >= FD_SETSIZE) { if (sock >= 0) ::close(sock); return -1; }
    timeval rcvTo; rcvTo.tv_sec = timeoutMs / 1000; rcvTo.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &rcvTo, sizeof(rcvTo));
    unsigned char packet[16];
    std::memset(packet, 0, sizeof(packet));
    packet[0] = 8;
    const uint16_t ident = (uint16_t)(getpid() & 0xFFFF);
    const uint16_t s = (uint16_t)seq;
    packet[4] = (unsigned char)(ident >> 8); packet[5] = (unsigned char)(ident & 0xFF);
    packet[6] = (unsigned char)(s >> 8);     packet[7] = (unsigned char)(s & 0xFF);
    const uint16_t ck = icmpChecksum16(packet, sizeof(packet));
    std::memcpy(&packet[2], &ck, sizeof(ck));
    sockaddr_in dst; std::memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = htonl(ip);
    QElapsedTimer tm; tm.start();
    if (::sendto(sock, packet, sizeof(packet), 0, (sockaddr*)&dst, sizeof(dst)) < 0) {
        ::close(sock); return -1;
    }
    while ((int)tm.elapsed() < timeoutMs) {
        fd_set rfds; FD_ZERO(&rfds); FD_SET(sock, &rfds);
        timeval tv; tv.tv_sec = 0; tv.tv_usec = 200000;
        const int sel = ::select(sock + 1, &rfds, nullptr, nullptr, &tv);
        if (sel < 0) break;
        if (sel == 0) continue;
        unsigned char buf[1024]; sockaddr_in from; socklen_t fl = sizeof(from);
        const ssize_t n = ::recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&from, &fl);
        const int off = icmpOffsetIn(buf, n);   // Darwin 回包带 IPv4 头
        if (n < off + 8) continue;
        const int type = buf[off];
        const uint16_t rseq = (uint16_t)((buf[off + 6] << 8) | buf[off + 7]);
        if (type == 0 && rseq == s) { const int ms = (int)tm.elapsed(); ::close(sock); return ms; }
        if (type == 3) { ::close(sock); return -1; }
    }
    ::close(sock);
    return -1;
}

static int traceHopMac(quint32 ip, int ttl, int& rttMs, QString& hopIp) {
    const int sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    if (sock < 0 || sock >= FD_SETSIZE) {
        if (sock >= 0) ::close(sock);
        return tcpTtlHop(ip, ttl, 443, rttMs, hopIp);   // 罕见降级：TCP-TTL
    }
    setsockopt(sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
    timeval rcvTo; rcvTo.tv_sec = 2; rcvTo.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &rcvTo, sizeof(rcvTo));
    unsigned char packet[16];
    std::memset(packet, 0, sizeof(packet));
    packet[0] = 8;
    const uint16_t ident = (uint16_t)(getpid() & 0xFFFF);
    const uint16_t s = (uint16_t)(ttl & 0xFFFF);
    packet[4] = (unsigned char)(ident >> 8); packet[5] = (unsigned char)(ident & 0xFF);
    packet[6] = (unsigned char)(s >> 8);     packet[7] = (unsigned char)(s & 0xFF);
    const uint16_t ck = icmpChecksum16(packet, sizeof(packet));
    std::memcpy(&packet[2], &ck, sizeof(ck));
    sockaddr_in dst; std::memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = htonl(ip);
    QElapsedTimer tm; tm.start();
    if (::sendto(sock, packet, sizeof(packet), 0, (sockaddr*)&dst, sizeof(dst)) < 0) {
        ::close(sock); rttMs = 0; hopIp.clear(); return -2;
    }
    while ((int)tm.elapsed() < 2000) {
        fd_set rfds; FD_ZERO(&rfds); FD_SET(sock, &rfds);
        timeval tv; tv.tv_sec = 0; tv.tv_usec = 200000;
        const int sel = ::select(sock + 1, &rfds, nullptr, nullptr, &tv);
        if (sel < 0) break;
        if (sel == 0) continue;
        unsigned char buf[1024]; sockaddr_in from; socklen_t fl = sizeof(from);
        const ssize_t n = ::recvfrom(sock, buf, sizeof(buf), 0, (sockaddr*)&from, &fl);
        const int off = icmpOffsetIn(buf, n);
        if (n < off + 8) continue;
        const int type = buf[off];
        QString srcIp;
        if (off >= 20) { in_addr sa; std::memcpy(&sa.s_addr, buf + 12, 4); srcIp = ip4ToStr(ntohl(sa.s_addr)); }
        else           srcIp = ip4ToStr(ntohl(from.sin_addr.s_addr));
        const bool fromTarget = (srcIp == ip4ToStr(ip));
        hopIp = srcIp;
        rttMs = (int)tm.elapsed();
        if (type == 0) { ::close(sock); return 0; }                 // Echo Reply (target)
        if (type == 11) { ::close(sock); return 1; }                // Time Exceeded (router)
        if (type == 3) { ::close(sock); return fromTarget ? 0 : 2; } // 目标不可达=到达；中间过滤=阻断
    }
    ::close(sock);
    rttMs = 0; hopIp.clear();
    return -1;
}
#endif

// ── First available system DNS server (R5-6: wire helpers live in DnsWire.h) ─
static QString systemDnsServer() {
    QString server;
    QFile resolv(QStringLiteral("/etc/resolv.conf"));
    if (resolv.open(QIODevice::ReadOnly)) {
        QTextStream ts(&resolv);
        while (!ts.atEnd()) {
            const QString line = ts.readLine().trimmed();
            if (line.startsWith(QLatin1String("nameserver ")) && server.isEmpty())
                server = line.mid(11);
        }
    }
#if defined(_WIN32)
    if (server.isEmpty()) server = QStringLiteral("223.5.5.5");
#else
    if (server.isEmpty()) server = QStringLiteral("8.8.8.8");
#endif
    return server;
}

// ═════════════════════════════════════════════════════════════════════════
// G4DnsResolution — dig-like output (A/AAAA/CNAME)
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeDnsResolution(DiagId id, const QString& target, RunContext& ctx) {
    const QString host = extractHostname(target.isEmpty() ? QStringLiteral("example.com") : target);
    QStringList out;
    out.append(QStringLiteral("; <<>> NetDiagnostics DNS <<>> %1").arg(host));
    out.append(QStringLiteral(";; QUESTION SECTION:"));
    out.append(QStringLiteral(";%1. IN A").arg(host));

    const dnsWire::Answer ans = dnsWire::udpQuery(host, 1, systemDnsServer(), 3000);
    const int ms = ans.elapsedMs;
    const int rcode = ans.rcode;
    if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
    int anCount = 0;
    QStringList ips;
    for (const auto& rec : ans.records) {
        if (rec.type == 1 || rec.type == 28) {
            out.append(QStringLiteral("%1.  %2  IN  %3  %4")
                .arg(host, -24).arg(rec.ttl, 6)
                .arg(rec.type == 1 ? QStringLiteral("A") : QStringLiteral("AAAA"), rec.value));
            ips.append(rec.value);
            ++anCount;
        } else if (rec.type == 5) {
            out.append(QStringLiteral("%1.  %2  IN  CNAME  %3").arg(host, -24).arg(rec.ttl, 6).arg(rec.value));
        }
    }
    out.append(QString());
    const char* rcodeName = rcode == 0 ? "NOERROR" : rcode == 3 ? "NXDOMAIN"
                          : rcode == 2 ? "SERVFAIL" : rcode == 1 ? "FORMERR" : "UNKNOWN";
    out.append(QStringLiteral(";; Query time: %1 ms; RCODE: %2").arg(ms).arg(QLatin1String(rcodeName)));

    DiagStatus status = anCount > 0 ? DiagStatus::Pass : DiagStatus::Fail;
    QString summary = anCount > 0
        ? QStringLiteral("Resolved to %1 (%2 ms)").arg(ips.join(QStringLiteral(", "))).arg(ms)
        : (rcode == 3 ? QStringLiteral("NXDOMAIN — no such domain") : QStringLiteral("No A records"));

    DiagnosticResult r = makeResult(id, status, summary, {}, out.join(QLatin1Char('\n')));
    r.data[QStringLiteral("queryTimeMs")] = ms;
    r.data[QStringLiteral("rcode")] = rcode;
    r.data[QStringLiteral("answerCount")] = anCount;
    QVariantList recList;
    for (const auto& rec : ans.records) {
        QVariantMap m;
        m[QStringLiteral("type")] = rec.type;
        m[QStringLiteral("ttl")] = rec.ttl;
        m[QStringLiteral("value")] = rec.value;
        recList.append(m);
    }
    r.data[QStringLiteral("records")] = recList;
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G4Ping — ICMP echo (Windows) + TCP fallback, 4 probes, loss/jitter stats
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probePing(DiagId id, const QString& target, RunContext& ctx) {
    const QString host = extractHostname(target.isEmpty() ? QStringLiteral("example.com") : target);
    const quint32 resolvedIp = resolveIPv4(host);
    QString ipStr;
    if (resolvedIp) ipStr = ip4ToStr(resolvedIp);
    const QString displayTarget = resolvedIp ? ipStr : host;

    QStringList lines;
    if (resolvedIp && host != ipStr)
        lines.append(QStringLiteral("Pinging %1 [%2] with 32 bytes of data:").arg(host, ipStr));
    else
        lines.append(QStringLiteral("Pinging %1 with 32 bytes of data:").arg(displayTarget));

    // ICMP 策略（按平台）：Windows=IcmpSendEcho（免 admin）；Apple=datagram
    // ICMP（SimplePing 模式免 root，含 iOS）；Linux=raw ICMP（需 CAP_NET_RAW，
    // 失败回退 TCP）；Android=仅 TCP。
    bool tcpFallback = false;
#if defined(__ANDROID__)
    tcpFallback = true;
#endif

    const int kPingBudgetMs = 22000;   // stay inside the 30s watchdog budget
    QElapsedTimer t; t.start();
    int sent = 0, rcvd = 0;
    double sumMs = 0, minMs = 1e9, maxMs = 0;
    QVariantList individualRtts;

    for (int i = 0; i < 4; ++i) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        if (t.elapsed() >= kPingBudgetMs) break;
        ++sent;
        int ms = -1;
#if defined(_WIN32)
        if (resolvedIp)
            ms = icmpEchoRttMsWindows(resolvedIp, i + 1, 2000);
#elif defined(__APPLE__)
        if (resolvedIp)
            ms = icmpEchoRttMsApple(resolvedIp, i + 1, 2000);
#elif defined(__linux__) && !defined(__ANDROID__)
        if (resolvedIp)
            ms = icmpEchoRttMsLinux(resolvedIp, i + 1, 2000);
#endif
        if (ms < 0) {
            const int ports[] = {443, 80, 22, 8080, 8443};
            for (int p : ports) {
                if (t.elapsed() >= kPingBudgetMs) break;
                ms = tcpRttMs(host, p);
                if (ms >= 0) break;
            }
            if (ms >= 0 && !tcpFallback) tcpFallback = true;
        }
        if (ms >= 0) {
            ++rcvd;
            sumMs += ms;
            if (ms < minMs) minMs = ms;
            if (ms > maxMs) maxMs = ms;
            individualRtts.append(ms);
            lines.append(QStringLiteral("Reply from %1: bytes=32 time=%2ms").arg(displayTarget).arg(ms, 3));
        } else {
            lines.append(QStringLiteral("Request timed out."));
        }
    }
    const double loss = sent > 0 ? (sent - rcvd) * 100.0 / sent : 100.0;
    const double avg = rcvd > 0 ? sumMs / rcvd : 0;
    double jitter = 0.0;
    if (rcvd > 1) {
        double varSum = 0.0;
        for (const QVariant& v : individualRtts)
            varSum += (v.toDouble() - avg) * (v.toDouble() - avg);
        jitter = std::sqrt(varSum / rcvd);
    }
    if (tcpFallback && rcvd > 0) {
        lines.append(QString());
        lines.append(QStringLiteral(
            "Note: ICMP ping unavailable on this platform (requires root/CAP_NET_RAW). "
            "Measurements are TCP connect RTT, which includes TCP handshake overhead "
            "and may be 2-5ms higher than true ICMP echo latency."));
    }
    lines.append(QString());
    lines.append(QStringLiteral("Ping statistics for %1:").arg(displayTarget));
    lines.append(QStringLiteral("    Packets: Sent = %1, Received = %2, Lost = %3 (%4% loss)")
        .arg(sent).arg(rcvd).arg(sent - rcvd).arg(loss, 0, 'f', 1));
    if (rcvd > 0) {
        lines.append(QStringLiteral("Approximate round trip times in milli-seconds:"));
        lines.append(QStringLiteral("    Minimum = %1ms, Maximum = %2ms, Average = %3ms")
            .arg(minMs, 0, 'f', 0).arg(maxMs, 0, 'f', 0).arg(avg, 0, 'f', 0));
    }

    DiagStatus status;
    QString summary;
    if (loss >= 100.0)          { status = DiagStatus::Fail; summary = QStringLiteral("100% packet loss"); }
    else if (loss >= 50.0)      { status = DiagStatus::Fail; summary = QStringLiteral("%1% loss").arg(loss, 0, 'f', 1); }
    else if (loss > 0)          { status = DiagStatus::Warning;
                                  summary = tcpFallback ? QStringLiteral("%1% loss, avg %2ms (TCP)").arg(loss, 0, 'f', 1).arg(avg, 0, 'f', 1)
                                                        : QStringLiteral("%1% loss, avg %2ms").arg(loss, 0, 'f', 1).arg(avg, 0, 'f', 1); }
    else if (tcpFallback)       { status = DiagStatus::Pass; summary = QStringLiteral("0% loss, avg %1ms (TCP)").arg(avg, 0, 'f', 1); }
    else                        { status = DiagStatus::Pass; summary = QStringLiteral("0% loss, avg %1ms").arg(avg, 0, 'f', 1); }

    DiagnosticResult r = makeResult(id, status, summary, {}, lines.join(QLatin1Char('\n')));
    r.data[QStringLiteral("target")] = host;
    r.data[QStringLiteral("resolvedIp")] = ipStr;
    r.data[QStringLiteral("packetsSent")] = sent;
    r.data[QStringLiteral("packetsReceived")] = rcvd;
    r.data[QStringLiteral("packetsLost")] = sent - rcvd;
    r.data[QStringLiteral("lossPercent")] = loss;
    r.data[QStringLiteral("rttMinMs")] = rcvd > 0 ? minMs : 0.0;
    r.data[QStringLiteral("rttMaxMs")] = rcvd > 0 ? maxMs : 0.0;
    r.data[QStringLiteral("rttAvgMs")] = avg;
    r.data[QStringLiteral("rttJitterMs")] = jitter;
    r.data[QStringLiteral("tcpFallback")] = tcpFallback;
    r.data[QStringLiteral("individualRtts")] = individualRtts;
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G4Traceroute — TTL hop probing (tracert.exe output format)
// ═════════════════════════════════════════════════════════════════════════
static QString fmtRtt(int ms) {
    if (ms < 1) return QStringLiteral("   <1 ms");
    return QStringLiteral("%1 ms").arg(ms, 5);
}

static DiagnosticResult probeTraceroute(DiagId id, const QString& target, RunContext& ctx) {
    const QString host = extractHostname(target.isEmpty() ? QStringLiteral("example.com") : target);
    const quint32 targetIp = resolveIPv4(host);
    if (!targetIp) {
        return makeResult(id, DiagStatus::Fail, QStringLiteral("DNS resolution failed"), {}, {});
    }
    const QString targetIpStr = ip4ToStr(targetIp);

    QStringList lines;
    lines.append(QString());
    lines.append(QStringLiteral("Tracing route to %1 [%2]").arg(host, targetIpStr));
    lines.append(QStringLiteral("over a maximum of 30 hops:"));
    lines.append(QString());

    QElapsedTimer t; t.start();
    int hopCount = 0, timeoutHops = 0;
    bool reached = false, blocked = false;
    QVariantList hops;

    for (int ttl = 1; ttl <= 30 && !reached; ++ttl) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        int rttMs = 0;
        QString hopIp;
#if defined(_WIN32)
        const int res = traceHopWindows(targetIp, ttl, rttMs, hopIp);
#elif defined(__linux__) && !defined(__ANDROID__)
        const int res = traceHopLinux(targetIp, ttl, rttMs, hopIp);
#elif defined(__APPLE__)
        const int res = traceHopMac(targetIp, ttl, rttMs, hopIp);
#else
        // Android：无 raw ICMP → TCP-TTL（内核可能不遵从 TTL——诚实局限）。
        const int res = tcpTtlHop(targetIp, ttl, 443, rttMs, hopIp);
#endif
        ++hopCount;
        if (res == 0) {
            reached = true;
            const QString rtt = fmtRtt(rttMs);
            lines.append(QStringLiteral(" %1  %2  %3  %4  %5 [%6]")
                .arg(ttl, 2).arg(rtt, rtt, rtt, host, targetIpStr));
            QVariantMap h; h[QStringLiteral("ttl")] = ttl; h[QStringLiteral("rttMs")] = rttMs;
            h[QStringLiteral("ip")] = targetIpStr; h[QStringLiteral("name")] = host;
            h[QStringLiteral("reached")] = true; h[QStringLiteral("filtered")] = false;
            hops.append(h);
        } else if (res == 1 || res == 2) {
            QString hopName = hopIp;
            // 私网/fake-ip 地址不做 PTR（慢且无意义）；只对公网 IP 反查。
            const bool privateIp = hopIp.startsWith(QLatin1String("10."))
                || hopIp.startsWith(QLatin1String("192.168."))
                || hopIp.startsWith(QLatin1String("172."))
                || hopIp.startsWith(QLatin1String("127."))
                || hopIp.startsWith(QLatin1String("198.18."))
                || hopIp.startsWith(QLatin1String("100."));
            if (!privateIp && !hopIp.isEmpty()) {
                const QHostInfo info = QHostInfo::fromName(hopIp);   // reverse DNS
                if (info.error() == QHostInfo::NoError && !info.hostName().isEmpty())
                    hopName = info.hostName();
            }
            const QString rtt = fmtRtt(rttMs);
            lines.append(QStringLiteral(" %1  %2  %3  %4  %5 [%6]")
                .arg(ttl, 2).arg(rtt, rtt, rtt, hopName, hopIp));
            QVariantMap h; h[QStringLiteral("ttl")] = ttl; h[QStringLiteral("rttMs")] = rttMs;
            h[QStringLiteral("ip")] = hopIp; h[QStringLiteral("name")] = hopName;
            h[QStringLiteral("reached")] = false; h[QStringLiteral("filtered")] = (res == 2);
            hops.append(h);
            if (res == 2) {
                lines.append(QStringLiteral("       ^ this router filtered the probe (Destination Unreachable) — the path is blocked here."));
                blocked = true;
                break;
            }
        } else {
            ++timeoutHops;
            const QString star = QStringLiteral("       *");
            lines.append(QStringLiteral(" %1  %2  %3  %4     Request timed out.")
                .arg(ttl, 2).arg(star, star, star));
            QVariantMap h; h[QStringLiteral("ttl")] = ttl; h[QStringLiteral("rttMs")] = 0;
            h[QStringLiteral("ip")] = QString(); h[QStringLiteral("name")] = QString();
            h[QStringLiteral("reached")] = false; h[QStringLiteral("filtered")] = false;
            hops.append(h);
            if (timeoutHops > 15) {
                lines.append(QStringLiteral(" ... (firewall may be blocking probes after hop %1)").arg(ttl));
                break;
            }
        }
    }

    // TCP reachability check when the route was never discovered.
    bool tcpReachable = false;
    if (!reached && !blocked) {
        const int ports[] = {443, 80, 22, 8080};
        for (int p : ports) {
            const int rtt = tcpRttMs(host, p);
            if (rtt >= 0) {
                tcpReachable = true;
                lines.append(QString());
                lines.append(QStringLiteral("NOTE: All ICMP probes timed out."));
                lines.append(QStringLiteral("  Target %1 [%2] is reachable via TCP port %3 (%4 ms).")
                    .arg(host, targetIpStr).arg(p).arg(rtt));
                lines.append(QStringLiteral("  ICMP may be filtered by the network — route discovery unavailable."));
                break;
            }
        }
    }
    lines.append(QString());
    if (reached)            lines.append(QStringLiteral("Trace complete."));
    else if (blocked)       lines.append(QStringLiteral("Trace stopped — a router/firewall filtered the probes (path blocked)."));
    else if (tcpReachable)  lines.append(QStringLiteral("Trace incomplete — ICMP filtered."));
    else                    lines.append(QStringLiteral("Trace incomplete — target may be firewalled."));

    DiagStatus status = reached ? DiagStatus::Pass
                      : blocked ? DiagStatus::Warning
                      : DiagStatus::Warning;
    QString summary = reached ? QStringLiteral("%1 hops to target").arg(hopCount)
                    : blocked ? QStringLiteral("Blocked at hop %1").arg(hopCount)
                    : QStringLiteral("Incomplete — %1 hops probed").arg(hopCount);

    DiagnosticResult r = makeResult(id, status, summary, {}, lines.join(QLatin1Char('\n')));
    r.data[QStringLiteral("hopCount")] = hopCount;
    r.data[QStringLiteral("hops")] = hops;
    r.data[QStringLiteral("reached")] = reached;
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G4PathPing — traceroute + per-hop loss statistics on the final hop
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probePathPing(DiagId id, const QString& target, RunContext& ctx) {
    const QString host = extractHostname(target.isEmpty() ? QStringLiteral("example.com") : target);
    const quint32 targetIp = resolveIPv4(host);
    if (!targetIp)
        return makeResult(id, DiagStatus::Fail, QStringLiteral("DNS Resolution Failed"), {}, {});
    const QString targetIpStr = ip4ToStr(targetIp);

    // Phase 1: route discovery (reuse the traceroute probe, then parse hops).
    DiagnosticResult tr = probeTraceroute(id, target, ctx);
    if (tr.isCancelled()) return tr;

    struct HopEntry { int ttl; QString ip; QString name; bool reached; };
    QVector<HopEntry> entries;
    static const QRegularExpression hopRe(QStringLiteral(R"(^\s*(\d+)\s+.*\[([\d.]+)\])"),
                                          QRegularExpression::MultilineOption);
    auto matches = hopRe.globalMatch(tr.rawOutput);
    int hopCount = 0;
    bool reached = false;
    while (matches.hasNext()) {
        const auto m = matches.next();
        const int ttlNum = m.captured(1).toInt();
        const QString hopIp = m.captured(2);
        const bool isTarget = (hopIp == targetIpStr);
        if (isTarget) reached = true;
        entries.append({ttlNum, hopIp, QString(), isTarget});
        hopCount = ttlNum;
    }

    // Phase 2: ping the final hop for loss statistics.
    struct HopStats { int sent = 0; int rcvd = 0; double loss = 100.0; int avgMs = 0; };
    QVector<HopStats> stats;
    for (int i = 1; i < entries.size(); ++i) stats.append({4, 0, 100.0, 0});
    // 需要至少 2 个条目（中间跳 + 目标跳）才有可 ping 的最终跳。
    if (entries.size() > 1 && !entries.last().ip.isEmpty()) {
        DiagnosticResult pr = probePing(id, entries.last().ip, ctx);
        if (pr.isCancelled()) return pr;
        HopStats& hs = stats.last();
        hs.sent = pr.data.value(QStringLiteral("packetsSent")).toInt();
        hs.rcvd = pr.data.value(QStringLiteral("packetsReceived")).toInt();
        hs.loss = pr.data.value(QStringLiteral("lossPercent")).toDouble();
        hs.avgMs = (int)pr.data.value(QStringLiteral("rttAvgMs")).toDouble();
    }

    QStringList lines;
    lines.append(QString());
    lines.append(QStringLiteral("Tracing route to %1 [%2]").arg(host, targetIpStr));
    lines.append(QStringLiteral("over a maximum of 30 hops:"));
    lines.append(QString());
    lines.append(QStringLiteral("  0  %1 [%2]").arg(QHostInfo::localHostName(), QStringLiteral("127.0.0.1")));
    for (int i = 1; i < entries.size(); ++i) {
        const QString ip = entries[i].ip;
        if (ip.isEmpty()) continue;   // timeout 跳无 IP，不生成行
        lines.append(QStringLiteral("  %1  %2 [%3]").arg(entries[i].ttl, 2)
            .arg(entries[i].name.isEmpty() ? QStringLiteral("--") : entries[i].name, ip));
    }
    lines.append(QString());
    lines.append(QStringLiteral("Computing statistics for %1 seconds... (probe)").arg(25));

    QVariantList hopData;
    for (int i = 0; i < stats.size(); ++i) {
        const int ttlNum = entries.value(i + 1).ttl;
        const QString ip = entries.value(i + 1).ip;
        if (ip.isEmpty()) continue;
        const bool isFinal = (i == stats.size() - 1);
        const HopStats& hs = stats[i];
        lines.append(QStringLiteral("  %1  %2  %3/%4 = %5%  avg %6ms")
            .arg(ttlNum, 2).arg(ip, -16)
            .arg(hs.rcvd).arg(hs.sent).arg(100.0 - hs.loss, 0, 'f', 0)
            .arg(hs.avgMs));
        QVariantMap h;
        h[QStringLiteral("ttl")] = ttlNum;
        h[QStringLiteral("ip")] = ip;
        h[QStringLiteral("sent")] = isFinal ? hs.sent : 0;
        h[QStringLiteral("received")] = isFinal ? hs.rcvd : 0;
        h[QStringLiteral("lossPercent")] = isFinal ? hs.loss : 100.0;
        h[QStringLiteral("avgMs")] = isFinal ? hs.avgMs : 0;
        h[QStringLiteral("reached")] = entries.value(i + 1).reached;
        hopData.append(h);
    }

    DiagStatus status = reached ? DiagStatus::Pass : DiagStatus::Warning;
    const double finalLoss = stats.isEmpty() ? 100.0 : stats.last().loss;
    QString summary = reached
        ? QStringLiteral("%1 hops, final-hop loss %2%").arg(hopCount).arg(100.0 - finalLoss, 0, 'f', 0)
        : QStringLiteral("Incomplete — %1 hops").arg(hopCount);

    DiagnosticResult r = makeResult(id, status, summary, {}, lines.join(QLatin1Char('\n')));
    r.data[QStringLiteral("hopCount")] = hopCount;
    r.data[QStringLiteral("hops")] = hopData;
    r.data[QStringLiteral("reached")] = reached;
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G4MtuDiscovery — path MTU via TCP_MAXSEG (Windows) / sysfs (Linux)
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeMtuDiscovery(DiagId id, const QString& target, RunContext& ctx) {
    const QString host = extractHostname(target.isEmpty() ? QStringLiteral("example.com") : target);
    const int probePort = extractProbePort(target);
    const quint32 resolvedIp = resolveIPv4(host);
    const bool targetResolved = (resolvedIp != 0);
    QString ipStr;
    if (resolvedIp) ipStr = ip4ToStr(resolvedIp);
    const QString displayAddr = ipStr.isEmpty() ? host : ipStr;

    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Path MTU Discovery for %1 [%2] (probe TCP port %3)")
        .arg(host, displayAddr).arg(probePort));

    int discoveredMtu = 0;
#if defined(_WIN32)
    if (resolvedIp) {
        SOCKET sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        // fd_set 是定长位图：fd >= FD_SETSIZE 时 FD_SET 栈溢出（归档教训）——超限直接放弃。
        if (sock != INVALID_SOCKET && sock < FD_SETSIZE) {
            sockaddr_in addr; std::memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons((u_short)probePort);
            addr.sin_addr.s_addr = htonl(resolvedIp);
            u_long nonblock = 1;
            ioctlsocket(sock, FIONBIO, &nonblock);
            QElapsedTimer t; t.start();
            ::connect(sock, (sockaddr*)&addr, sizeof(addr));
            fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
            timeval tv; tv.tv_sec = 3; tv.tv_usec = 0;
            const int sel = ::select(0, nullptr, &fdset, nullptr, &tv);
            if (sel > 0) {
                int err = 0; int elen = sizeof(err);
                getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &elen);
                if (err == 0 || err == WSAECONNREFUSED) {
                    int mss = 0; int mssLen = sizeof(mss);
                    if (getsockopt(sock, IPPROTO_TCP, TCP_MAXSEG, (char*)&mss, &mssLen) == 0 && mss > 0) {
                        discoveredMtu = mss + 40;   // MSS + IP(20) + TCP(20)
                        out.append(QStringLiteral("Reply from %1: MSS=%2 time=%3ms PMTU=%4")
                            .arg(displayAddr).arg(mss).arg((int)t.elapsed()).arg(discoveredMtu));
                    } else {
                        out.append(QStringLiteral("TCP connect succeeded but MSS not available."));
                    }
                }
            }
            closesocket(sock);
        }
    }
    if (discoveredMtu == 0) {
        discoveredMtu = 1500;
        out.append(QStringLiteral("PMTU TCP probe failed — using default MTU 1500."));
    }
#else
    if (resolvedIp) {
        // Best effort: probe TCP connect then read MSS via getsockopt (POSIX).
        const int sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock >= 0) {
            sockaddr_in addr; std::memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons((u_short)probePort);
            addr.sin_addr.s_addr = htonl(resolvedIp);
            // R5-7：阻塞 connect 对不可达主机可能挂数分钟（内核重传超时）；
            // 用 O_NONBLOCK + select 3s 限定，与 Windows 分支同策略。
            const int fl = ::fcntl(sock, F_GETFL, 0);
            ::fcntl(sock, F_SETFL, fl | O_NONBLOCK);
            QElapsedTimer t; t.start();
            const int rc = ::connect(sock, (sockaddr*)&addr, sizeof(addr));
            if (rc < 0 && errno == EINPROGRESS) {
                fd_set wfds; FD_ZERO(&wfds); FD_SET(sock, &wfds);
                timeval tv; tv.tv_sec = 3; tv.tv_usec = 0;
                const int sel = ::select(sock + 1, nullptr, &wfds, nullptr, &tv);
                if (sel > 0) {
                    int err = 0; socklen_t elen = sizeof(err);
                    getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &elen);
                    if (err != 0 && err != ECONNREFUSED) {
                        ::close(sock);
                        sock = -1;   // unreachable — skip MSS read
                    }
                } else {
                    ::close(sock);
                    sock = -1;       // connect timed out
                }
            }
            if (sock >= 0) {
                int mss = 0; socklen_t mssLen = sizeof(mss);
                if (getsockopt(sock, IPPROTO_TCP, TCP_MAXSEG, &mss, &mssLen) == 0 && mss > 0)
                    discoveredMtu = mss + 40;
                ::close(sock);
                if (discoveredMtu > 0)
                    out.append(QStringLiteral("Reply from %1: PMTU=%2 (%3ms)")
                        .arg(displayAddr).arg(discoveredMtu).arg((int)t.elapsed()));
            }
        }
    }
    if (discoveredMtu == 0) {
#if defined(__linux__)
        QDir netDir(QStringLiteral("/sys/class/net"));
        for (const auto& fi : netDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
            if (fi.fileName() == QLatin1String("lo")) continue;
            QFile f(QStringLiteral("/sys/class/net/%1/mtu").arg(fi.fileName()));
            if (f.open(QIODevice::ReadOnly)) {
                const int v = QString::fromLatin1(f.readAll().trimmed()).toInt();
                if (v > discoveredMtu && v < 10000) discoveredMtu = v;
            }
        }
#elif defined(__APPLE__) && !defined(PLATFORM_IOS)
        struct ifaddrs* ifa = nullptr;
        if (getifaddrs(&ifa) == 0) {
            for (struct ifaddrs* p = ifa; p; p = p->ifa_next) {
                if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
                const int s = ::socket(AF_INET, SOCK_DGRAM, 0);
                if (s < 0) continue;
                ifreq ifr; std::memset(&ifr, 0, sizeof(ifr));
                strncpy(ifr.ifr_name, p->ifa_name, IFNAMSIZ - 1);
                if (::ioctl(s, SIOCGIFMTU, &ifr) == 0 && ifr.ifr_mtu > discoveredMtu)
                    discoveredMtu = ifr.ifr_mtu;
                ::close(s);
            }
            freeifaddrs(ifa);
        }
#endif
        if (discoveredMtu == 0) discoveredMtu = 1500;
        out.append(QStringLiteral("PMTU TCP probe failed — using interface MTU %1.").arg(discoveredMtu));
    }
#endif

    DiagStatus status = targetResolved ? DiagStatus::Pass : DiagStatus::Warning;
    QString summary = QStringLiteral("MTU %1%2").arg(discoveredMtu)
        .arg(targetResolved ? QString() : QStringLiteral(" (local interface only — target unresolved)"));
    DiagnosticResult r = makeResult(id, status, summary, {}, out.join(QLatin1Char('\n')));
    r.data[QStringLiteral("mtu")] = discoveredMtu;
    r.data[QStringLiteral("targetResolved")] = targetResolved;
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G4IPv6Connectivity — AAAA resolution + TCP connect over IPv6
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeIPv6Connectivity(DiagId id, const QString& target, RunContext& ctx) {
    const QString host = extractHostname(target.isEmpty() ? QStringLiteral("ipv6.google.com") : target);
    QStringList out;
    out.append(QStringLiteral("IPv6 Connectivity Test"));
    out.append(QStringLiteral("Target: %1").arg(host));

    // Phase 1: AAAA resolution.
    QHostInfo info = QHostInfo::fromName(host);
    QStringList v6Addrs;
    if (info.error() == QHostInfo::NoError) {
        for (const QHostAddress& a : info.addresses())
            if (a.protocol() == QAbstractSocket::IPv6Protocol)
                v6Addrs.append(a.toString());
    }
    if (v6Addrs.isEmpty()) {
        out.append(QStringLiteral("DNS AAAA resolution FAILED — no IPv6 address for %1").arg(host));
        DiagnosticResult r = makeResult(id, DiagStatus::Warning,
            QStringLiteral("No IPv6 DNS resolution"), {}, out.join(QLatin1Char('\n')));
        r.data[QStringLiteral("dnsResolved")] = false;
        r.data[QStringLiteral("connectedCount")] = 0;
        r.data[QStringLiteral("totalPorts")] = 0;
        return r;
    }
    out.append(QStringLiteral("DNS AAAA: %1").arg(v6Addrs.join(QStringLiteral(", "))));

    // Phase 2: TCP connect over IPv6.
    static const struct { int port; const char* name; } kPorts[] = {
        {80, "HTTP"}, {443, "HTTPS"}, {22, "SSH"},
    };
    int connected = 0, failed = 0;
    QVariantList portsTested;
    for (const auto& tp : kPorts) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        bool ok = false;
        for (const QString& ip6 : v6Addrs) {
            QTcpSocket sock;
            sock.connectToHost(QHostAddress(ip6), (quint16)tp.port);
            if (sock.waitForConnected(3000)) {
                ok = true;
                sock.disconnectFromHost();
                break;
            }
        }
        if (ok) {
            out.append(QStringLiteral("TCP/%1 (%2): CONNECTED").arg(tp.port).arg(QLatin1String(tp.name)));
            ++connected;
        } else {
            out.append(QStringLiteral("TCP/%1 (%2): FAILED").arg(tp.port).arg(QLatin1String(tp.name)));
            ++failed;
        }
        QVariantMap pi;
        pi[QStringLiteral("port")] = tp.port;
        pi[QStringLiteral("service")] = QLatin1String(tp.name);
        pi[QStringLiteral("connected")] = ok;
        portsTested.append(pi);
    }
    out.append(QString());
    out.append(QStringLiteral("Result: %1/%2 ports reachable via IPv6").arg(connected).arg(connected + failed));

    DiagnosticResult r = makeResult(id, connected > 0 ? DiagStatus::Pass : DiagStatus::Fail,
        connected > 0 ? QStringLiteral("IPv6 reachable (%1/%2 ports)").arg(connected).arg(connected + failed)
                      : QStringLiteral("IPv6 unreachable (0/%1 ports)").arg(connected + failed),
        {}, out.join(QLatin1Char('\n')));
    r.data[QStringLiteral("host")] = host;
    r.data[QStringLiteral("ipv6Addresses")] = v6Addrs;
    r.data[QStringLiteral("dnsResolved")] = true;
    r.data[QStringLiteral("portsTested")] = portsTested;
    r.data[QStringLiteral("connectedCount")] = connected;
    r.data[QStringLiteral("failedCount")] = failed;
    r.data[QStringLiteral("totalPorts")] = connected + failed;
    return r;
}

} // namespace g4

// ── Registration (NEW-1: all six = All) ───────────────────────────────────
void registerG4Adapters() {
    using namespace PlatformFlag;
    AdapterRegistry::registerAdapters(DiagId::G4DnsResolution, {
        {PF_Desktop, "Desktop", {}, g4::probeDnsResolution},
        {PF_IOS,     "iOS",     {}, g4::probeDnsResolution},
        {PF_Android, "Android", {}, g4::probeDnsResolution},
    });
    AdapterRegistry::registerAdapters(DiagId::G4Ping, {
        {PF_Desktop, "Desktop", {}, g4::probePing},
        {PF_IOS,     "iOS",     {}, g4::probePing},
        {PF_Android, "Android", {}, g4::probePing},
    });
    AdapterRegistry::registerAdapters(DiagId::G4Traceroute, {
        {PF_Desktop, "Desktop", {}, g4::probeTraceroute},
        {PF_IOS,     "iOS",     {}, g4::probeTraceroute},
        {PF_Android, "Android", {}, g4::probeTraceroute},
    });
    AdapterRegistry::registerAdapters(DiagId::G4PathPing, {
        {PF_Desktop, "Desktop", {}, g4::probePathPing},
        {PF_IOS,     "iOS",     {}, g4::probePathPing},
        {PF_Android, "Android", {}, g4::probePathPing},
    });
    AdapterRegistry::registerAdapters(DiagId::G4MtuDiscovery, {
        {PF_Desktop, "Desktop", {}, g4::probeMtuDiscovery},
        {PF_IOS,     "iOS",     {}, g4::probeMtuDiscovery},
        {PF_Android, "Android", {}, g4::probeMtuDiscovery},
    });
    AdapterRegistry::registerAdapters(DiagId::G4IPv6Connectivity, {
        {PF_Desktop, "Desktop", {}, g4::probeIPv6Connectivity},
        {PF_IOS,     "iOS",     {}, g4::probeIPv6Connectivity},
        {PF_Android, "Android", {}, g4::probeIPv6Connectivity},
    });
}
