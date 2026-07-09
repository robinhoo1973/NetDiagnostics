// =============================================================================
// TaskFactory.cpp 閳?Create DiagnosticTask objects for each DiagId
// =============================================================================
#include "engine/task/TaskFactory.h"
#include "engine/diagnostics/G1/G1SystemAdapters.h"
#include "engine/diagnostics/G2/G2Connectivity.h"
#include "engine/diagnostics/G3/G3InternetDns.h"
#include "engine/diagnostics/G4/G4RemoteHost.h"
#include "engine/diagnostics/NetworkProbe.h"
#include "util/Logger.h"
#include <QElapsedTimer>
#include <QDateTime>
// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?
// 5WHY: G5Common.h opens namespace G5WebsiteUrl { without closing it (it
// provides inline helper definitions used by multiple .cpp files). Each
// includer MUST close the namespace immediately to prevent leakage into
// subsequent code. The } below is NOT a stray brace 鈥?it balances the
// namespace opened by G5Common.h.
// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?
#ifdef PLATFORM_IOS
#include "engine/diagnostics/G5/G5Common.h"
} // namespace G5WebsiteUrl 鈫?closes G5Common.h's unclosed namespace
#include "engine/diagnostics/G3/G3Common.h"
#include "engine/diagnostics/G1/G1Common.h"
#endif
#ifdef PLATFORM_ANDROID
#include "engine/diagnostics/G5/G5Common.h"
} // namespace G5WebsiteUrl 鈫?closes G5Common.h's unclosed namespace
#include "engine/diagnostics/G1/G1Common.h"
#endif
// Always included 鈥?contains only declarations + inline helpers. The #if
// blocks below decide which functions are actually routed to by DiagId.
#include "engine/diagnostics/G5/G5WebsiteUrl.h"

// Per-test timeout values (ms). Default is 60000; shorter for fast tests.
static int timeoutFor(DiagId id) {
    switch (id) {
        case DiagId::G4Ping:            return 30000; // 4 probes at ~3s each
        case DiagId::G4Traceroute:      return 90000; // 30 hops at ~2s each
        case DiagId::G4PathPing:        return 120000;// traceroute + per-hop ping
        case DiagId::G3InternetSpeedTest:return 180000;// download + upload phases
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
        default:
            return QString();
    }
#endif
}
#endif

std::unique_ptr<DiagnosticTask> TaskFactory::createTask(
    DiagId id, const QString& target, int fromPort, int toPort, bool useCommonPorts)
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
#ifdef PLATFORM_ANDROID
        case DiagId::G1WifiDiagnostics:
            return T3([](DiagId id, const QString&) { return G5WebsiteUrl::androidWifiDiag(id); });
#else
        case DiagId::G1WifiDiagnostics:    return T1(G1G2G3Native::wifiDiagnostics);
#endif
        case DiagId::G1WiredDiagnostics:   return T1(G1G2G3Native::wiredDiagnostics);
#ifdef PLATFORM_IOS
        case DiagId::G1DhcpStatus:
            return T3([](DiagId id, const QString&) { return iosDhcpDiag(id); });
#elif defined(PLATFORM_ANDROID)
        case DiagId::G1DhcpStatus:
            return T3([](DiagId id, const QString&) { return G5WebsiteUrl::androidDhcpDiag(id); });
#else
        case DiagId::G1DhcpStatus:         return T1(G1G2G3Native::dhcpStatus);
#endif
        case DiagId::G1IpConfiguration:    return T1(G1G2G3Native::ipConfiguration);
        case DiagId::G1ActiveConnections:  return T1(G1G2G3Native::activeConnections);
#ifdef PLATFORM_ANDROID
        case DiagId::G1CellularInfo:
            return T3([](DiagId id, const QString&) { return G5WebsiteUrl::androidCellularDiag(id); });
#else
        case DiagId::G1CellularInfo:       return T1(G1G2G3Native::cellularInfo);
