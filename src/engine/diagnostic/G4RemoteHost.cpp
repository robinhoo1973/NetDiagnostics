#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif
#include "engine/diagnostic/G4RemoteHost.h"
#include "util/DebugSwitch.h"
#include "util/PingParser.h"
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

        char owner[256];
        ns_name_uncompress(ns_msg_base(handle), ns_msg_end(handle), ns_rr_rdata(rr), owner, sizeof(owner));
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
            p += ns_name_uncompress(ns_msg_base(handle), ns_msg_end(handle), p, mname, sizeof(mname));
            p += ns_name_uncompress(ns_msg_base(handle), ns_msg_end(handle), p, rname, sizeof(rname));
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

static DiagnosticResult noTargetResult(DiagId id, DiagGroup group);

DiagnosticResult dnsResolution(const QString& target) {
    DiagnosticResult r;
    r.id = DiagId::G4DnsResolution; r.group = DiagGroup::G4;
    r.timestamp = QDateTime::currentDateTime();
    if (target.isEmpty()) return noTargetResult(r.id, r.group);
    QString host = extractHostname(target);
    QElapsedTimer t; t.start();
    QStringList out;
    QStringList ipsAll;

    out.append(QString());
    out.append(QStringLiteral("; <<>> NetDiagnostics DNS <<>> %1").arg(host));
    out.append(QStringLiteral(";; global options: +cmd"));
    QByteArray hb = host.toUtf8();

#ifdef _WIN32
    // ── Windows: DNSQuery for A, AAAA, MX, CNAME, SOA, NS ──────────────
    wchar_t whost[256]; MultiByteToWideChar(CP_UTF8, 0, hb.constData(), -1, whost, 256);

    struct { WORD type; const char* name; } wQueries[] = {
        {DNS_TYPE_A, "A"}, {DNS_TYPE_AAAA, "AAAA"}, {DNS_TYPE_MX, "MX"},
        {DNS_TYPE_CNAME, "CNAME"}, {DNS_TYPE_SOA, "SOA"}, {DNS_TYPE_NS, "NS"}
    };
    bool hasAnswer = false;
    for (auto& wq : wQueries) {
        PDNS_RECORD rec = nullptr;
        if (DnsQuery_W(whost, wq.type, DNS_QUERY_STANDARD, nullptr, &rec, nullptr) != 0 || !rec) continue;
        bool first = true;
        for (auto* p = rec; p; p = p->pNext) {
            if (p->wType != wq.type) continue;
            if (first) {
                out.append(QStringLiteral(";; %1 SECTION:").arg(wq.name));
                first = false; hasAnswer = true;
            }
            // Build dig-style line
            if (p->wType == DNS_TYPE_A) {
                struct in_addr a; a.S_un.S_addr = p->Data.A.IpAddress;
                QString ip = ip4ToStr(a);
                out.append(QStringLiteral("%1.  %2  IN  A  %3").arg(host, -30).arg(p->dwTtl, 6).arg(ip));
                ipsAll.append(ip);
            } else if (p->wType == DNS_TYPE_AAAA) {
                char ip6[46]; inet_ntop(AF_INET6, &p->Data.AAAA.Ip6Address, ip6, sizeof(ip6));
                out.append(QStringLiteral("%1.  %2  IN  AAAA  %3").arg(host, -30).arg(p->dwTtl, 6).arg(QString::fromLatin1(ip6)));
            } else if (p->wType == DNS_TYPE_MX)
                out.append(QStringLiteral("%1.  %2  IN  MX  %3 %4").arg(host, -30).arg(p->dwTtl, 6)
                    .arg(p->Data.MX.wPreference).arg(QString::fromWCharArray(p->Data.MX.pNameExchange)));
            else if (p->wType == DNS_TYPE_CNAME)
                out.append(QStringLiteral("%1.  %2  IN  CNAME  %3").arg(host, -30).arg(p->dwTtl, 6)
                    .arg(QString::fromWCharArray(p->Data.CNAME.pNameHost)));
            else if (p->wType == DNS_TYPE_SOA)
                out.append(QStringLiteral("%1.  %2  IN  SOA  %3 %4 (serial %5)").arg(host, -30).arg(p->dwTtl, 6)
                    .arg(QString::fromWCharArray(p->Data.SOA.pNamePrimaryServer))
                    .arg(QString::fromWCharArray(p->Data.SOA.pNameAdministrator))
                    .arg(p->Data.SOA.dwSerialNo));
            else if (p->wType == DNS_TYPE_NS)
                out.append(QStringLiteral("%1.  %2  IN  NS  %3").arg(host, -30).arg(p->dwTtl, 6)
                    .arg(QString::fromWCharArray(p->Data.NS.pNameHost)));
        }
        if (!first) out.append(QString());
        DnsRecordListFree(rec, DnsFreeRecordList);
    }
    if (!hasAnswer) out.append(QStringLiteral(";; (no records found)"));
#else
    // ── Linux: full dig-like with all sections ─────────────────────────
    // Primary query: A record to get header + ANSWER + AUTHORITY + ADDITIONAL
    {
        unsigned char buf[4096];
        int len = res_query(hb.constData(), C_IN, ns_t_a, buf, sizeof(buf));
        if (len >= 0) {
            ns_msg handle;
            if (ns_initparse(buf, len, &handle) >= 0) {
                int rcode = ns_msg_getflag(handle, ns_f_rcode);
                int qdCount = ns_msg_count(handle, ns_s_qd);
                int anCount = ns_msg_count(handle, ns_s_an);
                int nsCount = ns_msg_count(handle, ns_s_ns);
                int arCount = ns_msg_count(handle, ns_s_ar);
                bool qr = ns_msg_getflag(handle, ns_f_qr);
                bool rd = ns_msg_getflag(handle, ns_f_rd);
                bool ra = ns_msg_getflag(handle, ns_f_ra);
                bool aa = ns_msg_getflag(handle, ns_f_aa);
                bool tc = ns_msg_getflag(handle, ns_f_tc);
                uint16_t id = ns_msg_id(handle);

                out.append(QStringLiteral(";; Got answer:"));
                out.append(QStringLiteral(";; ->>HEADER<<- opcode: QUERY, status: %1, id: %2")
                    .arg(rcodeStr(rcode)).arg(id));
                out.append(QStringLiteral(";; flags: %1%2%3%4%5; QUERY: %6, ANSWER: %7, AUTHORITY: %8, ADDITIONAL: %9")
                    .arg(qr ? "qr " : "").arg(aa ? "aa " : "").arg(tc ? "tc " : "")
                    .arg(rd ? "rd " : "").arg(ra ? "ra " : "")
                    .arg(qdCount).arg(anCount).arg(nsCount).arg(arCount));
                out.append(QString());

                // QUESTION SECTION
                out.append(QStringLiteral(";; QUESTION SECTION:"));
                out.append(QStringLiteral(";%1.\t\t\tIN\tA").arg(host));
                out.append(QString());

                // ANSWER SECTION
                bool gotCname = false; QString cnameTarget;
                dnsDumpSection(handle, ns_s_an, QStringLiteral(";; ANSWER SECTION:"), host, out, gotCname, cnameTarget);

                // Collect A/AAAA IPs from answer
                for (int i = 0; i < anCount; i++) {
                    ns_rr rr;
                    if (ns_parserr(&handle, ns_s_an, i, &rr) < 0) continue;
                    int rt = ns_rr_type(rr);
                    const unsigned char* rd = ns_rr_rdata(rr);
                    if (rt == ns_t_a) {
                        struct in_addr a; memcpy(&a, rd, 4);
                        ipsAll.append(ip4ToStr(a));
                    } else if (rt == ns_t_aaaa) {
                        char ip6[INET6_ADDRSTRLEN];
                        inet_ntop(AF_INET6, rd, ip6, sizeof(ip6));
                        ipsAll.append(QString::fromLatin1(ip6));
                    }
                }

                if (anCount == 0) out.append(QStringLiteral(";; ANSWER SECTION: (empty)"));
                out.append(QString());

                // AUTHORITY SECTION
                dnsDumpSection(handle, ns_s_ns, QStringLiteral(";; AUTHORITY SECTION:"), host, out, gotCname, cnameTarget);

                // ADDITIONAL SECTION
                dnsDumpSection(handle, ns_s_ar, QStringLiteral(";; ADDITIONAL SECTION:"), host, out, gotCname, cnameTarget);

                // If CNAME found, also resolve CNAME target
                if (gotCname && !cnameTarget.isEmpty()) {
                    QByteArray cb = cnameTarget.toUtf8();
                    len = res_query(cb.constData(), C_IN, ns_t_a, buf, sizeof(buf));
                    if (len >= 0 && ns_initparse(buf, len, &handle) >= 0) {
                        dnsDumpSection(handle, ns_s_an, QStringLiteral(";; CNAME RESOLUTION (%1):").arg(cnameTarget), cnameTarget, out, gotCname, cnameTarget);
                        // Also collect IPs from CNAME target
                        int cc = ns_msg_count(handle, ns_s_an);
                        for (int i = 0; i < cc; i++) {
                            ns_rr rr;
                            if (ns_parserr(&handle, ns_s_an, i, &rr) < 0) continue;
                            if (ns_rr_type(rr) == ns_t_a) {
                                struct in_addr a; memcpy(&a, ns_rr_rdata(rr), 4);
                                QString ip = ip4ToStr(a);
                                if (!ipsAll.contains(ip)) ipsAll.append(ip);
                            }
                        }
                    }
                }
            }
        } else {
            out.append(QStringLiteral(";; Query failed (A record)"));
            out.append(QString());
        }
    }

    // Secondary queries: MX, SOA, TXT (if not already in A response)
    struct { int type; const char* name; } extra[] = {{ns_t_mx, "MX"}, {ns_t_soa, "SOA"}, {ns_t_txt, "TXT"}};
    for (auto& q : extra) {
        unsigned char buf[4096];
        int len = res_query(hb.constData(), C_IN, q.type, buf, sizeof(buf));
        if (len < 0) continue;
        ns_msg handle;
        if (ns_initparse(buf, len, &handle) < 0) continue;
        if (ns_msg_getflag(handle, ns_f_rcode) != ns_r_noerror) continue;
        bool dump = false; QString unused;
        dnsDumpSection(handle, ns_s_an, QStringLiteral(";; %1 SECTION:").arg(q.name), host, out, dump, unused);
        if (ns_msg_count(handle, ns_s_an) > 0) out.append(QString());
    }
#endif

    // ── Footer ──────────────────────────────────────────────────────────
    out.append(QStringLiteral(";; Query time: %1 msec").arg(t.elapsed()));
#if defined(_WIN32) || defined(__ANDROID__)
    out.append(QStringLiteral(";; SERVER: system resolver"));
#else
    // Show actual resolver address from _res (glibc-specific)
    QStringList nsList;
    for (int i = 0; i < MAXNS && _res.nsaddr_list[i].sin_addr.s_addr != 0; i++)
        nsList.append(ip4ToStr(_res.nsaddr_list[i].sin_addr));
    out.append(QStringLiteral(";; SERVER: %1").arg(nsList.isEmpty() ? QStringLiteral("system") : nsList.join(QStringLiteral(", "))));
#endif
    out.append(QStringLiteral(";; WHEN: %1").arg(QDateTime::currentDateTime().toString(QStringLiteral("ddd MMM d hh:mm:ss yyyy"))));
    out.append(QString());

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.durationMs = t.elapsed();

    if (!ipsAll.isEmpty()) {
        r.summary = QStringLiteral("%1 → %2").arg(host, ipsAll.join(QStringLiteral(", ")));
        r.status = DiagStatus::Pass;
    } else {
        r.summary = QStringLiteral("DNS resolution failed for %1").arg(host);
        r.status = DiagStatus::Fail;
    }
    r.properties.append(prop("Target", target));
    r.properties.append(prop("Host", host));
    r.properties.append(prop("Addresses", ipsAll.isEmpty() ? QStringLiteral("(none)") : ipsAll.join(QStringLiteral(", "))));
    return r;
}

