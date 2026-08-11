// =============================================================================
// TaskFactory.cpp — Create DiagnosticTask objects for each DiagId
// =============================================================================
#include "Diagnostics/Controller/TaskFactory.h"
#include "Common/Model/DiagCapability.h"
#include "Common/Platform/DeviceCapability.h"
#include "Diagnostics/Model/G1/G1SystemAdapters.h"
#include "Diagnostics/Model/G2/G2Connectivity.h"
#include "Diagnostics/Model/G3/G3Diagnostics.h"
#include "Diagnostics/Model/G4/G4RemoteHost.h"
#include "Diagnostics/Model/NetworkProbe.h"
#include "Common/Utils/Logger.h"
// 5WHY: G5Common.h is now self-contained (closes its own namespace).
#if defined(PLATFORM_IOS)
#include "Diagnostics/Model/G5/G5Common.h"

#include "Diagnostics/Model/G3/Platform/IOS/DnsResolve.h"
#include "Diagnostics/Model/G1/Platform/IOS/GatewayDhcpRouting.h"
#endif
#if defined(PLATFORM_ANDROID)
#include "Diagnostics/Model/G5/G5Common.h"

#include "Diagnostics/Model/G5/Platform/Android/NetworkDiagnostics.h"
#endif
// Always included — contains only declarations + inline helpers. The #if
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

// ── Factory function type ──────────────────────────────────────────────────
using FactoryFn = std::function<std::unique_ptr<DiagnosticTask>(DiagId, const QString&, int)>;

// ── Factory helpers (preserve original T1/T2/T3 pattern) ───────────────────
// T1 wraps G1/G2/G3 — takes (DiagId) only, ignores target.
template<typename Fn>
static FactoryFn makeT1(Fn fn, int custTmo = -1) {
    return [fn, custTmo](DiagId id, const QString& target, int tmo) {
        return std::make_unique<GenericTask>(id, target,
            [id, fn](DiagId, const QString&) { return fn(id); },
            custTmo > 0 ? custTmo : tmo);
    };
}

// T2 wraps G4/G5 — takes (const QString&) only, ignores DiagId.
template<typename Fn>
static FactoryFn makeT2(Fn fn, int custTmo = -1) {
    return [fn, custTmo](DiagId id, const QString& target, int tmo) {
        return std::make_unique<GenericTask>(id, target,
            [t = target, fn](DiagId, const QString&) { return fn(t); },
            custTmo > 0 ? custTmo : tmo);
    };
}

// T3 for complex inline-lambda cases (e.g., platform-specific HTTP).
template<typename Impl>
static FactoryFn makeT3(Impl impl, int custTmo = -1) {
    return [impl = std::move(impl), custTmo](DiagId id, const QString& target, int tmo) mutable {
        return std::make_unique<GenericTask>(id, target, std::move(impl),
                                             custTmo > 0 ? custTmo : tmo);
    };
}

// ── Platform-specific factory builders ─────────────────────────────────────

#if defined(PLATFORM_IOS)
static FactoryFn g4DnsResolutionFactory() {
    return makeT3([](DiagId id, const QString& target) {
        return iosDnsResolve(id, target, 3000);
    });
}
static FactoryFn g5HttpFactory() {
    return makeT3([](DiagId id, const QString& target) {
        return iosHttpDiagnostic(id, target);
    });
}
#endif // PLATFORM_IOS

#if defined(PLATFORM_ANDROID)
static FactoryFn g4DnsResolutionFactory() {
    return makeT3([](DiagId id, const QString& target) {
        return androidDnsDiag(id, target);
    });
}
static FactoryFn g5HttpFactory() {
    return makeT3([](DiagId id, const QString& target) {
        return androidHttpDiag(id, target);
    });
}
#endif // PLATFORM_ANDROID

#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID)
#if defined(NO_CURL)
static FactoryFn g5NoCurlFactory() {
    return makeT3([](DiagId id, const QString&) {
        return DiagnosticResult::skipped(id, QStringLiteral("HTTP test unavailable (libcurl required)"));
    });
}
#endif // NO_CURL
#endif // !PLATFORM_IOS && !PLATFORM_ANDROID

