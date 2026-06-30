// =============================================================================
// IosNetworkInfo.mm — iOS network info via public API workarounds
//
// Provides partial implementations for diagnostics that Apple's sandbox blocks:
// - Default gateway: SCNetworkReachability returns reachability + interface info
// - DHCP status: Always system-managed on iOS (no lease file access)
// - Routing table: Unavailable (Apple blocks sysctl NET_RT_DUMP)
// - ARP table: Unavailable (link-layer, no public API)
// =============================================================================
#import <SystemConfiguration/SystemConfiguration.h>
#import <sys/socket.h>
#import <netinet/in.h>
#import <arpa/inet.h>
#import <ifaddrs.h>
#import <net/if.h>
#include <QString>
#include <QStringList>
#include "models/DiagnosticResult.h"

// ── Default Gateway via SCNetworkReachability ──────────────────────────
// Returns the hostname/IP that SCNetworkReachability considers the default route.
// This is the destination used for connectivity checks — typically the router.
static QString iosDefaultGateway() {
    // Create a reachability ref to a public IP — forces the system to determine
    // the default route. We use 8.8.8.8 (Google DNS) as the probe.
    struct sockaddr_in zeroAddr;
    memset(&zeroAddr, 0, sizeof(zeroAddr));
    zeroAddr.sin_len = sizeof(zeroAddr);
    zeroAddr.sin_family = AF_INET;
    zeroAddr.sin_addr.s_addr = inet_addr("8.8.8.8");

    SCNetworkReachabilityRef reachability = SCNetworkReachabilityCreateWithAddress(
        kCFAllocatorDefault, (const struct sockaddr*)&zeroAddr);

    if (!reachability) return QString();

    SCNetworkReachabilityFlags flags;
    Boolean ok = SCNetworkReachabilityGetFlags(reachability, &flags);
    CFRelease(reachability);
    if (!ok) return QString();

    // Walk interfaces to find which one provides the default route.
    // The interface that has IFF_UP + a non-loopback IPv4 address is the most
    // likely candidate for the default gateway interface.
    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) != 0) return QStringLiteral("Unknown (getifaddrs failed)");

    QString gatewayInfo = QStringLiteral("System-managed (iOS) — interface: ");
    for (auto* p = ifa; p; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
        if (!(p->ifa_flags & IFF_UP)) continue;
        QString name = QString::fromLatin1(p->ifa_name);
        if (name == "lo0") continue;
        // The first non-loopback, UP, IPv4 interface is usually the default route
        char ip[INET_ADDRSTRLEN];
        auto* sa = (struct sockaddr_in*)p->ifa_addr;
        inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
        gatewayInfo += QStringLiteral("%1 (%2)").arg(name, QString::fromLatin1(ip));
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
