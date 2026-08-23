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
#if !defined(WINAPI_FAMILY)
#define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP
#endif
#if !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0601
#endif
#endif

#include "Common/Services/PlatformAdapter.h"
#include "Common/Services/DnsResolver.h"
#include "Common/Services/DnsWire.h"
#include "Common/Model/DiagnosticMeta.h"
#include "Common/Model/DiagNames.h"
#include "Diagnostics/View/DiagnosticFormatter.h"
#include "Diagnostics/Model/GHelpers.h"   // readProcLines（systemDnsServer 收敛）

#if defined(PLATFORM_IOS)
#include "Diagnostics/Model/G3/Platform/IOS/DnsResolve.h"
#endif
#if defined(PLATFORM_ANDROID)
#include "Diagnostics/Model/G5/Platform/Android/NetworkDiagnostics.h"
#endif

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
#if !defined(TCP_MAXSEG)
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
#include <netinet/tcp.h>
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
    // 5WHY (复核 2026-08-19 错误区块缺失): G5 的 makeResult 对 Fail/Warning/
    // Error 自动回填 errorOutput（摘要入错误区块），G4 无此行——失败的
    // Ping/Traceroute/MTU 等错误区块永不渲染（showError=true 契约落空）。
    // 对齐 G5 同一规则。
    if ((status == DiagStatus::Fail || status == DiagStatus::Warning
         || status == DiagStatus::Error) && r.errorOutput.isEmpty())
        r.errorOutput = summary;
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
    // H4：阻塞式 QHostInfo::fromName 在坏网络下可挂数十秒——统一走
    // DnsResolver 单例（3s 超时 + 线程安全缓存）。
    return DnsResolver::resolveIPv4(host, timeoutMs);
}

#if defined(_WIN32)
static QString ip4ToStr(quint32 ipHostOrder) {
    in_addr a; a.S_un.S_addr = htonl(ipHostOrder);
    return QString::fromLatin1(inet_ntoa(a));
}
#else
#if defined(__APPLE__) || defined(__linux__)
static QString ip4ToStr(quint32 ipHostOrder) {
    in_addr a; a.s_addr = htonl(ipHostOrder);
    return QString::fromLatin1(inet_ntoa(a));
}
#endif
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
    // 5WHY (复核 2026-08-21 procfs 收敛): 共享 readProcLines（与 G1/G2/G3/G5
    // 同源）——本函数为 G4 各探针提供默认 DNS 服务器，atEnd 陷阱曾使
    // G4 静默回退 8.8.8.8 测试错误解析器。
    for (const QString& raw : SystemDiagnostics::readProcLines(QStringLiteral("/etc/resolv.conf"))) {
        const QString line = raw.trimmed();
        if (line.startsWith(QLatin1String("nameserver ")) && server.isEmpty())
            server = line.mid(11);
    }
#if defined(_WIN32)
    if (server.isEmpty()) server = QStringLiteral("223.5.5.5");
#else
    if (server.isEmpty()) server = QStringLiteral("8.8.8.8");
#endif
    return server;
}

// RCODE → dig 风格状态名。5WHY (复核 2026-08-20 单一来源): 头部与摘要
// 卡各有一条 rcode 三元链——新增映射（如 REFUSED）漏改一处即两卡对同一
// 响应说法不一。收敛为本文件单一 helper（formatter 只收文本不译码）。
static QString rcodeText(int rcode) {
    switch (rcode) {
    case 0:  return QStringLiteral("NOERROR");
    case 1:  return QStringLiteral("FORMERR");
    case 2:  return QStringLiteral("SERVFAIL");
    case 3:  return QStringLiteral("NXDOMAIN");
    case 5:  return QStringLiteral("REFUSED");
    default: return QStringLiteral("UNKNOWN");
    }
}

