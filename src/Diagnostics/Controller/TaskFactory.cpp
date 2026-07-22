// =============================================================================
// TaskFactory.cpp 閳?Create DiagnosticTask objects for each DiagId
// =============================================================================
#include "Diagnostics/Controller/TaskFactory.h"
#include "Diagnostics/Model/G1/G1SystemAdapters.h"
#include "Diagnostics/Model/G2/G2Connectivity.h"
#include "Diagnostics/Model/G3/G3Diagnostics.h"
#include "Diagnostics/Model/G4/G4RemoteHost.h"
#include "Diagnostics/Model/NetworkProbe.h"
#include "Common/Utils/Logger.h"
// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?
// 5WHY: G5Common.h opens namespace G5WebsiteUrl { without closing it (it
// provides inline helper definitions used by multiple .cpp files). Each
// includer MUST close the namespace immediately to prevent leakage into
// subsequent code. The } below is NOT a stray brace 鈥?it balances the
// namespace opened by G5Common.h.
// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?
#if defined(PLATFORM_IOS)
#include "Diagnostics/Model/G5/G5Common.h"
} // namespace G5WebsiteUrl 鈫?closes G5Common.h's unclosed namespace
#include "Diagnostics/Model/G3/Platform/IOS/DnsResolve.h"
#include "Diagnostics/Model/G1/Platform/IOS/GatewayDhcpRouting.h"
#endif
#if defined(PLATFORM_ANDROID)
#include "Diagnostics/Model/G5/G5Common.h"
} // namespace G5WebsiteUrl — closes G5Common.h's unclosed namespace
#include "Diagnostics/Model/G5/Platform/Android/NetworkDiagnostics.h"
#endif
// Always included 鈥?contains only declarations + inline helpers. The #if
// blocks below decide which functions are actually routed to by DiagId.
#include "Diagnostics/Model/G5/G5WebsiteUrl.h"

// Per-test timeout values (ms). Default is 60000; shorter for fast tests.
static int timeoutFor(DiagId id) {
    switch (id) {
        case DiagId::G4Ping:            return 30000; // 4 probes at ~3s each
        case DiagId::G4Traceroute:      return 90000; // 30 hops at ~2s each
        case DiagId::G4PathPing:        return 120000;// traceroute + per-hop ping
        case DiagId::G3DnsIntegrity:   return 120000;// DoH queries × 5 domains + TLS
        case DiagId::G3InternetConnectivity:return 180000;// download + upload phases
        case DiagId::G5CurlVerbose:     return 120000;
        case DiagId::G5HttpTiming:      return 90000;
        default:                        return 60000;
    }
}

#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID)
// Diagnostics that cannot return useful data inside the mobile sandbox. They
// stay visible in the UI but are reported as Skipped; the detail page shows this
// reason. Tests that HAVE a working native/mobile implementation are absent here.
static QString platformSkipReason(DiagId id) {
#if defined(PLATFORM_IOS)
    switch (id) {
        case DiagId::G1NicAdvanced:
            return QStringLiteral("NIC driver properties (link speed, duplex, MAC address) are not exposed to sandboxed iOS apps.");
        case DiagId::G1WiredDiagnostics:
            return QStringLiteral("iOS devices have no wired Ethernet interface, and per-NIC statistics under /sys are inaccessible.");
        case DiagId::G1ActiveConnections:
            return QStringLiteral("Enumerating open sockets requires /proc/net/tcp, which the iOS sandbox blocks.");
        case DiagId::G2TcpSettings:
            return QStringLiteral("TCP kernel parameters live under /proc/sys/net, which is not readable on iOS.");
        case DiagId::G2ArpTable:
            return QStringLiteral("The ARP / neighbour table has no public API on iOS.");
        case DiagId::G2ProxySettings:
            return QStringLiteral("iOS uses a system-managed proxy (Wi-Fi PAC / VPN profile); environment-variable proxies do not apply and the active proxy is not readable by third-party apps.");
        case DiagId::G3NetskopeStatus:
            return QStringLiteral("Detecting a security-proxy agent requires enumerating running processes, which the iOS sandbox forbids.");
        case DiagId::G3DnsCache:
            return QStringLiteral("iOS does not expose the system DNS resolver cache to apps.");
        // 5WHY: Removed G1IpConfiguration/G3DnsServers/G3DnsIntegrity from
        // iOS skip list (commit 1b7e5d9 added them incorrectly).  Pre-MVC
        // (commit bd73d78) these ran via native G1G2G3Native functions which
        // return empty PASS on iOS — harmless, not crashes.  platformSkipReason()
        // did not exist pre-MVC.  Only skip tests that would crash/error on iOS.
        // 5WHY: G1CellularInfo routed to G1G2G3Native::cellularInfo() — no longer skipped.
        default:
            return QString();
    }
#else // PLATFORM_ANDROID
    switch (id) {
        case DiagId::G1NicAdvanced:
            return QStringLiteral("NIC driver properties are read from /sys/class/net, which is not accessible on Android.");
        case DiagId::G1WiredDiagnostics:
            return QStringLiteral("Android devices have no wired Ethernet interface, and /sys/class/net is inaccessible.");
        case DiagId::G1ActiveConnections:
            return QStringLiteral("Reading open sockets requires /proc/net/tcp, which is restricted on Android 10+.");
        case DiagId::G2TcpSettings:
            return QStringLiteral("TCP kernel parameters live under /proc/sys/net, which is not readable on Android.");
        case DiagId::G2RoutingTable:
            return QStringLiteral("The routing table is read from /proc/net/route, which is restricted on Android.");
        case DiagId::G2ArpTable:
            return QStringLiteral("The ARP table is read from /proc/net/arp, which is restricted on Android.");
        case DiagId::G2ProxySettings:
            return QStringLiteral("Android uses a system-managed proxy; environment-variable proxies do not apply.");
        case DiagId::G3NetskopeStatus:
            return QStringLiteral("Detecting a security-proxy agent requires enumerating running processes, which Android restricts.");
        case DiagId::G3DnsServers:
            return QStringLiteral("Android resolves DNS via ConnectivityManager; /etc/resolv.conf is not populated or readable by apps.");
        case DiagId::G3DnsCache:
            return QStringLiteral("Android does not expose the system DNS resolver cache to apps.");
        case DiagId::G3DnsIntegrity:
            return QStringLiteral("DNS integrity check requires reading system DNS resolver state, which is unavailable on Android.");
        default:
            return QString();
    }
#endif
}
#endif