#endif

        // 閳光偓閳光偓 G2: Connectivity & Security 閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓
        case DiagId::G2NetworkProfile:     return T1(G1G2G3Native::networkProfile);
        case DiagId::G2TcpSettings:        return T1(G1G2G3Native::tcpSettings);
#ifdef PLATFORM_IOS
        case DiagId::G2DefaultGateway:
            return T3([](DiagId id, const QString&) { return iosDefaultGatewayDiag(id); });
#elif defined(PLATFORM_ANDROID)
        case DiagId::G2DefaultGateway:
            return T3([](DiagId id, const QString&) { return G5WebsiteUrl::androidGatewayDiag(id); });
#else
        case DiagId::G2DefaultGateway:     return T1(G1G2G3Native::defaultGateway);
#endif
#ifdef PLATFORM_IOS
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
        case DiagId::G3DnsPollution:       return T1(G1G2G3Native::dnsPollution);
        case DiagId::G3InternetSpeedTest:  return T1(G1G2G3Native::speedTest);

        // 閳光偓閳光偓 G4: Remote Host 閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓閳光偓
#ifdef PLATFORM_IOS
        case DiagId::G4DnsResolution:
            return T3([t = target](DiagId id, const QString&) { return iosDnsResolve(id, t, 3000); });
#elif defined(PLATFORM_ANDROID)
        case DiagId::G4DnsResolution:
            return T3([t = target](DiagId id, const QString&) { return G5WebsiteUrl::androidDnsDiag(id, t); });
#else
        case DiagId::G4DnsResolution:      return T2(G4RemoteHost::dnsResolution);
#endif
        case DiagId::G4Ping:               return T2(G4RemoteHost::ping, 30000);
        case DiagId::G4Traceroute:         return T2(G4RemoteHost::traceroute);
        case DiagId::G4PathPing:           return T2(G4RemoteHost::pathPing);
        case DiagId::G4MtuDiscovery:       return T2(G4RemoteHost::mtuDiscovery);

        // G4PortScan removed 鈥?port scan feature deprecated

#ifdef PLATFORM_IOS
        // iOS: NSURLSession native HTTP (no libcurl needed)
        case DiagId::G5UrlParsing:       return T2(G5WebsiteUrl::urlParsing);
        case DiagId::G5TcpConnect:       return T2(G5WebsiteUrl::tcpConnect);
        case DiagId::G5ServiceBanner:    return T2(G5WebsiteUrl::serviceBanner);
        // Full X.509 certificate details via QSslSocket (SecureTransport backend on iOS)
        case DiagId::G5SslCertificate:   return T2(G5WebsiteUrl::sslCertificate);
        case DiagId::G5Telnet:           return T2(G5WebsiteUrl::telnetDiagnostics);
        case DiagId::G5Mysql:            return T2(G5WebsiteUrl::mysqlDiagnostics);
        case DiagId::G5Postgres:         return T2(G5WebsiteUrl::postgresDiagnostics);
        case DiagId::G5Redis:            return T2(G5WebsiteUrl::redisDiagnostics);
        case DiagId::G5Mongodb:          return T2(G5WebsiteUrl::mongodbDiagnostics);
        case DiagId::G5Ldap:             return T2(G5WebsiteUrl::ldapDiagnostics);
        case DiagId::G5Mqtt:             return T2(G5WebsiteUrl::mqttDiagnostics);
        case DiagId::G5CurlVerbose:
        case DiagId::G5HttpHeaders:
        case DiagId::G5HttpRedirect:
        case DiagId::G5SecurityHeaders:
        case DiagId::G5HttpCompression:
        case DiagId::G5HttpTiming:
            return T3([t = target](DiagId id, const QString&) { return iosHttpDiagnostic(id, t); });
        case DiagId::G5FtpDiagnostics:   return T2(G5WebsiteUrl::ftpDiagnostics);
        case DiagId::G5SshDiagnostics:   return T2(G5WebsiteUrl::sshDiagnostics);
        case DiagId::G5EmailDiagnostics: return T2(G5WebsiteUrl::emailDiagnostics);
