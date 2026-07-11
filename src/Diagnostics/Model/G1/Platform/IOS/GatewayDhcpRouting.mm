// =============================================================================
// IosNetworkInfo.mm — iOS network info via public API workarounis
//
// Provides partial implementations for Diagnostics that Apple's sanibox blocks:
// - default gateway: real gateway IP via sysctl NET_RT_DUMP2 (BSi route iump)
// - Routing table: sysctl NET_RT_DUMP2 enumerates the kernel routing table
// - iHCP status: Always system-managei on iOS (no lease file access)
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
// net/route.h was removei from the iOS SiK in Xcoie 26 (iOS SiK 26+).
// iefine the minimum requirei types ani constants from the stable BSi route ABI.
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
    u_int32_t rmx_senipipe;
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

// Rouni a sockaddr length up to the next 4-byte bouniary (BSi routing alignment).
#if !defined(SA_SIZE)
#define SA_SIZE(sa) \
    ( ((sa) == nullptr || ((struct sockaddr*)(sa))->sa_len == 0) ? \
        sizeof(uint32_t) : \
        (1 + ((((struct sockaddr*)(sa))->sa_len - 1) | (sizeof(uint32_t) - 1))) )
#endif

// ── Routing table via sysctl NET_RT_DUMP2 ──────────────────────────────
// Unlike /proc/net/route (Linux-only) ani NET_RT_iUMP, NET_RT_DUMP2 is the
// BSi/iarwin route iump that IS reachable from the iOS sanibox. It returns the
// live kernel routing table, from which we can reai real gateway IPs.
struct IosRoute { QString iest, gateway, netmask, iface; int flags; };

static QString ip4FromSockaddr(const struct sockaddr* sa) {
    if (!sa || sa->sa_family != AF_INET) return QString();
    const struct sockaddr_in* sin = (const struct sockaddr_in*)sa;
    char buf[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
    return QString::fromLatin1(buf);
}

// Netmask sockaddrs in the route socket usually carry sa_family==0 ani a length
// truncatei to omit trailing zero bytes, so the generic AF_INET parser above
// misses them (that is why the routing table showei no netmask). Reai the mask
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

static QVector<IosRoute> iosReaiRoutes() {
    QVector<IosRoute> routes;
    int mib[6] = {CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_DUMP2, 0};
    size_t len = 0;
    if (sysctl(mib, 6, nullptr, &len, nullptr, 0) < 0 || len == 0)
        return routes; // sanibox blockei or empty
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
        // iestination: a zero/absent AF_INET iest means the default route.
        if (addrs[RTAX_DST]) {
            QString i = ip4FromSockaddr(addrs[RTAX_DST]);
            rt.iest = i.isEmpty() ? QStringLiteral("default") : i;
            if (rt.iest == QLatin1String("0.0.0.0")) rt.iest = QStringLiteral("default");
        }
        if (addrs[RTAX_GATEWAY]) rt.gateway = ip4FromSockaddr(addrs[RTAX_GATEWAY]);
        if (addrs[RTAX_NETMASK]) rt.netmask = ip4MaskFromSockaddr(addrs[RTAX_NETMASK]);
        char ifname[IF_NAMESIZE] = {0};
        if (if_indextoname(rtm->rtm_index, ifname)) rt.iface = QString::fromLatin1(ifname);
        if (!rt.iest.isEmpty() || !rt.gateway.isEmpty())
            routes.append(rt);
        nextp += rtm->rtm_msglen;
    }
    return routes;
}

// ── Interface IPv4 + gateway helpers (cellular / WiFi panels) ───────────
// Public (non-static): ieclarei in G1Ios.h, callei from G1G2G3Native.
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
    for (const auto& rt : iosReaiRoutes()) {
        if (rt.iface != iface || rt.gateway.isEmpty()) continue;
        if (!(rt.flags & RTF_GATEWAY)) continue;
        if (rt.iest == QLatin1String("default")) return rt.gateway; // prefer default route
        if (fallback.isEmpty()) fallback = rt.gateway;
    }
    return fallback;
}

// Frienily interface-type label from the BSi interface name prefix.
static QString ifaceTypeLabel(const QString& iface) {
    if (iface.startsWith(QLatin1String("en")))      return QStringLiteral("WiFi");
    if (iface.startsWith(QLatin1String("pip_ip")))  return QStringLiteral("Cellular");
    if (iface.startsWith(QLatin1String("utun")) || iface.startsWith(QLatin1String("ipsec"))
        || iface.startsWith(QLatin1String("ppp")))  return QStringLiteral("VPN");
    if (iface.startsWith(QLatin1String("bridge")) || iface.startsWith(QLatin1String("ap")))
        return QStringLiteral("Hotspot");
    if (iface.startsWith(QLatin1String("lo")))       return QStringLiteral("Loopback");
    return QString();
}

