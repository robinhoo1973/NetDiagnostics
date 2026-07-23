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
#include <QList>
#include <cstddef>
#include "Common/Model/DiagnosticResult.h"
#include "Diagnostics/Model/G1/Platform/IOS/GatewayDhcpRouting.h" // 5WHY: own header for declaration checking
#include "Diagnostics/View/DiagnosticFormatter.h"
#include "Common/Utils/Logger.h"

// Round a sockaddr length up to the next 4-byte boundary (BSD routing alignment).
#if !defined(SA_SIZE)
#define SA_SIZE(sa) \
    ( ((sa) == nullptr || ((struct sockaddr*)(sa))->sa_len == 0) ? \
        sizeof(uint32_t) : \
        (1 + ((((struct sockaddr*)(sa))->sa_len - 1) | (sizeof(uint32_t) - 1))) )
#endif

// -- Routing table via sysctl NET_RT_DUMP2 --
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
// Public (non-static): declared in G1Ios.h, called from SystemDiagnostics.
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

// -- default Gateway: real IP from the routing table, fallback to interface --
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

// DHCP Status (iOS: always system-managed)
// 5WHY: Used plain text formatting — table view was desktop-only.
// On iOS, TaskFactory routes to iosDhcpDiag() which calls this function,
// NOT G1DhcpStatus.cpp. Now uses DiagnosticFormatter::formatTable for
// consistent table display across ALL platforms.
static QString iosDHCPStatus() {
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("DHCP Client Status"));
    out.append(QString());

    // Column spec matching G1DhcpStatus.cpp for cross-platform consistency
    static const QVector<DiagnosticFormatter::ColSpec> kDhcpCols = {
        {"Interface", 18, false},
        {"DHCP",       6, false},
        {"IP Address", 18, false},
        {"Server",     0, false},
    };

    QList<QStringList> rows;
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
            rows.append({name, "Yes", QString::fromLatin1(ip), "(system)"});
        }
        freeifaddrs(ifa);
    }

    if (!rows.isEmpty())
        out << DiagnosticFormatter::formatTable(kDhcpCols, rows);
    out.append(QString());
    out.append(QStringLiteral("  iOS manages DHCP at the system level —"));
    out.append(QStringLiteral("  lease details are not accessible to third-party apps."));
    return out.join('\n');
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
    out.append(QStringLiteral("IPv4 Route Table (iOS -- sysctl NET_RT_DUMP2)"));
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
// WiFi: CNCopyCurrentNetworkInfo (sync, V1) for iOS 9–25 SDK;
//   falls back to NEHotspotNetwork (async) when building with iOS 26+ SDK
//   where CNCopyCurrentNetworkInfo has been removed.
// Cellular: CTTelephonyNetworkInfo + CTCarrier
// =============================================================================

#if defined(PLATFORM_IOS)

#include <QSet>
#include <QString>
#include <QVariantMap>
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
    QString result;
    Logger::instance().event("iosCopyWiFiSSID() called");

    // NEHotspotNetwork — Apple's recommended iOS 14+ WiFi API.
    // Requires: location (WhenInUse) + wifi-info entitlement.
    __block NSString* ssid = nil;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);

    @try {
        [NEHotspotNetwork fetchCurrentWithCompletionHandler:^(NEHotspotNetwork* _Nullable network) {
            @autoreleasepool {
                if (network && network.SSID.length > 0)
                    ssid = [network.SSID copy];
                dispatch_semaphore_signal(sem);
            }
        }];
    } @catch (NSException* e) {
        Logger::instance().warn(QString("iosCopyWiFiSSID: exception: %1")
            .arg(QString::fromNSString(e.reason ?: @"(no reason)")));
        dispatch_semaphore_signal(sem);
    }
    long waited = dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));
    if (waited != 0) Logger::instance().warn("iosCopyWiFiSSID: semaphore timeout (5s)");

    if (ssid && ssid.length > 0)
        result = QString::fromNSString(ssid);

    if (result.isEmpty())
        Logger::instance().warn("iosCopyWiFiSSID: returned empty");
    else
        Logger::instance().event(QString("iosCopyWiFiSSID: returned '%1'").arg(result));
    return result;
}

