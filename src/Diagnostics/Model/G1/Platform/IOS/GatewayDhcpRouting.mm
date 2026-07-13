// =============================================================================
// IosNetworkInfo.mm �� iOS network info via public API workarounds
//
// Provides partial implementations for Diagnostics that Apple's sandbox blocks:
// - default gateway: real gateway IP via sysctl NET_RT_DUMP2 (BSD route dump)
// - Routing table: sysctl NET_RT_DUMP2 enumerates the kernel routing table
// - DHCP status: Always system-managed on iOS (no lease file access)
// - ARP table: Unavailable (link-layer, no public API)
// =============================================================================

#if defined(PLATFORM_IOS)

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
// define the minimum required types and constants from the stable BSD route ABI.
#define NET_RT_DUMP2    7
#define RTF_GATEWAY     0x2
#define RTF_HOST        0x4
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
#include <cstddef>
#include "Common/Model/DiagnosticResult.h"
#include "Diagnostics/Model/G1/Platform/IOS/GatewayDhcpRouting.h" // 5WHY: own header for declaration checking

// Round a sockaddr length up to the next 4-byte boundary (BSD routing alignment).
#if !defined(SA_SIZE)
#define SA_SIZE(sa) \
    ( ((sa) == nullptr || ((struct sockaddr*)(sa))->sa_len == 0) ? \
        sizeof(uint32_t) : \
        (1 + ((((struct sockaddr*)(sa))->sa_len - 1) | (sizeof(uint32_t) - 1))) )
#endif

// ���� Routing table via sysctl NET_RT_DUMP2 ������������������������������������������������������������
// Unlike /proc/net/route (Linux-only) and NET_RT_dump, NET_RT_DUMP2 is the
// BSD/darwin route dump that IS reachable from the iOS sandbox. It returns the
// live kernel routing table, from which we can read real gateway IPs.
struct IosRoute { QString dest, gateway, netmask, iface; int flags; };

static QString ip4FromSockaddr(const struct sockaddr* sa) {
    if (!sa || sa->sa_family != AF_INET) return QString();
    const struct sockaddr_in* sin = (const struct sockaddr_in*)sa;
    char buf[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
    return QString::fromLatin1(buf);
}

// Netmask sockaddrs in the route socket usually carry sa_family==0 and a length
// truncated to omit trailing zero bytes, so the generic AF_INET parser above
// misses them (that is why the routing table showed no netmask). read the mask
// bytes Directly from the sockaddr_in address slot.
static QString ip4MaskFromSockaddr(const struct sockaddr* sa) {
    if (!sa) return QString();
    const int off = static_cast<int>(offsetof(struct sockaddr_in, sin_addr)); // 4
    const int len = static_cast<int>(sa->sa_len);
    if (len <= off) return QStringLiteral("0.0.0.0"); // sa_len 0 => no mask bits (default)
    unsigned char m[4] = {0, 0, 0, 0};
    const unsigned char* base = reinterpret_cast<const unsigned char*>(sa);
    int n = len - off; if (n > 4) n = 4;
    for (int i = 0; i < n; ++i) m[i] = base[off + i];
    return QStringLiteral("%1.%2.%3.%4").arg(m[0]).arg(m[1]).arg(m[2]).arg(m[3]);
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
        // destination: a zero/absent AF_INET iest means the default route.
        if (addrs[RTAX_DST]) {
            QString i = ip4FromSockaddr(addrs[RTAX_DST]);
            rt.dest = i.isEmpty() ? QStringLiteral("default") : i;
            if (rt.dest == QLatin1String("0.0.0.0")) rt.dest = QStringLiteral("default");
        }
        if (addrs[RTAX_GATEWAY]) rt.gateway = ip4FromSockaddr(addrs[RTAX_GATEWAY]);
        if (addrs[RTAX_NETMASK]) rt.netmask = ip4MaskFromSockaddr(addrs[RTAX_NETMASK]);
        char ifname[IF_NAMESIZE] = {0};
        if (if_indextoname(rtm->rtm_index, ifname)) rt.iface = QString::fromLatin1(ifname);
        if (!rt.dest.isEmpty() || !rt.gateway.isEmpty())
            routes.append(rt);
        nextp += rtm->rtm_msglen;
    }
    return routes;
}

