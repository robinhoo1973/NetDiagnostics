#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif
#include "engine/diagnostics/G4/G4RemoteHost.h"
#include "util/DebugSwitch.h"
#include "util/Logger.h"
#include "util/DnsResolver.h"
#include "util/NetUtil.h"
#include <QHostInfo>
#include <QRegularExpression>
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

static DiagnosticResult noTargetResult(DiagId id, DiagGroup group);

// =============================================================================
// =============================================================================
#pragma once

#include <QVector>

struct PingResult {
    int sent = 0;
    int received = 0;
    int lost = 0;
    double lossPercent = 0.0;
    double minMs = 0.0;
    double avgMs = 0.0;
    double maxMs = 0.0;
    bool valid = false;
};

struct TracerouteHop {
    int hop = 0;
    QString ip;
    double rtt1Ms = 0.0;
    double rtt2Ms = 0.0;
    double rtt3Ms = 0.0;
    bool timedOut = false;
};

struct TracerouteResult {
    QVector<TracerouteHop> hops;
    int hopCount = 0;
    int timeoutHops = 0;
    bool reachedTarget = false;
};

public:
    /// Parse ping output. Handles Windows (ping -n) and Unix (ping -c) formats,
    /// English, Chinese, German, and other locales.
    static PingResult parse(const QString& output);

    /// Parse Windows tracert output.
    static TracerouteResult parseTraceroute(const QString& output);

    /// Extract loss percentage from any ping-like output.
    static double extractLossPercent(const QString& output);

    /// Count lines matching "bytes from" / "time=" patterns (fallback).
    static int countSuccessfulReplies(const QString& output);