// ── Task registration table — maps every DiagId → factory ──────────────────
// Linear scan O(n) for n=45 is acceptable.  Adding a new DiagId requires
// only one line here instead of touching 4–6 files.
struct TaskEntry {
    DiagId id;
    FactoryFn factory;
};

static const TaskEntry kTaskTable[] = {
    // ── G1: System & Adapters (8) ───────────────────────────────────────
    // 5WHY: Network adapter enumeration calls platform APIs (Win32 SetupDi,
    // sysfs on Linux, IOKit on macOS) that respond within 2-3 s. The 15 s
    // cap catches hung enumeration without blocking the pipeline for the
    // full default 60 s — a stuck NIC shouldn't stall the entire diagnostic.
#if defined(PLATFORM_ANDROID)
    // Android: every G1 test has a real JNI implementation
    // (Diagnostics/Model/G5/Platform/Android/NetworkDiagnostics.cpp).
    { DiagId::G1NetworkAdapters,   makeT1(androidNetworkAdaptersDiag, 15000) },
    { DiagId::G1NicAdvanced,       makeT1(androidNicAdvancedDiag) },
    { DiagId::G1WifiDiagnostics,   makeT1(androidWifiDiag) },
    { DiagId::G1WiredDiagnostics,  makeT1(androidWiredDiagnosticsDiag) },
    { DiagId::G1DhcpStatus,        makeT1(androidDhcpDiag) },
    { DiagId::G1IpConfiguration,   makeT1(androidIpConfigurationDiag) },
    { DiagId::G1ActiveConnections, makeT1(androidActiveConnectionsDiag) },
    { DiagId::G1CellularInfo,      makeT1(androidCellularDiag) },
#else
    { DiagId::G1NetworkAdapters,  makeT1(SystemDiagnostics::networkAdapters, 15000) },
    { DiagId::G1NicAdvanced,      makeT1(SystemDiagnostics::nicAdvanced) },
    { DiagId::G1WifiDiagnostics,  makeT1(SystemDiagnostics::wifiDiagnostics) },
    { DiagId::G1WiredDiagnostics, makeT1(SystemDiagnostics::wiredDiagnostics) },
#if defined(PLATFORM_IOS)
    { DiagId::G1DhcpStatus,       makeT1(iosDhcpDiag) },
#else
    { DiagId::G1DhcpStatus,       makeT1(SystemDiagnostics::dhcpStatus) },
#endif
    { DiagId::G1IpConfiguration,   makeT1(SystemDiagnostics::ipConfiguration) },
    { DiagId::G1ActiveConnections, makeT1(SystemDiagnostics::activeConnections) },
    { DiagId::G1CellularInfo,      makeT1(SystemDiagnostics::cellularInfo) },
#endif

    // ── G2: Connectivity & Security (6) ──────────────────────────────────
#if defined(PLATFORM_ANDROID)
    // Android: routing/proxy/ARP/TCP-state all have real implementations.
    { DiagId::G2NetworkProfile,  makeT1(androidNetworkProfileDiag) },
    { DiagId::G2TcpSettings,     makeT1(androidTcpSettingsDiag) },
    { DiagId::G2DefaultGateway,  makeT1(androidGatewayDiag) },
    { DiagId::G2RoutingTable,    makeT1(androidRoutingTableDiag) },
    { DiagId::G2ArpTable,        makeT1(androidArpTableDiag) },
    { DiagId::G2ProxySettings,   makeT1(androidProxyDiag) },
#else
    { DiagId::G2NetworkProfile,  makeT1(SystemDiagnostics::networkProfile) },
    { DiagId::G2TcpSettings,     makeT1(SystemDiagnostics::tcpSettings) },
#if defined(PLATFORM_IOS)
    { DiagId::G2DefaultGateway,  makeT1(iosDefaultGatewayDiag) },
#else
    { DiagId::G2DefaultGateway,  makeT1(SystemDiagnostics::defaultGateway) },
#endif
#if defined(PLATFORM_IOS)
    { DiagId::G2RoutingTable,    makeT1(iosRoutingTableDiag) },
#else
    { DiagId::G2RoutingTable,    makeT1(SystemDiagnostics::routingTable) },
#endif
    { DiagId::G2ArpTable,        makeT1(SystemDiagnostics::arpTable) },
    { DiagId::G2ProxySettings,   makeT1(SystemDiagnostics::proxySettings) },
#endif

    // ── G3: Internet & DNS (6) ───────────────────────────────────────────
#if defined(PLATFORM_ANDROID)
    // Android: DNS servers (ConnectivityManager), cache (warm/cold probe),
    // integrity (portable scoring engine).
    { DiagId::G3DnsServers,            makeT1(androidDnsServersDiag) },
    { DiagId::G3DnsCache,              makeT1(androidDnsCacheDiag) },
    { DiagId::G3DnsIntegrity,          makeT1(SystemDiagnostics::dnsIntegrity) },
#else
    // 5WHY: iOS G3DnsServers was routed to g3DnsServersFactory() → iosDnsResolve(),
    // which resolves a HOSTNAME.  G3 tests have no target (makeT1 pattern), so it
    // resolved an empty host → always SERVFAIL "DNS Resolution Failed for"
    // (crashes/Weixin Image_20260807095048_133_1.jpg).  SystemDiagnostics::dnsServers()
    // already enumerates real system DNS servers on iOS via res_ninit — use it everywhere.
    { DiagId::G3DnsServers,            makeT1(SystemDiagnostics::dnsServers) },
    { DiagId::G3DnsCache,              makeT1(SystemDiagnostics::dnsCache) },
    { DiagId::G3DnsIntegrity,          makeT1(SystemDiagnostics::dnsIntegrity) },
#endif
    { DiagId::G3GeoIPLoc,              makeT1(SystemDiagnostics::geoIPLoc) },
    { DiagId::G3InternetConnectivity,  makeT1(SystemDiagnostics::internetConnectivity) },

    // ── G4: Remote Host (6) ──────────────────────────────────────────────
#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID)
    { DiagId::G4DnsResolution,    g4DnsResolutionFactory() },
#else
    { DiagId::G4DnsResolution,    makeT2(G4RemoteHost::dnsResolution) },
#endif
    { DiagId::G4Ping,             makeT2(G4RemoteHost::ping, 30000) },
    { DiagId::G4IPv6Connectivity, makeT2(G4RemoteHost::ipv6Connectivity) },
    { DiagId::G4Traceroute,       makeT2(G4RemoteHost::traceroute) },
    { DiagId::G4PathPing,         makeT2(G4RemoteHost::pathPing) },
    { DiagId::G4MtuDiscovery,     makeT2(G4RemoteHost::mtuDiscovery) },

    // ── G5: Protocol — common socket-level (13) ──────────────────────────
    // 5WHY: These 13 entries were previously copy-pasted 4× across
    // iOS/Android/Desktop blocks.  Extracted here so adding a new protocol
    // or changing routing updates one place.
    { DiagId::G5UrlParsing,       makeT2(G5WebsiteUrl::urlParsing) },
    { DiagId::G5TcpConnect,       makeT2(G5WebsiteUrl::tcpConnect) },
    { DiagId::G5ServiceBanner,    makeT2(G5WebsiteUrl::serviceBanner) },
    { DiagId::G5FtpDiagnostics,   makeT2(G5WebsiteUrl::ftpDiagnostics) },
    { DiagId::G5SshDiagnostics,   makeT2(G5WebsiteUrl::sshDiagnostics) },
    { DiagId::G5EmailDiagnostics, makeT2(G5WebsiteUrl::emailDiagnostics) },
    { DiagId::G5Telnet,           makeT2(G5WebsiteUrl::telnetDiagnostics) },
    { DiagId::G5Mysql,            makeT2(G5WebsiteUrl::mysqlDiagnostics) },
    { DiagId::G5Postgres,         makeT2(G5WebsiteUrl::postgresDiagnostics) },
    { DiagId::G5Redis,            makeT2(G5WebsiteUrl::redisDiagnostics) },
    { DiagId::G5Mongodb,          makeT2(G5WebsiteUrl::mongodbDiagnostics) },
    { DiagId::G5Ldap,             makeT2(G5WebsiteUrl::ldapDiagnostics) },
    { DiagId::G5Mqtt,             makeT2(G5WebsiteUrl::mqttDiagnostics) },

    // ── G5: Protocol — HTTP (curl-dependent, 7) ──────────────────────────
#if defined(PLATFORM_IOS)
    // iOS: G5SslCertificate via QSslSocket; others via NSURLSession
    { DiagId::G5SslCertificate,   makeT2(G5WebsiteUrl::sslCertificate) },
    { DiagId::G5CurlVerbose,      g5HttpFactory() },
    { DiagId::G5HttpHeaders,      g5HttpFactory() },
    { DiagId::G5HttpRedirect,     g5HttpFactory() },
    { DiagId::G5SecurityHeaders,  g5HttpFactory() },
    { DiagId::G5HttpCompression,  g5HttpFactory() },
    { DiagId::G5HttpTiming,       g5HttpFactory() },
#else
#if defined(PLATFORM_ANDROID)
    // Android: curl-dependent HTTP via HttpURLConnection JNI
    { DiagId::G5CurlVerbose,      g5HttpFactory() },
    { DiagId::G5HttpHeaders,      g5HttpFactory() },
    { DiagId::G5SslCertificate,   g5HttpFactory() },
    { DiagId::G5HttpRedirect,     g5HttpFactory() },
    { DiagId::G5HttpTiming,       g5HttpFactory() },
    { DiagId::G5SecurityHeaders,  g5HttpFactory() },
    { DiagId::G5HttpCompression,  g5HttpFactory() },
#else // Desktop
#if !defined(NO_CURL)
    // Desktop with libcurl: full HTTP diagnostics
    { DiagId::G5CurlVerbose,      makeT2(G5WebsiteUrl::curlVerbose) },
    { DiagId::G5HttpHeaders,      makeT2(G5WebsiteUrl::httpHeaders) },
    { DiagId::G5SecurityHeaders,  makeT2(G5WebsiteUrl::securityHeaders) },
    { DiagId::G5SslCertificate,   makeT2(G5WebsiteUrl::sslCertificate) },
    { DiagId::G5HttpRedirect,     makeT2(G5WebsiteUrl::httpRedirect) },
    { DiagId::G5HttpCompression,  makeT2(G5WebsiteUrl::httpCompression) },
    { DiagId::G5HttpTiming,       makeT2(G5WebsiteUrl::httpTiming) },
#else
    // NO_CURL build: curl-dependent tests return Skipped
    { DiagId::G5SslCertificate,   makeT2(G5WebsiteUrl::sslCertificate) },
    { DiagId::G5CurlVerbose,      g5NoCurlFactory() },
    { DiagId::G5HttpHeaders,      g5NoCurlFactory() },
    { DiagId::G5SecurityHeaders,  g5NoCurlFactory() },
    { DiagId::G5HttpRedirect,     g5NoCurlFactory() },
    { DiagId::G5HttpCompression,  g5NoCurlFactory() },
    { DiagId::G5HttpTiming,       g5NoCurlFactory() },
#endif // !NO_CURL
#endif // PLATFORM_ANDROID
#endif // PLATFORM_IOS / Desktop
};