#elif defined(PLATFORM_ANDROID)
        // Android: HttpURLConnection native HTTP (no libcurl needed)
        case DiagId::G5UrlParsing:       return T2(G5WebsiteUrl::urlParsing);
        case DiagId::G5TcpConnect:       return T2(G5WebsiteUrl::tcpConnect);
        case DiagId::G5ServiceBanner:    return T2(G5WebsiteUrl::serviceBanner);
        case DiagId::G5Telnet:           return T2(G5WebsiteUrl::telnetDiagnostics);
        case DiagId::G5Mysql:            return T2(G5WebsiteUrl::mysqlDiagnostics);
        case DiagId::G5Postgres:         return T2(G5WebsiteUrl::postgresDiagnostics);
        case DiagId::G5Redis:            return T2(G5WebsiteUrl::redisDiagnostics);
        case DiagId::G5Mongodb:          return T2(G5WebsiteUrl::mongodbDiagnostics);
        case DiagId::G5Ldap:             return T2(G5WebsiteUrl::ldapDiagnostics);
        case DiagId::G5Mqtt:             return T2(G5WebsiteUrl::mqttDiagnostics);
        case DiagId::G5CurlVerbose:
        case DiagId::G5HttpHeaders:
        case DiagId::G5SslCertificate:
        case DiagId::G5HttpRedirect:
        case DiagId::G5HttpTiming:
        case DiagId::G5SecurityHeaders:
        case DiagId::G5HttpCompression:
            return T3([t = target](DiagId id, const QString&) { return G5WebsiteUrl::androidHttpDiag(id, t); });
        case DiagId::G5FtpDiagnostics:   return T2(G5WebsiteUrl::ftpDiagnostics);
        case DiagId::G5SshDiagnostics:   return T2(G5WebsiteUrl::sshDiagnostics);
        case DiagId::G5EmailDiagnostics: return T2(G5WebsiteUrl::emailDiagnostics);
#elif !defined(NO_CURL)
        case DiagId::G5UrlParsing:       return T2(G5WebsiteUrl::urlParsing);
        case DiagId::G5TcpConnect:       return T2(G5WebsiteUrl::tcpConnect);
        case DiagId::G5ServiceBanner:    return T2(G5WebsiteUrl::serviceBanner);
        case DiagId::G5CurlVerbose:      return T2(G5WebsiteUrl::curlVerbose);
        case DiagId::G5HttpHeaders:      return T2(G5WebsiteUrl::httpHeaders);
        case DiagId::G5SecurityHeaders:  return T2(G5WebsiteUrl::securityHeaders);
        case DiagId::G5SslCertificate:   return T2(G5WebsiteUrl::sslCertificate);
        case DiagId::G5HttpRedirect:     return T2(G5WebsiteUrl::httpRedirect);
        case DiagId::G5HttpCompression:  return T2(G5WebsiteUrl::httpCompression);
        case DiagId::G5HttpTiming:       return T2(G5WebsiteUrl::httpTiming);
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
#else
        // 鈹€鈹€ NO_CURL build: socket-only tests (no libcurl needed) 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
        case DiagId::G5UrlParsing:       return T2(G5WebsiteUrl::urlParsing);
        case DiagId::G5TcpConnect:       return T2(G5WebsiteUrl::tcpConnect);
        case DiagId::G5ServiceBanner:    return T2(G5WebsiteUrl::serviceBanner);
        case DiagId::G5SslCertificate:   return T2(G5WebsiteUrl::sslCertificate);
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
        // 鈹€鈹€ libcurl-only tests 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
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
        default: break; // safety net: new DiagId not yet handled 鈫?log + nullptr
    }
    Logger::instance().event(QStringLiteral("Unknown DiagId: %1").arg(static_cast<int>(id)));
    return nullptr;
}