//���� Cellular info ������������������������������������������������������������������������������������������������������������������������

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
        {"460-00", "China Mobile"}, {"460-02", "China Mobile"},
        {"460-01", "China Unicom"},
        {"460-03", "China Telecom"},
        // United States
        {"310-004", "Verizon"},   {"310-010", "Verizon"},   {"310-012", "Verizon"},
        {"310-013", "Verizon"},   {"310-014", "Verizon"},
        {"310-005", "AT&T"},      {"310-070", "AT&T"},      {"310-150", "AT&T"},
        {"310-170", "AT&T"},      {"310-210", "AT&T"},      {"310-220", "AT&T"},
        {"310-026", "T-Mobile"},  {"310-160", "T-Mobile"},  {"310-200", "T-Mobile"},
        // UK
        {"234-03", "Vodafone"},   {"234-10", "Vodafone"},
        {"234-15", "Vodafone"},   {"234-30", "O2"},
        {"234-20", "Three"},      {"234-50", "Three"},
        // Germany
        {"262-01", "Telekom"},    {"262-02", "Vodafone"},   {"262-03", "E-Plus"},
        {"262-07", "Telefonica"},
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
    Logger::instance().event("iosWiFiInfo() called — starting WiFi retrieval");

    // NEHotspotNetwork — Apple's recommended iOS 14+ WiFi API.
    // Requires: location (WhenInUse) + wifi-info entitlement.
    Logger::instance().event("iosWiFiInfo: using NEHotspotNetwork");
    __block NSString* ssid = nil;
    __block NSString* bssid = nil;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);

    @try {
        [NEHotspotNetwork fetchCurrentWithCompletionHandler:^(NEHotspotNetwork* _Nullable network) {
            @autoreleasepool {
                if (network) {
                    if (network.SSID && network.SSID.length > 0)
                        ssid = [network.SSID copy];
                    if (network.BSSID && network.BSSID.length > 0)
                        bssid = [network.BSSID copy];
                }
                dispatch_semaphore_signal(sem);
            }
        }];
    } @catch (NSException* e) {
        Logger::instance().warn(QString("iosWiFiInfo: exception: %1")
            .arg(QString::fromNSString(e.reason ?: @"(no reason)")).toUtf8().constData());
        dispatch_semaphore_signal(sem);
    }
    long waited = dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));
    if (waited != 0) Logger::instance().warn("iosWiFiInfo: semaphore timeout (5s)");

    if (ssid && ssid.length > 0) {
        info["ssid"] = QString::fromNSString(ssid);
        Logger::instance().event("iosWiFiInfo: SSID retrieved");
    } else {
        Logger::instance().warn("iosWiFiInfo: SSID is nil/empty");
    }
    if (bssid && bssid.length > 0)
        info["bssid"] = QString::fromNSString(bssid);

    // 5WHY: Check authorization status and log detailed permission state.
    // Declare locStr here so it's visible in both the log block AND the
    // diagnostic message block below (autorized-but-no-SSID case).
    const char* locStr = "Unknown";
    {
        CLAuthorizationStatus locStatus = [CLLocationManager authorizationStatus];
        switch (locStatus) {
            case kCLAuthorizationStatusNotDetermined: locStr = "NotDetermined"; break;
            case kCLAuthorizationStatusRestricted:     locStr = "Restricted"; break;
            case kCLAuthorizationStatusDenied:         locStr = "Denied"; break;
            case kCLAuthorizationStatusAuthorizedWhenInUse: locStr = "AuthorizedWhenInUse"; break;
            case kCLAuthorizationStatusAuthorizedAlways:    locStr = "AuthorizedAlways"; break;
        }
        Logger::instance().event(QString("iosWiFiInfo: CLLocationManager authorizationStatus = %1 (%2)")
            .arg((int)locStatus).arg(locStr));
    }

    if (!info.contains("ssid") || info["ssid"].toString().isEmpty()) {
        CLAuthorizationStatus locStatus = [CLLocationManager authorizationStatus];
        QString diagMsg;
        switch (locStatus) {
            case kCLAuthorizationStatusNotDetermined:
                diagMsg = QStringLiteral("WiFi SSID: Location permission not yet requested. "
                                         "Go to Settings > Privacy > Location Services and enable for NetDiagnostics.");
                Logger::instance().warn("iosWiFiInfo: location authorization NotDetermined");
                break;
            case kCLAuthorizationStatusRestricted:
                diagMsg = QStringLiteral("WiFi SSID: Location services are restricted "
                                         "(parental controls or MDM profile). Contact your administrator.");
                Logger::instance().warn("iosWiFiInfo: location authorization Restricted");
                break;
            case kCLAuthorizationStatusDenied:
                diagMsg = QStringLiteral("WiFi SSID: Location permission was denied. "
                                         "Go to Settings > Privacy > Location Services > NetDiagnostics and select 'While Using'.");
                Logger::instance().warn("iosWiFiInfo: location authorization Denied");
                break;
            case kCLAuthorizationStatusAuthorizedWhenInUse:
            case kCLAuthorizationStatusAuthorizedAlways:
                diagMsg = QStringLiteral(
                    "WiFi SSID: Not available despite location permission being granted. "
                    "1) Verify WiFi is connected in Settings > Wi-Fi. "
                    "2) The 'Access WiFi Information' capability must be enabled in "
                    "Xcode > Signing & Capabilities (requires paid Apple Developer account). "
                    "3) Check that the provisioning profile includes the "
                    "com.apple.developer.networking.wifi-info entitlement. "
                    "Without this, the WiFi API returns nil even with location access.");
                Logger::instance().warn(QString("iosWiFiInfo: location authorized (%1) but SSID empty — possible missing entitlement or WiFi not connected")
                    .arg(locStr));
                break;
            default:
                diagMsg = QStringLiteral("WiFi SSID: Unable to determine location authorization status.");
                Logger::instance().warn(QString("iosWiFiInfo: unknown location authorization status %1").arg((int)locStatus));
                break;
        }
        info["wifiDiagnostics"] = diagMsg;
    } else {
        Logger::instance().event("iosWiFiInfo: SUCCESS — SSID retrieved");
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
                    // 5WHY: rats[key] can be nil for the ACTIVE data SIM because iOS
                    // sometimes uses a different service-identifier format for the
                    // data line than for subscriber cellular providers.  Inactive
                    // SIMs get matched (their key is stable), active SIM gets nil.
                    // Fallback: if direct key match fails, scan all rats entries
                    // and match by radio-access label if only one unmatched entry
                    // remains — this catches the orphaned active-SIM RAT.
                    NSString* rat = rats ? rats[key] : nil;
                    if (rat) {
                        sim["radioAccess"] = QString::fromNSString(radioAccessLabel(rat));
                        sim["radioAccessRaw"] = QString::fromNSString(rat);
                        sim["_ratMatched"] = true;
                    } else {
                        sim["_ratMatched"] = false;
                    }
                    sims.append(sim);
                }
                // Second pass: if any SIM lacks a RAT match, scan rats for
                // orphaned entries and assign them to unmatched SIMs.
                // 5WHY: Used a QSet<QString> of already-assigned raw RAT strings
                // to skip duplicates.  When both SIMs are on the same technology
                // (e.g. both "LTE"), the second entry was skipped because its
                // raw value matched the first — leaving the active-SIM unmatched.
                // Now tracks which rats KEYS were consumed instead of raw values.
                bool hasUnmatched = false;
                for (int i = 0; i < sims.size(); ++i)
                    if (!sims[i].toMap().value("_ratMatched").toBool()) { hasUnmatched = true; break; }
                if (hasUnmatched && rats && rats.count > 0) {
                    QSet<NSString*> consumedKeys;
                    for (NSString* rk in rats) {
                        if (!rats[rk]) continue;
                        if (consumedKeys.contains(rk)) continue;
                        consumedKeys.insert(rk);
                        // Assign this rats entry to the first unmatched SIM
                        for (int i = 0; i < sims.size(); ++i) {
                            QVariantMap m = sims[i].toMap();
                            if (!m.value("_ratMatched").toBool()) {
                                m["radioAccess"] = QString::fromNSString(radioAccessLabel(rats[rk]));
                                m["radioAccessRaw"] = QString::fromNSString(rats[rk]);
                                m["_ratMatched"] = true;
                                sims[i] = m;
                                break;
                            }
                        }
                    }
                }
                // Clean up internal sentinel key
                for (int i = 0; i < sims.size(); ++i) {
                    QVariantMap m = sims[i].toMap();
                    m.remove("_ratMatched");
                    sims[i] = m;
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

        // ARC manages NSObject lifetime — no manual release needed.
    }

    // Signal strength is not available via public API (iOS restricts this)
    info["signalNotice"] = QStringLiteral("Signal strength: unavailable (Apple restricts public API access)");
    info["signalNote"] = QStringLiteral("To monitor signal: use Xcode -> Simulator -> I/O -> Cellular");

    return info;
}

// 5WHY: iosCellularInfo() existed and worked but was never wired to
// TaskFactory after MVC refactoring.  G1CellularInfo was incorrectly
// 5WHY: iosCellularDiag() REMOVED — SystemDiagnostics::cellularInfo()
// provides richer output with SIM iteration, IP/gateway per interface,
// and signal strength.  TaskFactory now routes directly to it.

#endif // PLATFORM_IOS