// ═════════════════════════════════════════════════════════════════════════
// G4DnsResolution — dig-like output (A/AAAA/CNAME)
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeDnsResolution(DiagId id, const QString& target, RunContext& ctx) {
    const QString host = extractHostname(target.isEmpty() ? QStringLiteral("example.com") : target);
    const QString server = systemDnsServer();
    const dnsWire::Answer ans = dnsWire::udpQuery(host, 1, server, 3000);
    const int ms = ans.elapsedMs;
    const int rcode = ans.rcode;
    if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
    // rcode=-1 ⇒ parseResponse 从未运行 ⇒ 连接失败/超时。dig 在此仅输出
    // 一行 "connection timed out"——不伪造 header（"Got answer"/假 id 4660）
    // 与 footer（"MSG SIZE rcvd: 0"）。5WHY (复核 2026-08-20 失败伪造)。
    const bool gotResponse = rcode >= 0;
    int anCount = 0;
    QStringList ips;
    QStringList out;
    if (!gotResponse) {
        out.append(QStringLiteral(";; connection timed out; no servers could be reached"));
        DiagnosticResult r = makeResult(id, DiagStatus::Fail,
            QStringLiteral("No response from %1").arg(server), {}, out.join(QLatin1Char('\n')));
        r.data[QStringLiteral("queryTimeMs")] = ms;
        r.data[QStringLiteral("rcode")] = -1;
        r.data[QStringLiteral("answerCount")] = 0;
        r.narrative = QStringLiteral("Sent an A query for %1 to %2 but no DNS response arrived within 3 s. "
            "The server may be unreachable, silently dropping UDP port 53, or the path to it is down. "
            "No dig-style header is shown because no response was received.")
            .arg(host, server);
        r.data[QStringLiteral("narrativeKey")] = QStringLiteral("nDnsTimeout");
        r.data[QStringLiteral("narrativeArgs")] = QVariantList{ host, server };
        return r;
    }
    // 5WHY (2026-08-20 用户诉求 "输出与 dig 有距离"): 曾硬编码
    // "flags: qr rd ra" 与 ANSWER 计数——真实响应头的 aa/tc/ra 位与
    // AUTHORITY/ADDITIONAL 计数丢失，空应答时也打印空的 ANSWER SECTION
    // 标题（dig 只在该节非空时输出标题）。改为响应头真实 flags 字 +
    // 真实计数；ANSWER SECTION 标题仅在存在应答记录时输出（dig 行为）。
    QString flags;
    if (ans.flags & 0x8000) flags += QStringLiteral("qr ");
    if (ans.flags & 0x0400) flags += QStringLiteral("aa ");
    if (ans.flags & 0x0200) flags += QStringLiteral("tc ");
    if (ans.flags & 0x0100) flags += QStringLiteral("rd ");
    if (ans.flags & 0x0080) flags += QStringLiteral("ra ");
    flags = flags.trimmed();
    // 5WHY (复核 2026-08-20 flags 去伪造): 曾 flags 空时回填 "qr rd"——
    // 响应头无任何标志位（QR 位清零 = 非真实 DNS 应答，如劫持网关代答）
    // 被渲染成正常应答，协议异常不可见。空即空，formatter 呈现
    // "(unparsed)"（诚实标记而非伪造）。
    out.append(DiagnosticFormatter::formatDnsHeader(host, rcodeText(rcode),
        ans.id, ans.anCount, flags, ans.nsCount, ans.arCount));
    out.append(QStringLiteral(";; QUESTION SECTION:"));
    out.append(DiagnosticFormatter::formatDnsQuestion(host, QStringLiteral("A")));
    out.append(QString());
    if (!ans.records.isEmpty()) {
        out.append(QStringLiteral(";; ANSWER SECTION:"));
        for (const auto& rec : ans.records) {
            if (rec.type == 1 || rec.type == 28) {
                out.append(DiagnosticFormatter::formatDnsRecord(host, rec.ttl,
                    rec.type == 1 ? QStringLiteral("A") : QStringLiteral("AAAA"), rec.value));
                ips.append(rec.value);
                ++anCount;
            } else if (rec.type == 5) {
                out.append(DiagnosticFormatter::formatDnsRecord(host, rec.ttl, QStringLiteral("CNAME"), rec.value));
            }
        }
        // 5WHY (复核 2026-08-20 计数一致): 头部 ANSWER 用真实头计数
        // （ans.anCount），节内只渲染已解析类型（A/AAAA/CNAME）——响应含
        // MX/TXT/OPT 等未解析类型时两处计数不一致（dig 渲染全部类型，
        // 本解析器不渲染未知 RR）。差异以注释行显式声明，不做静默截断。
        if (int(ans.anCount) > ans.records.size())
            out.append(QStringLiteral(";; (%1 additional record(s) of other types not shown)")
                .arg(int(ans.anCount) - ans.records.size()));
        out.append(QString());
    }
    out.append(DiagnosticFormatter::formatDnsFooter(ms, server, ans.msgSize));

    DiagStatus status = anCount > 0 ? DiagStatus::Pass : DiagStatus::Fail;
    // 5WHY (复核 2026-08-21 第三份 rcode 链): 曾此处再写一条 rcode==3 三元
    // ——与头部共用的 rcodeText 单一映射脱钩（加 REFUSED 语义即两处说法
    // 分叉）。判定仍用 rcode 布尔，名称走 rcodeText。
    QString summary = anCount > 0
        ? QStringLiteral("Resolved to %1 (%2 ms)").arg(ips.join(QStringLiteral(", "))).arg(ms)
        : (rcode == 3 ? QStringLiteral("%1 — no such domain").arg(rcodeText(rcode))
                      : QStringLiteral("No A records"));

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
    // 摘要卡叙述：查询服务器/响应码/应答数与延迟结论
    r.narrative = QStringLiteral("Query %1 (A) against %2 returned %3 with %4 answer record(s) in %5 ms. ")
        .arg(host, server, rcodeText(rcode), QString::number(anCount), QString::number(ms))
        + (anCount > 0 ? QStringLiteral("Resolved address(es): %1. Full dig-style output is in the terminal section.")
            .arg(ips.join(QStringLiteral(", ")))
                       : QStringLiteral("No A records were returned for this name."));
    if (anCount > 0) {
        r.data[QStringLiteral("narrativeKey")] = QStringLiteral("nDnsResolve");
        r.data[QStringLiteral("narrativeArgs")] = QVariantList{ host, server,
            rcodeText(rcode), QString::number(anCount), QString::number(ms), ips.join(QStringLiteral(", ")) };
    } else {
        r.data[QStringLiteral("narrativeKey")] = QStringLiteral("nDnsNoRec");
        r.data[QStringLiteral("narrativeArgs")] = QVariantList{ host, server,
            rcodeText(rcode), QString::number(ms) };
    }
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
#else
#if defined(__APPLE__)
        if (resolvedIp)
            ms = icmpEchoRttMsApple(resolvedIp, i + 1, 2000);
#else
#if defined(__linux__) && !defined(__ANDROID__)
        if (resolvedIp)
            ms = icmpEchoRttMsLinux(resolvedIp, i + 1, 2000);
#endif
#endif
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
    // 摘要卡叙述（可达性先行 → 延迟/丢包）：用户诉求顺序约定
    const QString reachLine = loss >= 100.0
        ? QStringLiteral("Target %1 is UNREACHABLE (100%% of %2 probe(s) lost).").arg(host).arg(sent)
        : QStringLiteral("Target %1 is reachable.").arg(host);
    r.narrative = reachLine + QLatin1Char(' ')
        + QStringLiteral("Sent %1 packet(s), received %2 — %3%% loss%4. ")
            .arg(sent).arg(rcvd).arg(loss, 0, 'f', 1)
            .arg(tcpFallback ? QStringLiteral(" (TCP fallback probe)") : QString())
        + (rcvd > 0
            ? QStringLiteral("Latency: average %1 ms (min %2 / max %3), jitter %4 ms.")
                .arg(avg, 0, 'f', 0).arg(minMs, 0, 'f', 0).arg(maxMs, 0, 'f', 0).arg(jitter, 0, 'f', 1)
                // 5WHY (2026-08-23 报告样本一致性 D4): VPN/中间盒代答场景下
                // RTT 恒整 0 ms 曾无任何解释（真实样本 "avg 0.0ms"）——补
                // 诚实脚注，避免用户误读为"零延迟"。
                + (avg < 0.5
                    ? QStringLiteral(" Sub-millisecond round trip — replies were likely answered "
                        "locally by a middlebox (VPN/CDN offload); treat as <1 ms.")
                    : QString())
            : QStringLiteral("No responses received — latency unknown."));
    if (loss >= 100.0) {
        r.data[QStringLiteral("narrativeKey")] = QStringLiteral("nPingUnreach");
        r.data[QStringLiteral("narrativeArgs")] = QVariantList{ host, QString::number(sent) };
    } else if (rcvd > 0) {
        r.data[QStringLiteral("narrativeKey")] = QStringLiteral("nPingReach");
        r.data[QStringLiteral("narrativeArgs")] = QVariantList{ host,
            QString::number(sent), QString::number(rcvd), QString::number(loss, 'f', 1),
            QString::number(avg, 'f', 0), QString::number(minMs, 'f', 0), QString::number(maxMs, 'f', 0),
            QString::number(jitter, 'f', 1),
            tcpFallback ? QStringLiteral(" · TCP fallback probe") : QString() };
    }
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
#else
#if defined(__linux__) && !defined(__ANDROID__)
        const int res = traceHopLinux(targetIp, ttl, rttMs, hopIp);
#else
#if defined(__APPLE__)
        const int res = traceHopMac(targetIp, ttl, rttMs, hopIp);
#else
        // Android：无 raw ICMP → TCP-TTL（内核可能不遵从 TTL——诚实局限）。
        const int res = tcpTtlHop(targetIp, ttl, 443, rttMs, hopIp);
#endif
#endif
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
                // 5WHY (simplify 2026-08-17): 原 fromName 同步无界（30-120s/跳，
                // 违反本文件 H4 规则）；中间版本每跳新建局部事件循环（第 3 份
                // 有界等待机制且无负缓存）。统一走 DnsResolver 单例：
                // 受守卫线程/轮询 + 2s 截止 + 正负缓存。
                const QString name = DnsResolver::instance().resolvePtr(hopIp, 500);   // 展示用：0.5s 截止
                if (!name.isEmpty())
                    hopName = name;
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
    // 摘要卡叙述：是否到达目标 → 跳数 → 路径状态
    r.narrative = reached
        ? QStringLiteral("Route to %1 complete: %2 hop(s), target reached. Per-hop RTT is in the terminal trace below.")
            .arg(host).arg(hopCount)
        : blocked
            ? QStringLiteral("Route to %1 BLOCKED at hop %2 — a router/firewall filtered the probes.").arg(host).arg(hopCount)
            : tcpReachable
                ? QStringLiteral("Route to %1 incomplete (%2 hop(s)) — ICMP filtered, but the target is reachable via TCP.").arg(host).arg(hopCount)
                : QStringLiteral("Route to %1 incomplete after %2 hop(s) — the target may be firewalled.").arg(host).arg(hopCount);
    r.data[QStringLiteral("narrativeKey")] = reached ? QStringLiteral("nTraceComplete")
        : blocked ? QStringLiteral("nTraceBlocked")
        : tcpReachable ? QStringLiteral("nTraceTcp")
        : QStringLiteral("nTraceIncomplete");
    r.data[QStringLiteral("narrativeArgs")] = QVariantList{ host, QString::number(hopCount) };
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

    struct HopEntry { int ttl; QString ip; QString name; bool reached; int rttMs = 0; };
    QVector<HopEntry> entries;
    // C5：直接用 traceroute 结果的 data["hops"]（含每跳 rttMs），
    // 不再正则解析 rawOutput——契约键 hops[].rttMs 从此有值。
    const QVariantList trHops = tr.data.value(QStringLiteral("hops")).toList();
    int hopCount = 0;
    bool reached = false;
    for (const QVariant& hv : trHops) {
        const QVariantMap hm = hv.toMap();
        const int ttlNum = hm.value(QStringLiteral("ttl")).toInt();
        const QString hopIp = hm.value(QStringLiteral("ip")).toString();
        const bool isTarget = (hopIp == targetIpStr);
        if (isTarget) reached = true;
        entries.append({ttlNum, hopIp, hm.value(QStringLiteral("name")).toString(),
                        isTarget, hm.value(QStringLiteral("rttMs")).toInt()});
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
        // 5WHY (2026-08-20 诚实呈现): 中间跳从未被 ping（stats 初始化
        // sent=4,rcvd=0,loss=100），曾统一打印 "0/4 = 0%"——伪称发了 4 包
        // 且交付 0%。仅末跳有真实统计；中间跳只列 IP，不再伪造计数行。
        if (isFinal) {
            lines.append(QStringLiteral("  %1  %2  %3/%4 = %5%  avg %6ms")
                .arg(ttlNum, 2).arg(ip, -16)
                .arg(hs.rcvd).arg(hs.sent).arg(100.0 - hs.loss, 0, 'f', 0)
                .arg(hs.avgMs));
        } else {
            lines.append(QStringLiteral("  %1  %2").arg(ttlNum, 2).arg(ip, -16));
        }
        QVariantMap h;
        h[QStringLiteral("ttl")] = ttlNum;
        h[QStringLiteral("ip")] = ip;
        // 5WHY (复核 2026-08-20 数据契约诚实): 中间跳从未被 ping——曾以
        // sent=0/loss=100/avg=0 落数据契约，把"未探测"伪装成"0 发包
        // 100% 丢包 0ms"（机器可读的伪造）。中间跳丢包/均值用 -1 哨兵
        // （未知），sent/received=0 语义为"未探测"（非 0 交付）。
        h[QStringLiteral("sent")] = isFinal ? hs.sent : 0;
        h[QStringLiteral("received")] = isFinal ? hs.rcvd : 0;
        h[QStringLiteral("lossPercent")] = isFinal ? hs.loss : -1.0;
        h[QStringLiteral("avgMs")] = isFinal ? hs.avgMs : -1;
        // C5：契约键 hops[].rttMs（kPathContract BarChart）——最终跳用 ping 均值，
        // 中间跳用 traceroute 探测 RTT
        h[QStringLiteral("rttMs")] = isFinal ? hs.avgMs : entries.value(i + 1).rttMs;
        h[QStringLiteral("reached")] = entries.value(i + 1).reached;
        hopData.append(h);
    }

    DiagStatus status = reached ? DiagStatus::Pass : DiagStatus::Warning;
    const double finalLoss = stats.isEmpty() ? 100.0 : stats.last().loss;
    // 5WHY (2026-08-23 报告样本一致性 D1, review/ui-ux-audit-plan §5.2): summary
    // 曾把交付率标成 "loss"——"final-hop loss 0%" 实为 delivery 100%，与叙述
    // "(loss 100%)" 同屏自相矛盾（真实样本实证）。统一 delivery 语式；reached
    // 但末跳零应答（统计不可信）时补诚实脚注而非让两处各说各话。
    QString summary = reached
        ? QStringLiteral("%1 hops, final-hop delivery %2%").arg(hopCount).arg(100.0 - finalLoss, 0, 'f', 0)
        : QStringLiteral("Incomplete — %1 hops").arg(hopCount);

    DiagnosticResult r = makeResult(id, status, summary, {}, lines.join(QLatin1Char('\n')));
    r.data[QStringLiteral("hopCount")] = hopCount;
    r.data[QStringLiteral("hops")] = hopData;
    r.data[QStringLiteral("reached")] = reached;
    // 摘要卡叙述：可达性 → 跳数 → 末跳丢包
    r.narrative = reached
        ? QStringLiteral("Path to %1 traverses %2 hop(s); final-hop packet delivery %3%% (loss %4%%). ")
            .arg(host).arg(hopCount).arg(100.0 - finalLoss, 0, 'f', 0).arg(finalLoss, 0, 'f', 0)
            + (finalLoss >= 99.9
                ? QStringLiteral("The end-to-end delivery probe received no replies — loss statistics "
                    "are inconclusive on this path (probes may be filtered).")
                : QStringLiteral("Per-hop loss/RTT is in the terminal section."))
        : QStringLiteral("Path to %1 incomplete after %2 hop(s) — intermediate hops stopped responding. "
            "Per-hop details are in the terminal section.").arg(host).arg(hopCount);
    if (reached && finalLoss < 99.9) {
        r.data[QStringLiteral("narrativeKey")] = QStringLiteral("nPathPingOk");
        r.data[QStringLiteral("narrativeArgs")] = QVariantList{ host, QString::number(hopCount),
            QString::number(100.0 - finalLoss, 'f', 0), QString::number(finalLoss, 'f', 0) };
    } else if (reached) {
        r.data[QStringLiteral("narrativeKey")] = QStringLiteral("nPathPingNoReply");
        r.data[QStringLiteral("narrativeArgs")] = QVariantList{ host, QString::number(hopCount) };
    } else {
        r.data[QStringLiteral("narrativeKey")] = QStringLiteral("nPathPingIncomplete");
        r.data[QStringLiteral("narrativeArgs")] = QVariantList{ host, QString::number(hopCount) };
    }
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
        int sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
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
                // 5WHY（Apple 构建失败）：Darwin 不定义 TCP_MAXSEG（无 getsockopt
                // MSS 读取），iOS/macOS CI 编译报错。仅在宏存在时读取；
                // 否则回落探测尺寸路径。
#if defined(TCP_MAXSEG)
                int mss = 0; socklen_t mssLen = sizeof(mss);
                if (getsockopt(sock, IPPROTO_TCP, TCP_MAXSEG, &mss, &mssLen) == 0 && mss > 0)
                    discoveredMtu = mss + 40;
#endif
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
#else
#if defined(__APPLE__) && !defined(PLATFORM_IOS)
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
    r.data[QStringLiteral("mss")] = discoveredMtu > 40 ? discoveredMtu - 40 : 0;
    r.data[QStringLiteral("effectiveMss")] = discoveredMtu > 40 ? discoveredMtu - 40 : 0;
    r.data[QStringLiteral("probePort")] = probePort;
    r.data[QStringLiteral("tcpProbeSuccessful")] = resolvedIp != 0 && targetResolved;
    // M10：mtuQuality 分级提示（巨帧风险 / IPv6 下限）
    QString quality = QStringLiteral("standard");
    if (discoveredMtu > 1500) quality = QStringLiteral("jumbo");
    else if (discoveredMtu < 1280) quality = QStringLiteral("below-ipv6-min");
    r.data[QStringLiteral("mtuQuality")] = quality;
    r.data[QStringLiteral("targetResolved")] = targetResolved;
    // 摘要卡叙述：MTU 值 → MSS → 质量分级/探测方式
    r.narrative = QStringLiteral("Path MTU to %1 is %2 bytes (effective MSS %3). ")
        .arg(host).arg(discoveredMtu).arg(discoveredMtu > 40 ? discoveredMtu - 40 : 0)
        + (targetResolved
            ? (quality == QLatin1String("jumbo")
                ? QStringLiteral("Jumbo frame (>1500) — fragmentation risk across legacy links.")
                : quality == QLatin1String("below-ipv6-min")
                    ? QStringLiteral("Below the IPv6 minimum (1280) — IPv6 tunnels may break.")
                    : QStringLiteral("Standard Ethernet MTU — no fragmentation expected."))
            : QStringLiteral("Target did not resolve — the value is the local interface MTU only."));
    r.data[QStringLiteral("narrativeKey")] = !targetResolved ? QStringLiteral("nMtuLocal")
        : quality == QLatin1String("jumbo") ? QStringLiteral("nMtuJumbo")
        : quality == QLatin1String("below-ipv6-min") ? QStringLiteral("nMtuLow")
        : QStringLiteral("nMtuStd");
    r.data[QStringLiteral("narrativeArgs")] = QVariantList{ host,
        QString::number(discoveredMtu), QString::number(discoveredMtu > 40 ? discoveredMtu - 40 : 0) };
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

    // Phase 1: AAAA resolution — DnsResolver 3s 超时（H4）
    QStringList v6Addrs;
    const QString v6 = DnsResolver::instance().resolve6(host, 3000);
    // 5WHY (2026-08-23 报告样本一致性 D2, review/ui-ux-audit-plan §5.2):
    // ::ffff:x.x.x.x 是 IPv4-mapped 地址——曾按原生 IPv6 计数并 connect，
    // 把 IPv4 连通误报成 "IPv6 reachable"（真实样本：AAAA=::ffff:198.18.0.246
    // 判 PASS 3/3）。映射地址剔除；仅有映射记录 = 无原生 IPv6，走独立诚实
    // 叙述而非伪造可达。
    if (v6.isEmpty()) {
        out.append(QStringLiteral("DNS AAAA resolution FAILED — no IPv6 address for %1").arg(host));
        DiagnosticResult r = makeResult(id, DiagStatus::Warning,
            QStringLiteral("No IPv6 DNS resolution"), {}, out.join(QLatin1Char('\n')));
        r.data[QStringLiteral("dnsResolved")] = false;
        r.data[QStringLiteral("connectedCount")] = 0;
        r.data[QStringLiteral("totalPorts")] = 0;
        // 5WHY (2026-08-20 用户诉求 "G4 各项目需摘要卡"): AAAA 解析失败
        // 分支曾直接 return——摘要卡（narrative 为空则不渲染）缺失，详情页
        // 只剩错误区块，无推导叙述。补与其余分支同构的叙述。
        r.narrative = QStringLiteral("Host %1 returned no IPv6 (AAAA) records — the network or the host "
            "does not publish IPv6, or IPv6 DNS is unavailable. Connectivity over IPv6 was not tested. "
            "IPv4 services are unaffected by this result.").arg(host);
        r.data[QStringLiteral("narrativeKey")] = QStringLiteral("nIPv6NoRecord");
        r.data[QStringLiteral("narrativeArgs")] = QVariantList{ host };
        return r;
    }
    if (v6.startsWith(QLatin1String("::ffff:"), Qt::CaseInsensitive)) {
        out.append(QStringLiteral("DNS AAAA: %1 (IPv4-mapped record — ignored)").arg(v6));
        DiagnosticResult r = makeResult(id, DiagStatus::Warning,
            QStringLiteral("No native IPv6 (IPv4-mapped AAAA only)"), {}, out.join(QLatin1Char('\n')));
        r.data[QStringLiteral("dnsResolved")] = true;
        r.data[QStringLiteral("nativeIpv6")] = false;
        r.data[QStringLiteral("connectedCount")] = 0;
        r.data[QStringLiteral("totalPorts")] = 0;
        r.narrative = QStringLiteral("Host %1 publishes only an IPv4-mapped AAAA record (%2), "
            "which represents an IPv4 address — there is no native IPv6 address to test. "
            "The port probes were skipped rather than reporting IPv4 results as IPv6 reachability.")
            .arg(host).arg(v6);
        r.data[QStringLiteral("narrativeKey")] = QStringLiteral("nIPv6Mapped");
        r.data[QStringLiteral("narrativeArgs")] = QVariantList{ host, v6 };
        return r;
    }
    v6Addrs.append(v6);
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
    // 摘要卡叙述：AAAA 解析 → 地址数 → 端口连通结论
    r.narrative = QStringLiteral("Host %1 resolves to %2 IPv6 address(es). ")
        .arg(host).arg(v6Addrs.size())
        + (connected > 0
            ? QStringLiteral("IPv6 is REACHABLE: %1/%2 test port(s) connected over IPv6.").arg(connected).arg(connected + failed)
            : QStringLiteral("IPv6 is UNREACHABLE: 0/%1 test port(s) connected — the network may not provide IPv6.").arg(connected + failed));
    r.data[QStringLiteral("narrativeKey")] = connected > 0 ? QStringLiteral("nIPv6Ok") : QStringLiteral("nIPv6Fail");
    r.data[QStringLiteral("narrativeArgs")] = connected > 0
        ? QVariantList{ host, QString::number(v6Addrs.size()), QString::number(connected), QString::number(connected + failed) }
        : QVariantList{ host, QString::number(v6Addrs.size()), QString::number(connected + failed) };
    return r;
}

} // namespace g4

// ── Registration (NEW-1: all six = All) ───────────────────────────────────
void registerG4Adapters() {
    using namespace PlatformFlag;
    // G4DnsResolution：iOS 走 iosDnsResolve（res_9 系统解析器），Android 走
    // androidDnsDiag（JNI InetAddress），桌面走 dnsWire 原始 DNS。
#if defined(PLATFORM_IOS)
    AdapterRegistry::registerAdapters(DiagId::G4DnsResolution, {
        {PF_IOS, "iOS", {}, [](DiagId i, const QString& t, RunContext&) { return iosDnsResolve(i, t, 3000); }},
    });
#else
#if defined(PLATFORM_ANDROID)
    AdapterRegistry::registerAdapters(DiagId::G4DnsResolution, {
        {PF_Android, "Android", {}, [](DiagId i, const QString& t, RunContext&) { return androidDnsDiag(i, t); }},
    });
#endif
#endif
    AdapterRegistry::registerAdapters(DiagId::G4DnsResolution, {
        {PF_Desktop, "Desktop", {}, g4::probeDnsResolution},
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