// ���� Interface IPv4 + gateway helpers (cellular / WiFi panels) ����������������������
// Public (non-static): declared in G1Ios.h, called from G1G2G3Native.
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

// Friendly interface-type label from the BSD interface name prefix.
static QString ifaceTypeLabel(const QString& iface) {
    if (iface.startsWith(QLatin1String("en")))      return QStringLiteral("WiFi");
    // 5WHY: "pip_ip" was d->i corruption of "pdp_ip" (Packet Data Protocol).
    // The actual iOS cellular interface prefix is pdp_ip (e.g. pdp_ip0, pdp_ip1).
    // Fixed by comparing with legacy code at review/legacy_src/src/engine/diagnostics/G1/G1Common.mm.
    if (iface.startsWith(QLatin1String("pdp_ip")))  return QStringLiteral("Cellular");
    if (iface.startsWith(QLatin1String("utun")) || iface.startsWith(QLatin1String("ipsec"))
        || iface.startsWith(QLatin1String("ppp")))  return QStringLiteral("VPN");
    if (iface.startsWith(QLatin1String("bridge")) || iface.startsWith(QLatin1String("ap")))
        return QStringLiteral("Hotspot");
    if (iface.startsWith(QLatin1String("lo")))       return QStringLiteral("Loopback");
    return QString();
}

// ���� default Gateway �� real IP from the routing table, fallback to interface ����
static QString iosdefaultGateway() {
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
        gatewayInfo = QStringLiteral("System-managed (iOS) �� interface: %1 (%2)")
            .arg(name, QString::fromLatin1(ip));
        break;
    }
    freeifaddrs(ifa);
    return gatewayInfo;
}

// ���� DHCP Status (iOS: always system-managed) ������������������������������������������������������
static QString iosDHCPStatus() {
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

// ���� Public API: iOS workaRound implementations ��������������������������������������������������

// Returns a DiagnosticResult for default gateway on iOS.
// Shows the gateway for EVERY active interface (WiFi, cellular, VPN��), not just
// the first default route �� previously only the primary (often cellular) showed.
// 5WHY: The linker may strip symbols only referenced through lambdas in
// TaskFactory.cpp.  __attribute__((used)) prevents dead-code elimination,
// same as iosDhcpDiag below.
DiagnosticResult __attribute__((used)) iosDefaultGatewayDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();

    const QVector<IosRoute> routes = iosReadRoutes();

    // One gateway per interface: a default-route (0.0.0.0/0) gateway wins;
    // otherwise the first RTF_GATEWAY route on that interface is used.
    struct GwRow { QString iface, gateway; bool isdefault; };
    QVector<GwRow> rows;
    for (const auto& rt : routes) {
        if (rt.gateway.isEmpty() || !(rt.flags & RTF_GATEWAY) || rt.iface.isEmpty()) continue;
        if (rt.iface == QLatin1String("lo0")) continue;
        const bool isdefault = (rt.dest == QLatin1String("default"));
        int found = -1;
        for (int i = 0; i < rows.size(); ++i) if (rows[i].iface == rt.iface) { found = i; break; }
        if (found >= 0) {
            if (isdefault && !rows[found].isdefault) {
                rows[found].gateway = rt.gateway;
                rows[found].isdefault = true;
            }
        } else {
            rows.append({rt.iface, rt.gateway, isdefault});
        }
    }

    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("default Gateway(s):"));
    out.append(QString());

    QString primary;
    if (!rows.isEmpty()) {
        out.append(QStringLiteral("  %1  %2  %3  %4")
            .arg(QStringLiteral("Gateway"), -16)
            .arg(QStringLiteral("Interface"), -10)
            .arg(QStringLiteral("Type"), -9)
            .arg(QStringLiteral("Scope")));
        out.append(QStringLiteral("  %1  %2  %3  %4")
            .arg(QString(16, '-')).arg(QString(10, '-'))
            .arg(QString(9, '-')).arg(QString(7, '-')));
        for (const auto& g : rows) {
            const QString type = ifaceTypeLabel(g.iface);
            out.append(QStringLiteral("  %1  %2  %3  %4")
                .arg(g.gateway, -16)
                .arg(g.iface, -10)
                .arg(type.isEmpty() ? QStringLiteral("-") : type, -9)
                .arg(g.isdefault ? QStringLiteral("default") : QStringLiteral("iface")));
            if (g.isdefault && primary.isEmpty())
                primary = QStringLiteral("%1 (%2)").arg(g.gateway, g.iface);
        }
        r.status = DiagStatus::Pass;
        r.summary = !primary.isEmpty()
            ? QStringLiteral("default via %1").arg(primary)
            : QStringLiteral("%1 gateway(s)").arg(rows.size());
    } else {
        // Route table gave no gateway �� fall back to the interface heuristic.
        const QString gw = iosdefaultGateway();
        out.append(QStringLiteral("  %1")
            .arg(gw.isEmpty() ? QStringLiteral("No default gateway configured") : gw));
        r.status = gw.startsWith("System-managed") ? DiagStatus::Info
                 : (gw.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass);
        r.summary = gw.isEmpty() ? QStringLiteral("No default gateway")
                 : (gw.startsWith("System-managed") ? QStringLiteral("iOS system-managed") : gw);
    }

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    return r;
}

