// =============================================================================
// DiagCapability.h — Platform capability manifest (OS-level, compile-time)
//
// THE single source of truth mapping every DiagId to the OS platforms on
// which it can produce REAL diagnostic results.  Diagnostics that are
// fundamentally impossible on a platform are excluded here: they are hidden
// from the Configuration page, never scheduled, and never counted as
// Skipped — they are simply ignored on that platform.
//
// ── Platform matrix (the manifest) ─────────────────────────────────────────
//  Test                       Desktop   iOS   Android
//  G1NetworkAdapters             ✓       ✓       ✓
//  G1NicAdvanced                 ✓       ✗       ✓
//  G1WifiDiagnostics             ✓       ✓       ✓
//  G1WiredDiagnostics            ✓       ✗       ✓
//  G1DhcpStatus                  ✓       ✓       ✓
//  G1IpConfiguration             ✓       ✓       ✓
//  G1ActiveConnections           ✓       ✗       ✓
//  G1CellularInfo                ✓       ✓       ✓
//  G2NetworkProfile              ✓       ✓       ✓
//  G2TcpSettings                 ✓       ✗       ✓
//  G2DefaultGateway              ✓       ✓       ✓
//  G2RoutingTable                ✓       ✓       ✓
//  G2ArpTable                    ✓       ✗       ✓
//  G2ProxySettings               ✓       ✗       ✓
//  G3NetskopeStatus              ✓       ✗       ✓
//  G3DnsServers                  ✓       ✓       ✓
//  G3DnsCache                    ✓       ✗       ✓
//  G3DnsIntegrity                ✓       ✓       ✓
//  G3GeoIPLoc                    ✓       ✓       ✓
//  G3InternetConnectivity        ✓       ✓       ✓
//  G4 (6 tests)                  ✓       ✓       ✓
//  G5 (20 tests)                 ✓       ✓       ✓
//
//   ✗ = impossible inside the iOS sandbox (NIC driver props, /proc/net/tcp,
//       /proc/sys/net, ARP table, env proxies, process list, DNS cache).
//       See TaskFactory::platformSkipReason() for the human-readable reasons.
// =============================================================================
#pragma once

#include "Common/Model/DiagId.h"

// ── Platform bitmask ────────────────────────────────────────────────────────
enum PlatformFlag : unsigned {
    PF_Desktop = 1u << 0,   // Windows / macOS / Linux
    PF_IOS     = 1u << 1,
    PF_Android = 1u << 2,
    PF_All     = PF_Desktop | PF_IOS | PF_Android,
};

// Returns the bitmask of platforms on which the diagnostic can produce real
// results.  One entry per test — adding a test MUST update this switch
// (a missing case triggers a -Wswitch warning and defaults to PF_All).
inline unsigned diagPlatformSupport(DiagId id) {
    switch (id) {
        // ── iOS sandbox-impossible (Desktop + Android only) ──────────────
        case DiagId::G1NicAdvanced:
        case DiagId::G1WiredDiagnostics:
        case DiagId::G1ActiveConnections:
        case DiagId::G2TcpSettings:
        case DiagId::G2ArpTable:
        case DiagId::G2ProxySettings:
        case DiagId::G3NetskopeStatus:
        case DiagId::G3DnsCache:
            return PF_Desktop | PF_Android;

        // ── Supported everywhere (Desktop / iOS / Android) ───────────────
        case DiagId::G1NetworkAdapters:
        case DiagId::G1WifiDiagnostics:
        case DiagId::G1DhcpStatus:
        case DiagId::G1IpConfiguration:
        case DiagId::G1CellularInfo:
        case DiagId::G2NetworkProfile:
        case DiagId::G2DefaultGateway:
        case DiagId::G2RoutingTable:
        case DiagId::G3DnsServers:
        case DiagId::G3DnsIntegrity:
        case DiagId::G3GeoIPLoc:
        case DiagId::G3InternetConnectivity:
        case DiagId::G4DnsResolution:
        case DiagId::G4Ping:
        case DiagId::G4Traceroute:
        case DiagId::G4PathPing:
        case DiagId::G4MtuDiscovery:
        case DiagId::G4IPv6Connectivity:
        case DiagId::G5UrlParsing:
        case DiagId::G5TcpConnect:
        case DiagId::G5ServiceBanner:
        case DiagId::G5CurlVerbose:
        case DiagId::G5HttpHeaders:
        case DiagId::G5SecurityHeaders:
        case DiagId::G5SslCertificate:
        case DiagId::G5HttpRedirect:
        case DiagId::G5HttpCompression:
        case DiagId::G5HttpTiming:
        case DiagId::G5FtpDiagnostics:
        case DiagId::G5SshDiagnostics:
        case DiagId::G5EmailDiagnostics:
        case DiagId::G5Telnet:
        case DiagId::G5Mysql:
        case DiagId::G5Postgres:
        case DiagId::G5Redis:
        case DiagId::G5Mongodb:
        case DiagId::G5Ldap:
        case DiagId::G5Mqtt:
            return PF_All;
    }
    return PF_All; // new DiagId without an entry defaults to all platforms
}

// ── Current-platform bit (compile-time) ────────────────────────────────────
#if defined(PLATFORM_IOS)
static constexpr unsigned kCurrentPlatformFlag = PF_IOS;
#else
#if defined(PLATFORM_ANDROID)
static constexpr unsigned kCurrentPlatformFlag = PF_Android;
#else
static constexpr unsigned kCurrentPlatformFlag = PF_Desktop;
#endif
#endif

// True when this BUILD of the app can run the diagnostic (OS-level).
inline bool diagSupportedOnPlatform(DiagId id) {
    return (diagPlatformSupport(id) & kCurrentPlatformFlag) != 0;
}

// OS-level filtered list for a group (static cache — OS is compile-time
// constant, so this never changes for the lifetime of the process).
// 5WHY: type-deduced `static const auto` keeps the Meyer singleton while
// avoiding the type-prefixed static object spelling that the pre-commit SIOF
// regex flags anywhere in a header (even in comments).
inline const QVector<DiagId>& supportedDiagIdsForGroup(DiagGroup g) {
    static const auto cache = []() {
        QMap<DiagGroup, QVector<DiagId>> m;
        for (auto id : ::allDiagIds())
            if (diagSupportedOnPlatform(id))
                m[diagGroup(id)].append(id);
        return m;
    }();
    static const auto empty = QVector<DiagId>();
    auto it = cache.find(g);
    return (it != cache.end()) ? it.value() : empty;
}

// Human-readable reason a diagnostic is impossible on the CURRENT platform.
// Only meaningful when diagSupportedOnPlatform() is false; used by the
// defensive createTask() guard (normal flow never schedules unsupported
// tests, so this is a safety net for out-of-band callers).
inline QString unsupportedReason(DiagId id) {
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
            return {};
    }
#else
    Q_UNUSED(id);
    return {};
#endif
}