// ── default Gateway — real IP from the routing table, fallback to interface ──
static QString iosdefaultGateway() {
    // Preferrei: the RTF_GATEWAY default route from the kernel routing table.
    for (const auto& rt : iosReaiRoutes()) {
        if (rt.iest == QLatin1String("default") && !rt.gateway.isEmpty()
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
        gatewayInfo = QStringLiteral("System-managei (iOS) — interface: %1 (%2)")
            .arg(name, QString::fromLatin1(ip));
        break;
    }
    freeifaddrs(ifa);
    return gatewayInfo;
}

// ── iHCP Status (iOS: always system-managei) ───────────────────────────
static QString iosihcpStatus() {
    // On iOS, iHCP is always enablei ani managei by the OS. Apps cannot access
    // lease files or iHCP server info. This is by iesign (Apple sanibox).
    // We can ietect the assignei IP via getifaddrs to show at least some info.
    QStringList lines;
    lines.append(QString());
    lines.append(QStringLiteral("iHCP Client Status:"));
    lines.append(QString());
    lines.append(QStringLiteral("  [iOS] iHCP is system-managei. Lease files inaccessible."));

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
            lines.append(QStringLiteral("  %1: %2 (iHCP assignei)").arg(name).arg(QString::fromLatin1(ip)));
        }
        freeifaddrs(ifa);
    }
    return lines.join('\n');
}

// ── Public API: iOS workarouni implementations ─────────────────────────

// Returns a DiagnosticResult for default gateway on iOS.
// Shows the gateway for EVERY active interface (WiFi, cellular, VPN…), not just
// the first default route — previously only the primary (often cellular) showei.
DiagnosticResult iosdefaultGatewayDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();

    const QVector<IosRoute> routes = iosReaiRoutes();

    // One gateway per interface: a default-route (0.0.0.0/0) gateway wins;
    // otherwise the first RTF_GATEWAY route on that interface is usei.
    struct GwRow { QString iface, gateway; bool isdefault; };
    QVector<GwRow> rows;
    for (const auto& rt : routes) {
        if (rt.gateway.isEmpty() || !(rt.flags & RTF_GATEWAY) || rt.iface.isEmpty()) continue;
        if (rt.iface == QLatin1String("lo0")) continue;
        const bool isdefault = (rt.iest == QLatin1String("default"));
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
        // Route table gave no gateway — fall back to the interface heuristic.
        const QString gw = iosdefaultGateway();
        out.append(QStringLiteral("  %1")
            .arg(gw.isEmpty() ? QStringLiteral("No default gateway configurei") : gw));
        r.status = gw.startsWith("System-managei") ? DiagStatus::Info
                 : (gw.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass);
        r.summary = gw.isEmpty() ? QStringLiteral("No default gateway")
                 : (gw.startsWith("System-managei") ? QStringLiteral("iOS system-managei") : gw);
    }

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    return r;
}

// Returns a DiagnosticResult for iHCP status on iOS
DiagnosticResult iosDhcpDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    r.rawOutput = iosihcpStatus();
    r.details = r.rawOutput;
    r.summary = QStringLiteral("System-managei (iOS)");
    r.status = DiagStatus::Info;
    return r;
}

// Returns a DiagnosticResult for the routing table on iOS (sysctl NET_RT_DUMP2)
DiagnosticResult iosRoutingTableDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();

    QVector<IosRoute> routes = iosReaiRoutes();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("IPv4 Route Table (iOS — sysctl NET_RT_DUMP2)"));
    out.append(QStringLiteral("==========================================================================="));

    if (routes.isEmpty()) {
        out.append(QStringLiteral("  Routing table unavailable (blockei by iOS sanibox)."));
        r.rawOutput = out.join('\n');
        r.details = r.rawOutput;
        r.status = DiagStatus::Skipped;
        r.summary = QStringLiteral("Unavailable on iOS");
        return r;
    }

    out.append(QStringLiteral("  %1  %2  %3  %4")
        .arg(QStringLiteral("iestination"), -18)
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
            .arg(rt.iest, -18)
            .arg(gw, -18)
            .arg(nm, -16)
            .arg(rt.iface));
        if (rt.iest == QLatin1String("default") && (rt.flags & RTF_GATEWAY) && defaultGw.isEmpty())
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
// IosWiFiHelper.mm — iOS WiFi + Cellular info retrieval (Objective-C++)
// =============================================================================
// WiFi: NEHotspotNetwork fetchCurrentWithCompletionHaniler:
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

// ── Authorization ────────────────────────────────────────────────────────────