private:
    static PingResult parseUnix(const QString& output);
    static PingResult parseWindows(const QString& output);
};
// =============================================================================
// =============================================================================

    // Try Unix format first (more common on Linux), then Windows
    auto r = parseUnix(output);
    if (r.valid) return r;
    r = parseWindows(output);
    if (r.valid) return r;

    // Fallback: count matching reply lines
    r.valid = true;
    r.received = countSuccessfulReplies(output);
    r.lossPercent = extractLossPercent(output);
    if (r.lossPercent < 0) r.lossPercent = (r.received > 0) ? 0.0 : 100.0;
    return r;
}

    PingResult r;

    // Try: "N packets transmitted, N received, N% packet loss, time Nms"
    // Also: "N packets transmitted, N received, +N errors, N% packet loss"
    static const QRegularExpression unixRe(
        R"((\d+)\s+packets?\s+transmitted,\s*(\d+)\s+(?:packets?\s+)?received,)"
        R"((?:\+\d+\s+errors,\s*)?(\d+(?:\.\d+)?)%\s+packet\s+loss)",
        QRegularExpression::CaseInsensitiveOption
    );
    auto m = unixRe.match(output);
    if (m.hasMatch()) {
        r.sent = m.captured(1).toInt();
        r.received = m.captured(2).toInt();
        r.lossPercent = m.captured(3).toDouble();
        r.lost = r.sent - r.received;
        r.valid = true;
    }

    // Try: "rtt min/avg/max/mdev = 1.234/5.678/9.012/1.234 ms"
    static const QRegularExpression rttRe(
        R"(rtt\s+min/avg/max/mdev\s*=\s*([\d.]+)/([\d.]+)/([\d.]+)/[\d.]+\s*ms)",
        QRegularExpression::CaseInsensitiveOption
    );
    auto rm = rttRe.match(output);
    if (rm.hasMatch()) {
        r.minMs = rm.captured(1).toDouble();
        r.avgMs = rm.captured(2).toDouble();
        r.maxMs = rm.captured(3).toDouble();
    }

    // Fallback: find N% pattern anywhere
    if (!r.valid) {
        double loss = extractLossPercent(output);
        if (loss >= 0) {
            r.lossPercent = loss;
            r.received = countSuccessfulReplies(output);
            r.valid = true;
        }
    }

    return r;
}

    PingResult r;

    // Try "Sent = N, Received = N, Lost = N (N% loss)"
    // Also locale-independent: parse numbers after "Packets:"
    static const QRegularExpression winRe(
        R"(Sent\s*=\s*(\d+).*?Received\s*=\s*(\d+).*?Lost\s*=\s*(\d+)\s*\((\d+(?:\.\d+)?)%\s*loss\))",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption
    );
    auto m = winRe.match(output);
    if (m.hasMatch()) {
        r.sent = m.captured(1).toInt();
        r.received = m.captured(2).toInt();
        r.lost = m.captured(3).toInt();
        r.lossPercent = m.captured(4).toDouble();
        r.valid = true;
    }

    // Try "Minimum = Nms, Maximum = Nms, Average = Nms"
    static const QRegularExpression rttWinRe(
        R"(Minimum\s*=\s*(\d+)ms.*?Maximum\s*=\s*(\d+)ms.*?Average\s*=\s*(\d+)ms)",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption
    );
    auto rm = rttWinRe.match(output);
    if (rm.hasMatch()) {
        r.minMs = rm.captured(1).toDouble();
        r.maxMs = rm.captured(2).toDouble();
        r.avgMs = rm.captured(3).toDouble();
    }

    // Fallback: find locale-independent loss pattern
    if (!r.valid) {
        double loss = extractLossPercent(output);
        if (loss >= 0) {
            r.lossPercent = loss;
            r.received = countSuccessfulReplies(output);
            r.valid = true;
        }
    }

    return r;
}

    // Match "N% loss" or "N% packet loss" or "(N% loss)" — case insensitive, locale independent
    static const QRegularExpression re(
        R"((\d+(?:\.\d+)?)\s*%\s*(?:packet\s*)?loss)",
        QRegularExpression::CaseInsensitiveOption
    );
    auto m = re.match(output);
    return m.hasMatch() ? m.captured(1).toDouble() : -1.0;
}

    int count = 0;
    for (const auto& line : output.split('\n')) {
        if (line.contains("bytes from", Qt::CaseInsensitive) ||
            line.contains("time=", Qt::CaseInsensitive) ||
            line.contains("time<", Qt::CaseInsensitive) ||
            line.contains("zeit=", Qt::CaseInsensitive) ||
            line.contains("tempo=", Qt::CaseInsensitive)) {
            ++count;
        }
    }
    return count;
}

    TracerouteResult result;

    // Windows: "  N   rtt1   rtt2   rtt3   IP" — rtt may be "<1 ms" or digits
    static const QRegularExpression winHop(
        R"(^\s*(\d+)\s+(<1\s*ms|\d+\s*ms|\*)\s+(<1\s*ms|\d+\s*ms|\*)\s+(<1\s*ms|\d+\s*ms|\*)\s+([\d.]+))",
        QRegularExpression::MultilineOption
    );
    // Unix: " N  hostname (IP)  rtt1 ms  rtt2 ms  rtt3 ms" or " N  * * *"
    static const QRegularExpression unixHop(
        R"(^\s*(\d+)\s+(?:\S+\s+)?(?:\(?([\d.]+)\)?)?\s+(?:(\d+\.?\d*)\s*ms\s+)?(?:(\d+\.?\d*)\s*ms\s+)?(?:(\d+\.?\d*)\s*ms)?)",
        QRegularExpression::MultilineOption
    );
    static const QRegularExpression starHop(
        R"(^\s*(\d+)\s+\*\s+\*\s+\*)",
        QRegularExpression::MultilineOption
    );

    // Helper: parse a captured RTT field ("<1 ms", "3 ms", "*", or empty)
    auto parseRttMs = [](const QString& cap) -> double {
        if (cap.isEmpty() || cap.startsWith('*')) return -1.0; // timeout
        if (cap.startsWith("<1")) return 0.5;                  // sub-millisecond
        return cap.section(' ', 0, 0).toDouble();              // "N ms" → N
    };

    // Try Windows format first
    auto winMatches = winHop.globalMatch(output);
    if (winMatches.hasNext()) {
        while (winMatches.hasNext()) {
            auto m = winMatches.next();
            TracerouteHop hop;
            hop.hop = m.captured(1).toInt();
            hop.rtt1Ms = parseRttMs(m.captured(2));
            hop.rtt2Ms = parseRttMs(m.captured(3));
            hop.rtt3Ms = parseRttMs(m.captured(4));
            hop.ip = m.captured(5).trimmed();
            // Mark hop as timed out if all three probes failed
            if (hop.rtt1Ms < 0 && hop.rtt2Ms < 0 && hop.rtt3Ms < 0)
                hop.timedOut = true;
            result.hops.append(hop);
        }
        result.hopCount = static_cast<int>(result.hops.size());
    }

    // Try Unix format
    if (result.hops.isEmpty()) {
        auto unixMatches = unixHop.globalMatch(output);
        bool hasHops = false;
        while (unixMatches.hasNext()) {
            auto m = unixMatches.next();
            TracerouteHop hop;
            hop.hop = m.captured(1).toInt();
            hop.ip = m.captured(2);
            hop.rtt1Ms = m.captured(3).toDouble();
            hop.rtt2Ms = m.captured(4).toDouble();
            hop.rtt3Ms = m.captured(5).toDouble();
            hop.timedOut = hop.ip.isEmpty() && hop.rtt1Ms == 0;
            if (hop.timedOut) ++result.timeoutHops;
            result.hops.append(hop);
            hasHops = true;
        }
        if (hasHops) result.hopCount = static_cast<int>(result.hops.size());
    }

    // Count * * * timeouts (Windows and Unix)
    auto starMatches = starHop.globalMatch(output);
    int starCount = 0;
    while (starMatches.hasNext()) {
        starMatches.next();
        ++starCount;
    }
    if (starCount > result.timeoutHops) result.timeoutHops = starCount;

    // Detect if target was reached
    result.reachedTarget = output.contains("Trace complete", Qt::CaseInsensitive) ||
                           (!result.hops.isEmpty() && !result.hops.last().timedOut);

    return result;
}