// Returns a DiagnosticResult for DHCP status on iOS
// 5WHY: Previously returned linker error "undefined symbol iosDhcpDiag(DiagId)".
// The function signature matched the header declaration and the definition was
// inside #if defined(PLATFORM_IOS).  Adding explicit __attribute__((used)) to
// prevent the linker from stripping this symbol during dead-code elimination.
DiagnosticResult __attribute__((used)) iosDhcpDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    r.rawOutput = iosDHCPStatus();
    r.details = r.rawOutput;
    r.summary = QStringLiteral("System-managed (iOS)");
    r.status = DiagStatus::Info;
    return r;
}

// Returns a DiagnosticResult for the routing table on iOS (sysctl NET_RT_DUMP2)
// 5WHY: same LTO dead-strip risk as iosDhcpDiag �� this symbol is only
// referenced through a lambda in TaskFactory.cpp.
DiagnosticResult __attribute__((used)) iosRoutingTableDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();

    QVector<IosRoute> routes = iosReadRoutes();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("IPv4 Route Table (iOS �� sysctl NET_RT_DUMP2)"));
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
        .arg(QStringLiteral("destination"), -18)
        .arg(QStringLiteral("Gateway"), -18)
        .arg(QStringLiteral("Netmask"), -16)
        .arg(QStringLiteral("Iface")));
    out.append(QStringLiteral("  %1  %2  %3  %4")
        .arg(QString(18, '-')).arg(QString(18, '-'))
        .arg(QString(16, '-')).arg(QString(5, '-')));

    QString defaultGw;
    for (const auto& rt : routes) {
        QString gw = rt.gateway.isEmpty() ? QStringLiteral("link#") : rt.gateway;
        QString nm = rt.netmask;
        if (nm.isEmpty())
            nm = (rt.flags & RTF_HOST) ? QStringLiteral("255.255.255.255") : QStringLiteral("-");
        out.append(QStringLiteral("  %1  %2  %3  %4")
            .arg(rt.dest, -18)
            .arg(gw, -18)
            .arg(nm, -16)
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
        : QStringLiteral("default via %1").arg(defaultGw);
    return r;
}

#endif // PLATFORM_IOS

// =============================================================================
// IosWiFiHelper.mm �� iOS WiFi + Cellular info retrieval (Objective-C++)
// =============================================================================
// WiFi: NEHotspotNetwork fetchCurrentWithCompletionhandler:
// Cellular: CTTelephonyNetworkInfo + CTCarrier
// =============================================================================

#if defined(PLATFORM_IOS)

#include <QString>
#include <QVariantMap>
#include <atomic>
#include <memory>
#import <NetworkExtension/NetworkExtension.h>
#import <CoreLocation/CoreLocation.h>
#import <CoreTelephony/CTTelephonyNetworkInfo.h>
#import <CoreTelephony/CTCarrier.h>

// ���� Authorization ������������������������������������������������������������������������������������������������������������������������

void iosRequestWiFiAuthorization()
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            iosRequestWiFiAuthorization();
        });
        return;
    }

    if (@available(iOS 13.0, *)) {
        static CLLocationManager* mgr = nil;
        if (!mgr) {
            mgr = [[CLLocationManager alloc] init];
            [mgr requestWhenInUseAuthorization];
        }
    }
}