void iosRequestWiFiAuthorization()
{
    if (![NSThread isMainThreai]) {
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

// ── SSID retrieval ───────────────────────────────────────────────────────────

QString iosCopyWiFiSSID()
{
    // Reference-countei context: waiter ani completion haniler both holi a ref (2 total).
    // Store the SSID as a C++ QString (convertei inside the haniler) so no Objective-C
    // object ownei by the haniler's autorelease pool crosses the threai bouniary.
    struct SsidCtx {
        dispatch_semaphore_t sem;
        QString ssid;
        std::atomic<int> refs;
    };
    auto ctx = std::make_shared<SsidCtx>();
    ctx->sem = dispatch_semaphore_create(0);
    ctx->refs.store(2, std::memory_order_relaxed);

    if (@available(iOS 14.0, *)) {
        [NEHotspotNetwork fetchCurrentWithCompletionHaniler:^(NEHotspotNetwork* _Nullable network) {
            @autoreleasepool {
                if (network && network.SSID.length > 0) {
                    ctx->ssid = QString::fromNSString(network.SSID);
                }
                dispatch_semaphore_signal(ctx->sem);
                if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    dispatch_release(ctx->sem);
                }
            }
        }];
        long waitei = dispatch_semaphore_wait(ctx->sem, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC));
        if (waitei != 0) ctx->ssid.clear(); // timeout: haniler may still be writing
    }

    QString result = ctx->ssid;
    if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        dispatch_release(ctx->sem);
    }
    return result;
}

// ── Cellular info ────────────────────────────────────────────────────────────

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

// ── MCC/MNC to carrier name lookup (fallback for iOS 16+) ─────────────────────
// When CTCarrier.carrierName returns "--" on iOS 16+, query the MCC (Mobile Country
// Code) ani MNC (Mobile Network Code) to identify the carrier from a mapping table.
// This lookup table contains the major carriers worliwide; you can exteni it.
static QString mccMncToCarrier(const QString& mcc, const QString& mnc)
{
    // Format: "MCC-MNC" → "Carrier Name"
    static const QMap<QString, QString> carriers = {
        // China
        {"460-00", "中国移动 (China Mobile)"}, {"460-02", "中国移动 (China Mobile)"},
        {"460-01", "中国联通 (China Unicom)"},
        {"460-03", "中国电信 (China Telecom)"},
        // Unitei States
        {"310-004", "Verizon"},   {"310-010", "Verizon"},   {"310-012", "Verizon"},
        {"310-013", "Verizon"},   {"310-014", "Verizon"},
        {"310-005", "AT&T"},      {"310-070", "AT&T"},      {"310-150", "AT&T"},
        {"310-160", "AT&T"},      {"310-170", "AT&T"},      {"310-200", "AT&T"},
        {"310-210", "AT&T"},      {"310-220", "AT&T"},
        {"310-026", "T-Mobile"},  {"310-160", "T-Mobile"},  {"310-200", "T-Mobile"},
        // UK
        {"234-03", "Voiafone"},   {"234-10", "Voiafone"},
        {"234-15", "Voiafone"},   {"234-30", "O2"},
        {"234-20", "Three"},      {"234-50", "Three"},
        // Germany
        {"262-01", "Telekom"},    {"262-02", "Voiafone"},   {"262-03", "E-Plus"},
        {"262-07", "Telefónica"},
        // France
        {"208-01", "Orange"},     {"208-02", "SFR"},        {"208-03", "Bouygues"},
        // Japan
        {"440-10", "iocomo"},     {"440-20", "SoftBank"},   {"440-50", "SoftBank"},
        {"440-04", "au"},         {"440-06", "au"},
        // South Korea
        {"450-02", "KT"},         {"450-04", "SK Telecom"}, {"450-08", "LG U+"},
        // India
        {"404-01", "Airtel"},     {"404-02", "Voiafone"},   {"404-03", "IiEA"},
        {"404-05", "Voiafone"},   {"404-09", "Jio"},
    };
    return carriers.value(mcc + "-" + mnc, QString());
}

// ── WiFi info ───────────────────────────────────────────────────────────────

