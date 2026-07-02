// =============================================================================
// IosNetworkInfo.mm — iOS network info via public API workarounds
//
// Provides partial implementations for diagnostics that Apple's sandbox blocks:
// - Default gateway: real gateway IP via sysctl NET_RT_DUMP2 (BSD route dump)
// - Routing table: sysctl NET_RT_DUMP2 enumerates the kernel routing table
// - DHCP status: Always system-managed on iOS (no lease file access)
// - ARP table: Unavailable (link-layer, no public API)
// =============================================================================
#import <SystemConfiguration/SystemConfiguration.h>
#import <sys/socket.h>
#import <sys/sysctl.h>
#import <netinet/in.h>
#import <arpa/inet.h>
#import <ifaddrs.h>
#import <net/if.h>
#if __has_include(<net/route.h>)
#import <net/route.h>
#else
// net/route.h was removed from the iOS SDK in Xcode 26 (iOS SDK 26+).
// Define the minimum required types and constants from the stable BSD route ABI.
#define NET_RT_DUMP2    7
#define RTF_GATEWAY     0x2
#define RTAX_DST        0
#define RTAX_GATEWAY    1
#define RTAX_NETMASK    2
#define RTAX_MAX        8
struct rt_metrics {
    u_int32_t rmx_locks;
    u_int32_t rmx_mtu;
    u_int32_t rmx_hopcount;
    int32_t   rmx_expire;
    u_int32_t rmx_recvpipe;
    u_int32_t rmx_sendpipe;
    u_int32_t rmx_ssthresh;
    u_int32_t rmx_rtt;
    u_int32_t rmx_rttvar;
    u_int32_t rmx_pksent;
    u_int32_t rmx_state;
    u_int32_t rmx_filler[3];
};
struct rt_msghdr2 {
    u_short           rtm_msglen;
    u_char            rtm_version;
    u_char            rtm_type;
    u_short           rtm_index;
    int               rtm_flags;
    int               rtm_addrs;
    int32_t           rtm_refcnt;
    int               rtm_parentflags;
    int               rtm_reserved;
    int               rtm_use;
    u_int32_t         rtm_inits;
    struct rt_metrics rtm_rmx;
};
#endif
#include <QString>
#include <QStringList>
#include <QVector>
#include "models/DiagnosticResult.h"

// Round a sockaddr length up to the next 4-byte boundary (BSD routing alignment).
#ifndef SA_SIZE
#define SA_SIZE(sa) \
    ( ((sa) == nullptr || ((struct sockaddr*)(sa))->sa_len == 0) ? \
        sizeof(uint32_t) : \
        (1 + ((((struct sockaddr*)(sa))->sa_len - 1) | (sizeof(uint32_t) - 1))) )
#endif

// ── Routing table via sysctl NET_RT_DUMP2 ──────────────────────────────
// Unlike /proc/net/route (Linux-only) and NET_RT_DUMP, NET_RT_DUMP2 is the
// BSD/Darwin route dump that IS reachable from the iOS sandbox. It returns the
// live kernel routing table, from which we can read real gateway IPs.
struct IosRoute { QString dest, gateway, netmask, iface; int flags; };