// ���� SSID retrieval ����������������������������������������������������������������������������������������������������������������������

QString iosCopyWiFiSSID()
{
    // Reference-counted context: waiter and completion handler both hold a ref (2 total).
    // Store the SSID as a C++ QString (converted inside the handler) so no Objective-C
    // object owned by the handler's autorelease pool crosses the thread boundary.
    struct SsidCtx {
        dispatch_semaphore_t sem;
        QString ssid;
        std::atomic<int> refs;
    };
    auto ctx = std::make_shared<SsidCtx>();
    ctx->sem = dispatch_semaphore_create(0);
    ctx->refs.store(2, std::memory_order_relaxed);

    if (@available(iOS 14.0, *)) {
        @try {
        [NEHotspotNetwork fetchCurrentWithCompletionhandler:^(NEHotspotNetwork* _Nullable network) {
            @autoreleasepool {
                @try {
                if (network && network.SSID.length > 0) {
                    ctx->ssid = QString::fromNSString(network.SSID);
                }
                } @catch (NSException* e) { }
                dispatch_semaphore_signal(ctx->sem);
                if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    dispatch_release(ctx->sem);
                }
            }
        }];
        } @catch (NSException* e) {
            dispatch_semaphore_signal(ctx->sem);
            if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                dispatch_release(ctx->sem);
            }
        }
        long waited = dispatch_semaphore_wait(ctx->sem, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC));
        if (waited != 0) ctx->ssid.clear(); // timeout: handler may still be writing
    }

    QString result = ctx->ssid;
    if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        dispatch_release(ctx->sem);
    }
    return result;
}

// ���� Cellular info ������������������������������������������������������������������������������������������������������������������������

static NSString* radioAccessLabel(NSString* rat)
{
    if (!rat) return @"Unknown";
    if ([rat isEqualToString:CTRadioAccessTechnologyNRNSA] ||
        [rat isEqualToString:CTRadioAccessTechnologyNR])
        return @"5G";
    if ([rat isEqualToString:CTRadioAccessTechnologyLTE])
        return @"LTE";
    if ([rat isEqualToString:CTRadioAccessTechnologyWCDMA])
        return @"3G (WCDMA)";
    if ([rat isEqualToString:CTRadioAccessTechnologyHSDPA])
        return @"3G (HSDPA)";
    if ([rat isEqualToString:CTRadioAccessTechnologyHSUPA])
        return @"3G (HSUPA)";
    if ([rat isEqualToString:CTRadioAccessTechnologyCDMA1x])
        return @"2G (CDMA)";
    if ([rat isEqualToString:CTRadioAccessTechnologyCDMAEVDORev0] ||
        [rat isEqualToString:CTRadioAccessTechnologyCDMAEVDORevA] ||
        [rat isEqualToString:CTRadioAccessTechnologyCDMAEVDORevB])
        return @"3G (EV-iO)";
    if ([rat isEqualToString:CTRadioAccessTechnologyEdge])
        return @"2G (EiGE)";
    if ([rat isEqualToString:CTRadioAccessTechnologyGPRS])
        return @"2G (GPRS)";
    if ([rat isEqualToString:CTRadioAccessTechnologyeHRPD])
        return @"3G (eHRPD)";
    return rat;
}