std::unique_ptr<DiagnosticTask> TaskFactory::createTask(
    DiagId id, const QString& target)
{
    int tmo = timeoutFor(id);

    // Helpers: wrap function pointers that don't match GenericTask::Impl signature.
    // T1 wraps G1/G2/G3 閳?takes (DiagId) only, ignores target.
    // T2 wraps G4/G5 閳?takes (const QString&) only, ignores DiagId.
    auto T1 = [&](auto fn, int custTmo = -1) {
        return std::make_unique<GenericTask>(id, target,
            [id, fn](DiagId, const QString&) { return fn(id); },
            custTmo > 0 ? custTmo : tmo);
    };
    auto T2 = [&](auto fn, int custTmo = -1) {
        return std::make_unique<GenericTask>(id, target,
            [t = target, fn](DiagId, const QString&) { return fn(t); },
            custTmo > 0 ? custTmo : tmo);
    };
    // T3 for complex inline-lambda cases (e.g., PortScan)
    auto T3 = [&](GenericTask::Impl impl, int custTmo = -1) {
        return std::make_unique<GenericTask>(id, target, std::move(impl),
                                             custTmo > 0 ? custTmo : tmo);
    };

#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID)
    // Short-circuit platform-unsupported tests: show them as Skipped with an
    // explanation (detail page) instead of misleading empty/hardcoded output.
    if (const QString skipReason = platformSkipReason(id); !skipReason.isEmpty())
        return T3([id, skipReason](DiagId, const QString&) {
            return DiagnosticResult::skipped(id, skipReason);
        });
#endif

    switch (id) {
        // 閳光偓閳光偓 G1: System & Adapters 閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓
        case DiagId::G1NetworkAdapters:    return T1(G1G2G3Native::networkAdapters, 15000);
        case DiagId::G1NicAdvanced:        return T1(G1G2G3Native::nicAdvanced);
#if defined(PLATFORM_ANDROID)
        case DiagId::G1WifiDiagnostics:
            return T3([](DiagId id, const QString&) { return androidWifiDiag(id); });
#else
        case DiagId::G1WifiDiagnostics:    return T1(G1G2G3Native::wifiDiagnostics);
#endif
        case DiagId::G1WiredDiagnostics:   return T1(G1G2G3Native::wiredDiagnostics);
#if defined(PLATFORM_IOS)
        case DiagId::G1DhcpStatus:
            return T3([](DiagId id, const QString&) { return iosDhcpDiag(id); });
        // 5WHY: iosCellularDiag() produces simplified output (carrier/MCC/MNC only).
        // G1G2G3Native::cellularInfo() runs the FULL SIM iteration with IP/gateway
        // per interface, signal strength, and multi-SIM detail.  Route to the
        // native function so iOS users see the same rich output as desktop builds.
        case DiagId::G1CellularInfo:
            return T1(G1G2G3Native::cellularInfo);
#else
#if defined(PLATFORM_ANDROID)
        case DiagId::G1DhcpStatus:
            return T3([](DiagId id, const QString&) { return androidDhcpDiag(id); });
        case DiagId::G1CellularInfo:
            return T3([](DiagId id, const QString&) { return androidCellularDiag(id); });
#else
        case DiagId::G1DhcpStatus:         return T1(G1G2G3Native::dhcpStatus);
        case DiagId::G1CellularInfo:       return T1(G1G2G3Native::cellularInfo);
#endif
#endif  // closes #if defined(PLATFORM_IOS)
        // 5WHY: Moved OUT of #else so compiled on iOS.  Pre-MVC these ran
        // via native functions returning empty PASS on iOS — harmless.
        case DiagId::G1IpConfiguration:    return T1(G1G2G3Native::ipConfiguration);
        case DiagId::G1ActiveConnections:  return T1(G1G2G3Native::activeConnections);

        // 閳光偓閳光偓 G2: Connectivity & Security 閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓
        case DiagId::G2NetworkProfile:     return T1(G1G2G3Native::networkProfile);
        case DiagId::G2TcpSettings:        return T1(G1G2G3Native::tcpSettings);
#if defined(PLATFORM_IOS)
        case DiagId::G2DefaultGateway:
            return T3([](DiagId id, const QString&) { return iosDefaultGatewayDiag(id); });
#else
#if defined(PLATFORM_ANDROID)
        case DiagId::G2DefaultGateway:
            return T3([](DiagId id, const QString&) { return androidGatewayDiag(id); });
#else
        case DiagId::G2DefaultGateway:     return T1(G1G2G3Native::defaultGateway);
#endif
#endif  // 5WHY: closes #if defined(PLATFORM_IOS) at L182 — fix runaway #else blocking G2Routing-G5 on iOS
#if defined(PLATFORM_IOS)
        case DiagId::G2RoutingTable:
            return T3([](DiagId id, const QString&) { return iosRoutingTableDiag(id); });
#else
        case DiagId::G2RoutingTable:       return T1(G1G2G3Native::routingTable);
#endif
        case DiagId::G2ArpTable:           return T1(G1G2G3Native::arpTable);
        case DiagId::G2ProxySettings:      return T1(G1G2G3Native::proxySettings);

        // 閳光偓閳光偓 G3: Internet & DNS 閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓
        case DiagId::G3NetskopeStatus:     return T1(G1G2G3Native::netskopeStatus);
        case DiagId::G3DnsServers:         return T1(G1G2G3Native::dnsServers);
        case DiagId::G3DnsCache:           return T1(G1G2G3Native::dnsCache);
        case DiagId::G3DnsIntegrity:       return T1(G1G2G3Native::dnsIntegrity);
        case DiagId::G3GeoIPLoc:          return T1(G1G2G3Native::geoIPLoc);
        case DiagId::G3InternetConnectivity:  return T1(G1G2G3Native::internetConnectivity);

        // 閳光偓閳光偓 G4: Remote Host 閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓
#if defined(PLATFORM_IOS)
        case DiagId::G4DnsResolution:
            return T3([t = target](DiagId id, const QString&) { return iosDnsResolve(id, t, 3000); });
#else
#if defined(PLATFORM_ANDROID)
        case DiagId::G4DnsResolution:
            return T3([t = target](DiagId id, const QString&) { return androidDnsDiag(id, t); });
#else
        case DiagId::G4DnsResolution:      return T2(G4RemoteHost::dnsResolution);
#endif
#endif  // 5WHY: closes #if defined(PLATFORM_IOS) — missing #endif compiled out G4Ping-G5 on iOS
        case DiagId::G4Ping:               return T2(G4RemoteHost::ping, 30000);
        case DiagId::G4Traceroute:         return T2(G4RemoteHost::traceroute);
        case DiagId::G4PathPing:           return T2(G4RemoteHost::pathPing);
        case DiagId::G4MtuDiscovery:       return T2(G4RemoteHost::mtuDiscovery);

        // G4PortScan removed — port scan feature deprecated

        // ── G5 common socket-level tests (all platforms, no libcurl needed) ──
        // 5WHY: These 13 cases were copy-pasted 4× across iOS/Android/Desktop
        // blocks (~50 lines of duplication).  Extracted here so adding a new
        // protocol or changing routing updates one place.  Only curl-dependent
        // HTTP tests stay in the platform-specific blocks below.
        case DiagId::G5UrlParsing:       return T2(G5WebsiteUrl::urlParsing);
        case DiagId::G5TcpConnect:       return T2(G5WebsiteUrl::tcpConnect);
        case DiagId::G5ServiceBanner:    return T2(G5WebsiteUrl::serviceBanner);
        case DiagId::G5FtpDiagnostics:   return T2(G5WebsiteUrl::ftpDiagnostics);
        case DiagId::G5SshDiagnostics:   return T2(G5WebsiteUrl::sshDiagnostics);
        case DiagId::G5EmailDiagnostics: return T2(G5WebsiteUrl::emailDiagnostics);
        case DiagId::G5Telnet:           return T2(G5WebsiteUrl::telnetDiagnostics);
        case DiagId::G5Mysql:            return T2(G5WebsiteUrl::mysqlDiagnostics);
        case DiagId::G5Postgres:         return T2(G5WebsiteUrl::postgresDiagnostics);
        case DiagId::G5Redis:            return T2(G5WebsiteUrl::redisDiagnostics);
        case DiagId::G5Mongodb:          return T2(G5WebsiteUrl::mongodbDiagnostics);
        case DiagId::G5Ldap:             return T2(G5WebsiteUrl::ldapDiagnostics);
        case DiagId::G5Mqtt:             return T2(G5WebsiteUrl::mqttDiagnostics);

#if defined(PLATFORM_IOS)
        // iOS: curl-dependent HTTP via NSURLSession (iosHttpDiagnostic)
        // G5SslCertificate via QSslSocket (SecureTransport backend on iOS)
        case DiagId::G5SslCertificate:   return T2(G5WebsiteUrl::sslCertificate);
        case DiagId::G5CurlVerbose:
        case DiagId::G5HttpHeaders:
        case DiagId::G5HttpRedirect:
        case DiagId::G5SecurityHeaders:
        case DiagId::G5HttpCompression:
        case DiagId::G5HttpTiming:
            return T3([t = target](DiagId id, const QString&) { return iosHttpDiagnostic(id, t); });
#else
#if defined(PLATFORM_ANDROID)
        // Android: curl-dependent HTTP via HttpURLConnection JNI
        case DiagId::G5CurlVerbose:
        case DiagId::G5HttpHeaders:
        case DiagId::G5SslCertificate:
        case DiagId::G5HttpRedirect:
        case DiagId::G5HttpTiming:
        case DiagId::G5SecurityHeaders:
        case DiagId::G5HttpCompression:
            return T3([t = target](DiagId id, const QString&) { return androidHttpDiag(id, t); });
#else
#if !defined(NO_CURL)
        // Desktop with libcurl: full HTTP diagnostics
        case DiagId::G5CurlVerbose:      return T2(G5WebsiteUrl::curlVerbose);
        case DiagId::G5HttpHeaders:      return T2(G5WebsiteUrl::httpHeaders);
        case DiagId::G5SecurityHeaders:  return T2(G5WebsiteUrl::securityHeaders);
        case DiagId::G5SslCertificate:   return T2(G5WebsiteUrl::sslCertificate);
        case DiagId::G5HttpRedirect:     return T2(G5WebsiteUrl::httpRedirect);
        case DiagId::G5HttpCompression:  return T2(G5WebsiteUrl::httpCompression);
        case DiagId::G5HttpTiming:       return T2(G5WebsiteUrl::httpTiming);
#else
        // NO_CURL build: curl-dependent tests return Skipped
        case DiagId::G5SslCertificate:   return T2(G5WebsiteUrl::sslCertificate);
        case DiagId::G5CurlVerbose:      [[fallthrough]];
        case DiagId::G5HttpHeaders:      [[fallthrough]];
        case DiagId::G5SecurityHeaders:  [[fallthrough]];
        case DiagId::G5HttpRedirect:     [[fallthrough]];
        case DiagId::G5HttpCompression:  [[fallthrough]];
        case DiagId::G5HttpTiming:
            return T3([](DiagId id, const QString&) {
                return DiagnosticResult::skipped(id, QStringLiteral("HTTP test unavailable (libcurl required)"));
            });
#endif
#endif  // close converted #elif
#endif  // close converted #elif
        // 5WHY: default:break fell through to post-switch log+nullptr — misleading
        // because break suggests "do nothing."  Now explicitly logs inside default
        // AND keeps the post-switch return as a safety net for case-fallthrough UB.
        default:
            Logger::instance().event(QStringLiteral("Unknown DiagId: %1").arg(static_cast<int>(id)));
            return nullptr;
    }
    // Safety net: catch any case that falls through without returning (UB guard)
    Logger::instance().event(QStringLiteral("Unreachable: fell through switch for DiagId %1").arg(static_cast<int>(id)));
    return nullptr;
}