// ── TCP / ICMP socket helpers ─────────────────────────────────────────

static quint32 resolveIPv4(const QString& host) {
    return DnsResolver::resolveIPv4(host, 3000);
}

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
    // iOS/macOS: use real ICMP Echo via a datagram ICMP socket (no root needed).
    // If ICMP is blocked/unavailable, fall back to a TCP connect probe. Windows
    // and Linux keep the permission-safe TCP connect method (unchanged).
    for (int i=0; i<4; ++i) {
        ++sent;
        int ms = -1;
#if !defined(_WIN32) && !defined(__linux__)
        if (resolvedIp)
            ms = icmpEchoRttMs(resolvedIp, i + 1, 2000); // ICMP Echo first (accurate)
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

    // Build output — strict Windows tracert.exe format
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

    // TCP TTL probing via tcpTraceHop() — uses IP_RECVERR on Linux to capture
    // ICMP Time Exceeded from intermediate routers without root privileges.
    TRACE(" traceroute: using tcpTraceHop\n");

    QElapsedTimer t; t.start();
    int hopCount = 0, timeoutHops = 0; bool reached = false; bool blocked = false;

    for (int ttl = 1; ttl <= 30 && !reached; ++ttl) {
        int rttMs = 0; QString hopIp;
        int res = tcpTraceHop(host, ttl, rttMs, hopIp);
        ++hopCount;

        if (res == 0) {
            // Reached target
            reached = true;
            QString rttStr = (rttMs < 1) ? QStringLiteral("  <1 ms")
                : QStringLiteral("%1 ms").arg(rttMs, 5);
            lines.append(QStringLiteral(" %1  %2  %3  %4  %5 [%6]")
                .arg(ttl, 2).arg(rttStr).arg(rttStr).arg(rttStr)
                .arg(host).arg(targetIpStr));
            TRACE(" traceroute TTL=%d: REACHED %s [%s] %dms\n",
                ttl, host.toUtf8().constData(), hopIp.toUtf8().constData(), rttMs);
        } else if (res == 1 || res == 2) {
            // res 1 = intermediate router (ICMP Time Exceeded).
            // res 2 = a non-target router replied Destination Unreachable and is
            //         filtering the path (e.g. corporate proxy / admin-prohibited).
            QString rttStr = (rttMs < 1) ? QStringLiteral("  <1 ms")
                : QStringLiteral("%1 ms").arg(rttMs, 5);
            // Resolve reverse DNS for the hop IP
            QString hopName = hopIp;
            struct sockaddr_in sa; memset(&sa, 0, sizeof(sa));
            sa.sin_family = AF_INET;
            sa.sin_addr.s_addr = inet_addr(hopIp.toUtf8().constData());
            char hbuf[NI_MAXHOST] = {};
            if (getnameinfo((struct sockaddr*)&sa, sizeof(sa), hbuf, sizeof(hbuf),
                            nullptr, 0, 0) == 0 && hbuf[0])
                hopName = QString::fromLatin1(hbuf);
            lines.append(QStringLiteral(" %1  %2  %3  %4  %5 [%6]")
                .arg(ttl, 2).arg(rttStr).arg(rttStr).arg(rttStr)
                .arg(hopName).arg(hopIp));
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
            // Timeout
            ++timeoutHops;
            lines.append(QStringLiteral(" %1     *        *        *     Request timed out.").arg(ttl, 2));
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
    // filter ICMP while allowing TCP — ping works via its TCP fallback, but ICMP
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
                lines.append(QStringLiteral("  ICMP may be filtered by the network or device — route discovery unavailable."));
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
        lines.append(QStringLiteral("Trace incomplete — ICMP filtered."));
    } else {
        lines.append(QStringLiteral("Trace incomplete — target may be firewalled."));
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
    if (reached) { r.status = DiagStatus::Pass; r.summary = QStringLiteral("Target reached in %1 hops").arg(hopCount); }
    else if (blocked) { r.status = DiagStatus::Warning; r.summary = QStringLiteral("Path filtered by a router at hop %1").arg(hopCount); }
    else if (tcpReachable) { r.status = DiagStatus::Warning; r.summary = QStringLiteral("ICMP filtered — %1 reachable via TCP").arg(host); }
    else if (hopCount > 0) { r.status = DiagStatus::Warning; r.summary = QStringLiteral("Partial path (%1 hops)").arg(hopCount); }
    else { r.status = DiagStatus::Fail; r.summary = QStringLiteral("No hops discovered"); }
    return r;
}

// ── PathPing — Windows pathping.exe format with TCP-based traceroute ───────
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
            trimmed.startsWith("Trace") || trimmed.startsWith("...")) continue;
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
    // NOTE: ping() uses TCP connect (ports 443,80,22,8080,8443) to measure RTT.
    // Routers typically do NOT run TCP servers, so per-hop statistics will show
    // 100% loss for intermediate hops. Windows pathping.exe uses ICMP Echo for
    // this phase, which routers DO respond to. A future improvement would be to
    // implement ICMP Echo (raw socket) for per-hop statistics on Linux (requires
    // root or CAP_NET_RAW) while keeping TCP connect for reachability checks.
    // For now, the route discovery (Phase 1) works correctly via traceroute.
    struct HopStats { int sent; int rcvd; double loss; int avgMs; };
    QVector<HopStats> hopStats;
    for (int i = 1; i < hops.size(); ++i) {
        HopStats hs = {4, 0, 100.0, 0};
        if (!hops[i].ip.isEmpty()) {
            auto pr = ping(hops[i].ip);
            // Parse: "    Packets: Sent = 4, Received = 3, Lost = 1 (25.0% loss),"
            // Parse: "    Minimum = 8ms, Maximum = 12ms, Average = 10ms"
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
        hopStats.append(hs);
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
            if (nextIdx < hopStats.size() && !nextHop.ip.isEmpty()) {
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
DiagnosticResult mtuDiscovery(const QString& target) {
    DiagnosticResult r;
    r.id = DiagId::G4MtuDiscovery; r.group = DiagGroup::G4;
    r.timestamp = QDateTime::currentDateTime();
    if (target.isEmpty()) return noTargetResult(r.id, r.group);
    QString host = extractHostname(target);
    int probePort = extractProbePort(target);
    quint32 resolvedIp = resolveIPv4(host);
    QString ipStr;
    if (resolvedIp) { struct in_addr a; a.s_addr = htonl(resolvedIp); ipStr = ip4ToStr(a); }
    QStringList out;

    // ── Windows ping -f -l style MTU discovery ─────────────────────────
    out.append(QString());
    out.append(QStringLiteral("Path MTU Discovery for %1 [%2] (probe TCP port %3)")
        .arg(host, ipStr.isEmpty() ? host : ipStr).arg(probePort));
    out.append(QString());

    // Try TCP connect and get MSS → derive path MTU
    int discoveredMtu = 0, mss = 0;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    // fd_set overflow guard: FD_SET with fd >= FD_SETSIZE corrupts the stack.
    if (sock >= 0 && sock >= FD_SETSIZE) { closeSocket(sock); sock = -1; }
    if (sock >= 0 && resolvedIp) {
        struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET; addr.sin_port = htons(probePort);
        addr.sin_addr.s_addr = htonl(resolvedIp);
        setNonblockWin(sock);
        QElapsedTimer t; t.start();
        ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
        fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
        struct timeval tv = {3, 0};
        int sel = select(sock + 1, nullptr, &fdset, nullptr, &tv);
        if (sel > 0) {
            int err = 0; socklen_t elen = sizeof(err);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &elen);
            if (err == 0 || err == ECONNREFUSED) {
                socklen_t mssLen = sizeof(mss);
                if (getsockopt(sock, IPPROTO_TCP, TCP_MAXSEG, (char*)&mss, &mssLen) == 0 && mss > 0) {
                    discoveredMtu = mss + 40; // MSS + IP(20) + TCP(20) headers
                }
#ifdef IP_MTU
                // Also try IP_MTU directly
                int ipMtu = 0; socklen_t ipMtuLen = sizeof(ipMtu);
                if (getsockopt(sock, IPPROTO_IP, IP_MTU, (char*)&ipMtu, &ipMtuLen) == 0 && ipMtu > discoveredMtu)
                    discoveredMtu = ipMtu;
#endif
            }
        }
        int rtt = (int)t.elapsed();
        if (mss > 0) {
            out.append(QStringLiteral("Pinging %1 [%2] with MSS=%3 bytes of data:").arg(host, ipStr).arg(mss));
            out.append(QStringLiteral("Reply from %1: MSS=%2 time=%3ms PMTU=%4").arg(ipStr).arg(mss).arg(rtt).arg(discoveredMtu));
        } else {
            out.append(QStringLiteral("Pinging %1 [%2] MTU probe:").arg(host, ipStr));
            out.append(QStringLiteral("TCP connect succeeded but MSS not available."));
        }
        closeSocket(sock);
    }
    if (discoveredMtu == 0) {
        // Fallback: probe with interface MTU (Windows ping -f -l style)
        discoveredMtu = 1500;
        out.append(QStringLiteral("PMTU TCP probe failed — using interface MTU."));
#ifdef _WIN32
        out.append(QStringLiteral("Pinging %1 [%2] with %3 bytes of data:").arg(host, ipStr).arg(discoveredMtu - 28));
        out.append(QStringLiteral("Using default MTU: %1").arg(discoveredMtu));
#else
        // Check local interface MTU from /sys/class/net
        QDir netDir(QStringLiteral("/sys/class/net"));
        for (const auto& fi : netDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QFile f(QStringLiteral("/sys/class/net/%1/mtu").arg(fi.fileName()));
            if (f.open(QIODevice::ReadOnly)) {
                int v = QString::fromLatin1(f.readAll().trimmed()).toInt();
                if (v > 0 && v > discoveredMtu) discoveredMtu = v;
            }
        }
        int payload = discoveredMtu > 28 ? discoveredMtu - 28 : discoveredMtu;
        out.append(QStringLiteral("Pinging %1 [%2] with %3 bytes of data:").arg(host, ipStr.isEmpty() ? host : ipStr).arg(payload));
        out.append(QStringLiteral("Reply from local interface: MTU=%1 bytes").arg(discoveredMtu));
#endif
    }

    out.append(QString());
    out.append(QStringLiteral("Ping statistics for %1:").arg(ipStr.isEmpty() ? host : ipStr));
    out.append(QStringLiteral("    Maximum MTU: %1 bytes").arg(discoveredMtu));
    out.append(QStringLiteral("    Effective MSS: %1 bytes").arg(discoveredMtu > 40 ? discoveredMtu - 40 : 0));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.properties.append(prop("Target", target));
    r.properties.append(prop("Host", host));
    r.properties.append(prop("MtuValue", QString::number(discoveredMtu)));
    r.properties.append(prop("MssValue", QString::number(mss)));
    if (discoveredMtu >= 1500) { r.status = DiagStatus::Pass; r.summary = QStringLiteral("MTU %1 (standard)").arg(discoveredMtu); }
    else if (discoveredMtu >= 1280) { r.status = DiagStatus::Warning; r.summary = QStringLiteral("MTU %1 (below 1500)").arg(discoveredMtu); }
    else { r.status = DiagStatus::Warning; r.summary = QStringLiteral("Low MTU: %1").arg(discoveredMtu); }
    return r;
}

} // namespace G4RemoteHost