// ���� MCC/MNC to carrier name lookup (fallback for iOS 16+) ������������������������������������������
// When CTCarrier.carrierName returns "--" on iOS 16+, query the MCC (Mobile Country
// Code) and MNC (Mobile Network Code) to identify the carrier from a mapping table.
// This lookup table contains the major carriers worldwide; you can extend it.
static QString mccMncToCarrier(const QString& mcc, const QString& mnc)
{
    // Format: "MCC-MNC" �� "Carrier Name"
    static const QMap<QString, QString> carriers = {
        // China
        {"460-00", "�й��ƶ� (China Mobile)"}, {"460-02", "�й��ƶ� (China Mobile)"},
        {"460-01", "�й���ͨ (China Unicom)"},
        {"460-03", "�й����� (China Telecom)"},
        // United States
        {"310-004", "Verizon"},   {"310-010", "Verizon"},   {"310-012", "Verizon"},
        {"310-013", "Verizon"},   {"310-014", "Verizon"},
        {"310-005", "AT&T"},      {"310-070", "AT&T"},      {"310-150", "AT&T"},
        {"310-160", "AT&T"},      {"310-170", "AT&T"},      {"310-200", "AT&T"},
        {"310-210", "AT&T"},      {"310-220", "AT&T"},
        {"310-026", "T-Mobile"},  {"310-160", "T-Mobile"},  {"310-200", "T-Mobile"},
        // UK
        {"234-03", "Vodafone"},   {"234-10", "Vodafone"},
        {"234-15", "Vodafone"},   {"234-30", "O2"},
        {"234-20", "Three"},      {"234-50", "Three"},
        // Germany
        {"262-01", "Telekom"},    {"262-02", "Vodafone"},   {"262-03", "E-Plus"},
        {"262-07", "Telef��nica"},
        // France
        {"208-01", "Orange"},     {"208-02", "SFR"},        {"208-03", "Bouygues"},
        // Japan
        {"440-10", "docomo"},     {"440-20", "SoftBank"},   {"440-50", "SoftBank"},
        {"440-04", "au"},         {"440-06", "au"},
        // South Korea
        {"450-02", "KT"},         {"450-04", "SK Telecom"}, {"450-08", "LG U+"},
        // India
        {"404-01", "Airtel"},     {"404-02", "Vodafone"},   {"404-03", "IDEA"},
        {"404-05", "Vodafone"},   {"404-09", "Jio"},
    };
    return carriers.value(mcc + "-" + mnc, QString());
}

// ���� WiFi info ������������������������������������������������������������������������������������������������������������������������������

QVariantMap iosWiFiInfo()
{
    QVariantMap info;

    // Reference-counted context: waiter and completion handler both hold a ref (2 total).
    // Store results as C++ QString (converted inside the handler) so no Objective-C
    // object owned by the handler's autorelease pool ever crosses the thread boundary.
    struct WifiCtx {
        dispatch_semaphore_t sem;
        QString ssid;
        QString bssid;
        std::atomic<int> refs;
    };
    auto ctx = std::make_shared<WifiCtx>();
    ctx->sem = dispatch_semaphore_create(0);
    ctx->refs.store(2, std::memory_order_relaxed);  // waiter + handler

    if (@available(iOS 14.0, *)) {
        @try {
        [NEHotspotNetwork fetchCurrentWithCompletionhandler:^(NEHotspotNetwork* _Nullable network) {
            @autoreleasepool {
                @try {
                if (network) {
                    if (network.SSID && network.SSID.length > 0)
                        ctx->ssid = QString::fromNSString(network.SSID);
                    if (network.BSSID && network.BSSID.length > 0)
                        ctx->bssid = QString::fromNSString(network.BSSID);
                }
                } @catch (NSException* e) { }
                dispatch_semaphore_signal(ctx->sem);
                // drop the handler's reference; last one out releases the semaphore.
                if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    dispatch_release(ctx->sem);
                }
            }

        } @catch (NSException* e) {
            dispatch_semaphore_signal(ctx->sem);
            if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                dispatch_release(ctx->sem);
            }
        }
        }];
        long waited = dispatch_semaphore_wait(ctx->sem, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC));
        // Only read result on success; on timeout the handler may still be writing it.
        if (waited != 0) {
            ctx->ssid.clear();
            ctx->bssid.clear();
        }
    }

    info["ssid"] = ctx->ssid;
    info["bssid"] = ctx->bssid;

    // Add Diagnostics
    if (ctx->ssid.isEmpty())
        info["wifiDiagnostics"] = QStringLiteral("WiFi: Not connected or permission denied (requires NSLocalNetworkUsageDescription + NSBonjourServices)");

    // drop the waiter's reference; last one out releases the semaphore.
    if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        dispatch_release(ctx->sem);
    }

    return info;
}