static QString ip4FromSockaddr(const struct sockaddr* sa) {
    if (!sa || sa->sa_family != AF_INET) return QString();
    const struct sockaddr_in* sin = (const struct sockaddr_in*)sa;
    char buf[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
    return QString::fromLatin1(buf);
}

static QVector<IosRoute> iosReadRoutes() {
    QVector<IosRoute> routes;
    int mib[6] = {CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_DUMP2, 0};
    size_t len = 0;
    if (sysctl(mib, 6, nullptr, &len, nullptr, 0) < 0 || len == 0)
        return routes; // sandbox blocked or empty
    QByteArray buf(static_cast<int>(len), 0);
    if (sysctl(mib, 6, buf.data(), &len, nullptr, 0) < 0)
        return routes;

    char* lim = buf.data() + len;
    for (char* nextp = buf.data(); nextp < lim; ) {
        struct rt_msghdr2* rtm = (struct rt_msghdr2*)nextp;
        if (rtm->rtm_msglen == 0) break;
        struct sockaddr* sa = (struct sockaddr*)(rtm + 1);
        struct sockaddr* addrs[RTAX_MAX] = {nullptr};
        for (int i = 0; i < RTAX_MAX; ++i) {
            if (rtm->rtm_addrs & (1 << i)) {
                addrs[i] = sa;
                sa = (struct sockaddr*)((char*)sa + SA_SIZE(sa));
            }
        }
        IosRoute rt;
        rt.flags = rtm->rtm_flags;
        // Destination: a zero/absent AF_INET dest means the default route.
        if (addrs[RTAX_DST]) {
            QString d = ip4FromSockaddr(addrs[RTAX_DST]);
            rt.dest = d.isEmpty() ? QStringLiteral("default") : d;
            if (rt.dest == QLatin1String("0.0.0.0")) rt.dest = QStringLiteral("default");
        }
        if (addrs[RTAX_GATEWAY]) rt.gateway = ip4FromSockaddr(addrs[RTAX_GATEWAY]);
        if (addrs[RTAX_NETMASK]) rt.netmask = ip4FromSockaddr(addrs[RTAX_NETMASK]);
        char ifname[IF_NAMESIZE] = {0};
        if (if_indextoname(rtm->rtm_index, ifname)) rt.iface = QString::fromLatin1(ifname);
        if (!rt.dest.isEmpty() || !rt.gateway.isEmpty())
            routes.append(rt);
        nextp += rtm->rtm_msglen;
    }
    return routes;
}

// ── Interface IPv4 + gateway helpers (cellular / WiFi panels) ───────────
// Public (non-static): declared in IosNetworkInfo.h, called from G1G2G3Native.
QString iosInterfaceIPv4(const QString& iface) {
    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) != 0) return QString();
    QString ip;
    for (auto* p = ifa; p; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
        if (QString::fromLatin1(p->ifa_name) != iface) continue;
        char buf[INET_ADDRSTRLEN] = {0};
        auto* sin = (struct sockaddr_in*)p->ifa_addr;
        inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
        ip = QString::fromLatin1(buf);
        break;
    }
    freeifaddrs(ifa);
    return ip;
}

QString iosGatewayForInterface(const QString& iface) {
    QString fallback;
    for (const auto& rt : iosReadRoutes()) {
        if (rt.iface != iface || rt.gateway.isEmpty()) continue;
        if (!(rt.flags & RTF_GATEWAY)) continue;
        if (rt.dest == QLatin1String("default")) return rt.gateway; // prefer default route
        if (fallback.isEmpty()) fallback = rt.gateway;
    }
    return fallback;
}

// ── Default Gateway — real IP from the routing table, fallback to interface ──
static QString iosDefaultGateway() {
    // Preferred: the RTF_GATEWAY default route from the kernel routing table.
    for (const auto& rt : iosReadRoutes()) {
        if (rt.dest == QLatin1String("default") && !rt.gateway.isEmpty()
            && (rt.flags & RTF_GATEWAY)) {
            return QStringLiteral("%1 (interface %2)")
                .arg(rt.gateway, rt.iface.isEmpty() ? QStringLiteral("?") : rt.iface);
        }
    }
    // Fallback: first non-loopback UP IPv4 interface (route table unavailable).
    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) != 0) return QString();
    QString gatewayInfo;
    for (auto* p = ifa; p; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
        if (!(p->ifa_flags & IFF_UP)) continue;
        QString name = QString::fromLatin1(p->ifa_name);
        if (name == "lo0") continue;
        char ip[INET_ADDRSTRLEN];
        auto* sa = (struct sockaddr_in*)p->ifa_addr;
        inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
        gatewayInfo = QStringLiteral("System-managed (iOS) — interface: %1 (%2)")
            .arg(name, QString::fromLatin1(ip));
        break;
    }
    freeifaddrs(ifa);
    return gatewayInfo;
}