// ── createTask — lookup table, call factory, apply timeout ──────────────────
std::unique_ptr<DiagnosticTask> TaskFactory::createTask(
    DiagId id, const QString& target)
{
    int tmo = timeoutFor(id);

    // 5WHY (defensive guard, platform-independent): platform/device-impossible
    // tests are filtered out BEFORE scheduling (AppState::runDiagnostics + the
    // Config page via DeviceCapability::diagRunnable), so createTask is never
    // called for them in the normal flow.  If ANY out-of-band caller (future
    // code, harness) still asks for one, short-circuit to a Skipped result
    // with the reason instead of running empty/hardcoded output.  Guarding on
    // ALL platforms (not just PLATFORM_MOBILE) also protects desktop from
    // device-unsupported tests such as G1CellularInfo on a modem-less machine.
    if (!diagSupportedOnPlatform(id)
        || !DeviceCapability::diagSupportedOnDevice(id)) {
        const QString skipReason = unsupportedReason(id);
        return std::make_unique<GenericTask>(id, target,
            [id, skipReason](DiagId, const QString&) {
                return DiagnosticResult::skipped(id, skipReason);
            }, tmo);
    }

    // Linear scan the static table — O(n) for n=45 is acceptable
    for (const auto& entry : kTaskTable) {
        if (entry.id == id) {
            return entry.factory(id, target, tmo);
        }
    }

    Logger::instance().event(QStringLiteral("Unknown DiagId: %1").arg(static_cast<int>(id)));
    return nullptr;
}