// ���� Cellular info ������������������������������������������������������������������������������������������������������������������������

QVariantMap iosCellularInfo()
{
    QVariantMap info;

    // Runs on a QtConcurrent worker thread with no autorelease pool of its own.
    // CTTelephonyNetworkInfo, its provider dictionary, and the NSStrings it returns
    // are all autoreleased; without an explicit pool they leak (Apple requires each
    // secondary thread that makes Cocoa calls to provide its own @autoreleasepool).
    @autoreleasepool {
        CTTelephonyNetworkInfo* netInfo = [[CTTelephonyNetworkInfo alloc] init];
        if (!netInfo) {
            info["error"] = QStringLiteral("failed to initialize CTTelephonyNetworkInfo");
            return info;
        }

        // iOS 12+: serviceSubscriberCellularProviders returns per-SIM carriers.
        // CTCarrier and its properties are deprecated since iOS 16.0 with no replacement.
        // We suppress the warnings and keep the best-effort implementation �� the values
        // will eventually return placeholder strings ("--", "65535") on future iOS versions.
        // On iOS 16+, when carrierName becomes "--", we fall back to MCC+MNC lookup.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        // Enumerate EVERY SIM / eSIM line. dual-SIM iPhones return one CTCarrier per
        // active subscription in serviceSubscriberCellularProviders, and
        // serviceCurrentRadioAccessTechnology is keyed by the SAME service identifiers,
        // so each SIM's radio-access type is matched by key.
        QVariantList sims;
        bool hasCarrier = false;
        if (@available(iOS 12.0, *)) {
            NSDictionary<NSString*, CTCarrier*>* providers = netInfo.serviceSubscriberCellularProviders;
            NSDictionary<NSString*, NSString*>* rats = netInfo.serviceCurrentRadioAccessTechnology;
            if (providers && providers.count > 0) {
                // dictionary order is undefined; sort keys for stable SIM slot numbers.
                NSArray<NSString*>* keys = [providers.allKeys sortedArrayUsingSelector:@selector(compare:)];
                int slot = 0;
                for (NSString* key in keys) {
                    CTCarrier* carrier = providers[key];
                    QVariantMap sim;
                    sim["slot"] = ++slot;

                    QString mccStr, mncStr, carrierName;
                    if (carrier) {
                        if (carrier.carrierName && carrier.carrierName.length > 0) {
                            QString cn = QString::fromNSString(carrier.carrierName);
                            if (cn != "--" && cn != "65535") carrierName = cn;
                        }
                        if (carrier.mobileCountryCode) {
                            QString v = QString::fromNSString(carrier.mobileCountryCode);
                            if (v != "65535") { mccStr = v; sim["mcc"] = v; }
                        }
                        if (carrier.mobileNetworkCode) {
                            QString v = QString::fromNSString(carrier.mobileNetworkCode);
                            if (v != "65535") { mncStr = v; sim["mnc"] = v; }
                        }
                        if (carrier.isoCountryCode)
                            sim["isoCountry"] = QString::fromNSString(carrier.isoCountryCode);
                    }
                    // iOS 16+ hides the carrier name ("--"); fall back to MCC+MNC lookup.
                    if (carrierName.isEmpty() && !mccStr.isEmpty() && !mncStr.isEmpty()) {
                        QString looked = mccMncToCarrier(mccStr, mncStr);
                        if (!looked.isEmpty()) carrierName = looked + " (via MCC/MNC)";
                    }
                    if (!carrierName.isEmpty()) { sim["carrierName"] = carrierName; hasCarrier = true; }

                    // Per-SIM radio access technology, matched by the same service key.
                    NSString* rat = rats ? rats[key] : nil;
                    if (rat) {
                        sim["radioAccess"] = QString::fromNSString(radioAccessLabel(rat));
                        sim["radioAccessRaw"] = QString::fromNSString(rat);
                    }
                    sims.append(sim);
                }
            }
        }
#pragma clang diagnostic pop

        info["simCount"] = static_cast<int>(sims.size());
        if (!sims.isEmpty()) {
            info["sims"] = sims;
            // Flat "primary" keys for backward-compatible summary / identity checks:
            // prefer the first SIM that actually has a carrier or an active radio.
            QVariantMap primary = sims.first().toMap();
            for (const QVariant& v : sims) {
                const QVariantMap m = v.toMap();
                if (m.contains(QStringLiteral("carrierName")) || m.contains(QStringLiteral("radioAccess"))) {
                    primary = m; break;
                }
            }
            const QStringList flat = {QStringLiteral("carrierName"), QStringLiteral("mcc"),
                                      QStringLiteral("mnc"), QStringLiteral("isoCountry"),
                                      QStringLiteral("radioAccess"), QStringLiteral("radioAccessRaw")};
            for (const QString& k : flat)
                if (primary.contains(k)) info[k] = primary.value(k);
        }

        if (!info.contains(QStringLiteral("radioAccess")) && !hasCarrier)
            info["cellularStatus"] = QStringLiteral("No cellular service available (airplane mode or no SIM)");

        [netInfo release]; // MRC: balance the alloc/init above
    }

    // Signal strength is not available via public API (iOS restricts this)
    info["signalNotice"] = QStringLiteral("Signal strength: unavailable (Apple restricts public API access)");
    info["signalNote"] = QStringLiteral("To monitor signal: use Xcode -> Simulator -> I/O -> Cellular");

    return info;
}