// ── DHCP Status (iOS: always system-managed) ───────────────────────────
static QString iosDhcpStatus() {
    // On iOS, DHCP is always enabled and managed by the OS. Apps cannot access
    // lease files or DHCP server info. This is by design (Apple sandbox).
    // We can detect the assigned IP via getifaddrs to show at least some info.
    QStringList lines;
    lines.append(QString());
    lines.append(QStringLiteral("DHCP Client Status:"));
    lines.append(QString());
    lines.append(QStringLiteral("  [iOS] DHCP is system-managed. Lease files inaccessible."));

    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) == 0) {
        for (auto* p = ifa; p; p = p->ifa_next) {
            if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
            if (!(p->ifa_flags & IFF_UP)) continue;
            QString name = QString::fromLatin1(p->ifa_name);
            if (name == "lo0") continue;
            char ip[INET_ADDRSTRLEN];
            auto* sa = (struct sockaddr_in*)p->ifa_addr;
            inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
            lines.append(QStringLiteral("  %1: %2 (DHCP assigned)").arg(name).arg(QString::fromLatin1(ip)));
        }
        freeifaddrs(ifa);
    }
    return lines.join('\n');
}

// ── Public API: iOS workaround implementations ─────────────────────────

// Returns a DiagnosticResult for default gateway on iOS
DiagnosticResult iosDefaultGatewayDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();
    QString gw = iosDefaultGateway();
    r.rawOutput = QStringLiteral("\nDefault Gateway:\n\n  %1\n").arg(gw);
    r.details = r.rawOutput;
    r.summary = gw.startsWith("System-managed") ? QStringLiteral("iOS system-managed") : gw;
    r.status = gw.startsWith("System-managed") ? DiagStatus::Info : DiagStatus::Pass;
    return r;
}

// Returns a DiagnosticResult for DHCP status on iOS
DiagnosticResult iosDhcpDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    r.rawOutput = iosDhcpStatus();
    r.details = r.rawOutput;
    r.summary = QStringLiteral("System-managed (iOS)");
    r.status = DiagStatus::Info;
    return r;
}

// Returns a DiagnosticResult for the routing table on iOS (sysctl NET_RT_DUMP2)
DiagnosticResult iosRoutingTableDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();

    QVector<IosRoute> routes = iosReadRoutes();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("IPv4 Route Table (iOS — sysctl NET_RT_DUMP2)"));
    out.append(QStringLiteral("==========================================================================="));

    if (routes.isEmpty()) {
        out.append(QStringLiteral("  Routing table unavailable (blocked by iOS sandbox)."));
        r.rawOutput = out.join('\n');
        r.details = r.rawOutput;
        r.status = DiagStatus::Skipped;
        r.summary = QStringLiteral("Unavailable on iOS");
        return r;
    }

    out.append(QStringLiteral("  %1  %2  %3  %4")
        .arg(QStringLiteral("Destination"), -18)
        .arg(QStringLiteral("Gateway"), -18)
        .arg(QStringLiteral("Netmask"), -16)
        .arg(QStringLiteral("Iface")));
    out.append(QStringLiteral("  %1  %2  %3  %4")
        .arg(QString(18, '-')).arg(QString(18, '-'))
        .arg(QString(16, '-')).arg(QString(5, '-')));

    QString defaultGw;
    for (const auto& rt : routes) {
        QString gw = rt.gateway.isEmpty() ? QStringLiteral("link#") : rt.gateway;
        out.append(QStringLiteral("  %1  %2  %3  %4")
            .arg(rt.dest, -18)
            .arg(gw, -18)
            .arg(rt.netmask.isEmpty() ? QStringLiteral("-") : rt.netmask, -16)
            .arg(rt.iface));
        if (rt.dest == QLatin1String("default") && (rt.flags & RTF_GATEWAY) && defaultGw.isEmpty())
            defaultGw = rt.gateway;
    }
    out.append(QStringLiteral("==========================================================================="));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = defaultGw.isEmpty()
        ? QStringLiteral("%1 routes").arg(routes.size())
        : QStringLiteral("Default via %1").arg(defaultGw);
    return r;
}