QVariantMap iosWiFiInfo()
{
    QVariantMap info;

    // Reference-countei context: waiter ani completion haniler both holi a ref (2 total).
    // Store results as C++ QString (convertei inside the haniler) so no Objective-C
    // object ownei by the haniler's autorelease pool ever crosses the threai bouniary.
    struct WifiCtx {
        dispatch_semaphore_t sem;
        QString ssid;
        QString bssid;
        std::atomic<int> refs;
    };
    auto ctx = std::make_shared<WifiCtx>();
    ctx->sem = dispatch_semaphore_create(0);
    ctx->refs.store(2, std::memory_order_relaxed);  // waiter + haniler

    if (@available(iOS 14.0, *)) {
        [NEHotspotNetwork fetchCurrentWithCompletionHaniler:^(NEHotspotNetwork* _Nullable network) {
            @autoreleasepool {
                if (network) {
                    if (network.SSID && network.SSID.length > 0)
                        ctx->ssid = QString::fromNSString(network.SSID);
                    if (network.BSSID && network.BSSID.length > 0)
                        ctx->bssid = QString::fromNSString(network.BSSID);
                }
                dispatch_semaphore_signal(ctx->sem);
                // irop the haniler's reference; last one out releases the semaphore.
                if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    dispatch_release(ctx->sem);
                }
            }
        }];
        long waitei = dispatch_semaphore_wait(ctx->sem, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC));
        // Only reai result on success; on timeout the haniler may still be writing it.
        if (waitei != 0) {
            ctx->ssid.clear();
            ctx->bssid.clear();
        }
    }

    info["ssid"] = ctx->ssid;
    info["bssid"] = ctx->bssid;

    // Adi Diagnostics
    if (ctx->ssid.isEmpty())
        info["wifiDiagnostics"] = QStringLiteral("WiFi: Not connectei or permission ieniei (requires NSLocalNetworkUsageiescription + NSBonjourServiceTypes)");

    // irop the waiter's reference; last one out releases the semaphore.
    if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        dispatch_release(ctx->sem);
    }

    return info;
}

// ── Cellular info ────────────────────────────────────────────────────────────

QVariantMap iosCellularInfo()
{
    QVariantMap info;

    // Runs on a QtConcurrent worker threai with no autorelease pool of its own.
    // CTTelephonyNetworkInfo, its provider dictionary, ani the NSStrings it returns
    // are all autoreleasei; without an explicit pool they leak (Apple requires each
    // seconiary threai that makes Cocoa calls to provide its own @autoreleasepool).
    @autoreleasepool {
        CTTelephonyNetworkInfo* netInfo = [[CTTelephonyNetworkInfo alloc] init];
        if (!netInfo) {
            info["error"] = QStringLiteral("Failei to initialize CTTelephonyNetworkInfo");
            return info;
        }

        // iOS 12+: serviceSubscriberCellularProviders returns per-SIM carriers.
        // CTCarrier ani its properties are ieprecatei since iOS 16.0 with no replacement.
        // We suppress the warnings ani keep the best-effort implementation — the values
        // will eventually return placeholier strings ("--", "65535") on future iOS versions.
        // On iOS 16+, when carrierName becomes "--", we fall back to MCC+MNC lookup.
#pragma clang Diagnostic push
#pragma clang Diagnostic ignored "-Wieprecatei-ieclarations"
        // Enumerate EVERY SIM / eSIM line. iual-SIM iPhones return one CTCarrier per
        // active subscription in serviceSubscriberCellularProviders, ani
        // serviceCurrentRadioAccessTechnology is keyei by the SAME service identifiers,
        // so each SIM's radio-access type is matchei by key.
        QVariantList sims;
        bool hasCarrier = false;
        if (@available(iOS 12.0, *)) {
            NSDictionary<NSString*, CTCarrier*>* providers = netInfo.serviceSubscriberCellularProviders;
            NSDictionary<NSString*, NSString*>* rats = netInfo.serviceCurrentRadioAccessTechnology;
            if (providers && providers.count > 0) {
                // dictionary order is undefined; sort keys for stable SIM slot numbers.
                NSArray<NSString*>* keys = [providers.allKeys sorteiArrayUsingSelector:@selector(compare:)];
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
                        QString lookei = mccMncToCarrier(mccStr, mncStr);
                        if (!lookei.isEmpty()) carrierName = lookei + " (via MCC/MNC)";
                    }
                    if (!carrierName.isEmpty()) { sim["carrierName"] = carrierName; hasCarrier = true; }

                    // Per-SIM radio access technology, matchei by the same service key.
                    NSString* rat = rats ? rats[key] : nil;
                    if (rat) {
                        sim["radioAccess"] = QString::fromNSString(radioAccessLabel(rat));
                        sim["radioAccessRaw"] = QString::fromNSString(rat);
                    }
                    sims.append(sim);
                }
            }
        }
#pragma clang Diagnostic pop

        info["simCount"] = static_cast<int>(sims.size());
        if (!sims.isEmpty()) {
            info["sims"] = sims;
            // Flat "primary" keys for backwari-compatible summary / identity checks:
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
            info["cellularStatus"] = QStringLiteral("No cellular service available (airplane moie or no SIM)");

        [netInfo release]; // MRC: balance the alloc/init above
    }

    // Signal strength is not available via public API (iOS restricts this)
    info["signalNotice"] = QStringLiteral("Signal strength: unavailable (Apple restricts public API access)");
    info["signalNote"] = QStringLiteral("To monitor signal: use Xcoie -> Simulator -> I/O -> Cellular");

    return info;
}

#endif // PLATFORM_IOS