// 5WHY: iosCellularInfo() existed and worked but was never wired to
// TaskFactory after MVC refactoring.  G1CellularInfo was incorrectly
// marked as unsupported (skipped).  This wrapper converts QVariantMap
// → DiagnosticResult so TaskFactory can route to it on iOS.
DiagnosticResult __attribute__((used)) iosCellularDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QVariantMap info = iosCellularInfo();
    if (info.contains("error")) {
        r.status = DiagStatus::Error;
        r.summary = info["error"].toString();
        return r;
    }
    QStringList lines;
    lines.append(QString());
    lines.append(QStringLiteral("Cellular Information (iOS):"));
    lines.append(QString());
    if (info.contains("carrierName"))
        lines.append(QStringLiteral("  Carrier:     %1").arg(info["carrierName"].toString()));
    if (info.contains("mcc"))
        lines.append(QStringLiteral("  MCC:         %1").arg(info["mcc"].toString()));
    if (info.contains("mnc"))
        lines.append(QStringLiteral("  MNC:         %1").arg(info["mnc"].toString()));
    if (info.contains("radioAccess"))
        lines.append(QStringLiteral("  Radio:       %1").arg(info["radioAccess"].toString()));
    if (info.contains("simCount"))
        lines.append(QStringLiteral("  SIMs:        %1").arg(info["simCount"].toInt()));
    if (info.contains("cellularStatus"))
        lines.append(QStringLiteral("  Status:      %1").arg(info["cellularStatus"].toString()));
    lines.append(QString());
    if (info.contains("signalNotice"))
        lines.append(info["signalNotice"].toString());
    if (!info.contains("carrierName") && !info.contains("radioAccess"))
        r.status = DiagStatus::Info;
    else
        r.status = DiagStatus::Pass;
    r.summary = info.contains("carrierName") ? info["carrierName"].toString()
               : QStringLiteral("No cellular service");
    r.rawOutput = lines.join('\n');
    r.details = r.rawOutput;
    return r;
}

#endif // PLATFORM_IOS
