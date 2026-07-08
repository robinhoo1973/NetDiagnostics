// =============================================================================
// DiagId.h — Diagnostic test identifiers and enums
//
// Central registry of all diagnostic tests, groups, statuses, and metadata.
// =============================================================================
#pragma once

#include <QString>
#include <QVector>
#include <QMap>

// ── Test Group ──────────────────────────────────────────────────────────────
enum class DiagGroup {
    G1, G2, G3, G4, G5
};

inline QString diagGroupLabel(DiagGroup g) {
    switch (g) {
        case DiagGroup::G1: return QStringLiteral("System & Adapters");
        case DiagGroup::G2: return QStringLiteral("Connectivity & Security");
        case DiagGroup::G3: return QStringLiteral("Internet & DNS");
        case DiagGroup::G4: return QStringLiteral("Remote Host");
        case DiagGroup::G5: return QStringLiteral("Protocol");
    }
    return {};
}

// ── Test Status ─────────────────────────────────────────────────────────────
enum class DiagStatus {
    Pass, Warning, Fail, Skipped, Error, Info
};

inline QString diagStatusIcon(DiagStatus s) {
    switch (s) {
        case DiagStatus::Pass:    return QStringLiteral("badge-check");
        case DiagStatus::Warning: return QStringLiteral("badge-warning");
        case DiagStatus::Fail:    return QStringLiteral("badge-close");
        case DiagStatus::Skipped: return QStringLiteral("badge-skip");
        case DiagStatus::Error:   return QStringLiteral("badge-error");
        case DiagStatus::Info:    return QStringLiteral("badge-info");
    }
    return {};
}

// ── Test ID (38 values) ─────────────────────────────────────────────────────
enum class DiagId {
    // G1 — System & Adapters (8)
    G1NetworkAdapters,
    G1NicAdvanced,
    G1WifiDiagnostics,
    G1WiredDiagnostics,
    G1DhcpStatus,
    G1IpConfiguration,
    G1ActiveConnections,
    G1CellularInfo,

    // G2 — Connectivity & Security (6)
    G2NetworkProfile,
    G2TcpSettings,
    G2DefaultGateway,
    G2RoutingTable,
    G2ArpTable,
    G2ProxySettings,

    // G3 — Internet & DNS (5)
    G3NetskopeStatus,
    G3DnsServers,
    G3DnsCache,
    G3DnsPollution,
    G3InternetSpeedTest,

    // G4 — Remote Host (6)
    G4DnsResolution,
    G4Ping,
    G4Traceroute,
    G4PathPing,
    G4MtuDiscovery,
    G4PortScan,

    // G5 — Website / URL (13)
    G5UrlParsing,
    G5TcpConnect,
    G5ServiceBanner,
    G5CurlVerbose,
    G5HttpHeaders,
    G5SecurityHeaders,
    G5SslCertificate,
    G5HttpRedirect,
    G5HttpCompression,
    G5HttpTiming,
    G5FtpDiagnostics,
    G5SshDiagnostics,
    G5EmailDiagnostics,

    // G5 — per-scheme protocol diagnostics (7 new)
    G5Telnet,
    G5Mysql,
    G5Postgres,
    G5Redis,
    G5Mongodb,
    G5Ldap,
    G5Mqtt,
};

// ── DiagId metadata ─────────────────────────────────────────────────────────
inline DiagGroup diagGroup(DiagId id) {
    switch (id) {
        case DiagId::G1NetworkAdapters:
        case DiagId::G1NicAdvanced:
        case DiagId::G1WifiDiagnostics:
        case DiagId::G1WiredDiagnostics:
        case DiagId::G1DhcpStatus:
        case DiagId::G1IpConfiguration:
        case DiagId::G1ActiveConnections:
        case DiagId::G1CellularInfo:
            return DiagGroup::G1;
        case DiagId::G2NetworkProfile:
        case DiagId::G2TcpSettings:
        case DiagId::G2DefaultGateway:
        case DiagId::G2RoutingTable:
        case DiagId::G2ArpTable:
        case DiagId::G2ProxySettings:
            return DiagGroup::G2;
        case DiagId::G3NetskopeStatus:
        case DiagId::G3DnsServers:
        case DiagId::G3DnsCache:
        case DiagId::G3DnsPollution:
        case DiagId::G3InternetSpeedTest:
            return DiagGroup::G3;
        case DiagId::G4DnsResolution:
        case DiagId::G4Ping:
        case DiagId::G4Traceroute:
        case DiagId::G4PathPing:
        case DiagId::G4MtuDiscovery:
        case DiagId::G4PortScan:
            return DiagGroup::G4;
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
            return DiagGroup::G5;
        // G5 — Per-scheme protocol diagnostics (FTP, SSH, DB, etc.)
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
            return DiagGroup::G5;
    }
    return DiagGroup::G1;
}

inline QString diagIdLabelKey(DiagId id) {
    switch (id) {
        case DiagId::G1NetworkAdapters:     return QStringLiteral("test_g1_adapters");
        case DiagId::G1NicAdvanced:         return QStringLiteral("test_g1_nic_advanced");
        case DiagId::G1WifiDiagnostics:     return QStringLiteral("test_g1_wifi");
        case DiagId::G1WiredDiagnostics:    return QStringLiteral("test_g1_wired");
        case DiagId::G1DhcpStatus:          return QStringLiteral("test_g1_dhcp");
        case DiagId::G1IpConfiguration:     return QStringLiteral("test_g1_ipconfig");
        case DiagId::G1ActiveConnections:   return QStringLiteral("test_g1_connections");
        case DiagId::G1CellularInfo:        return QStringLiteral("test_g1_cellular");
        case DiagId::G2NetworkProfile:      return QStringLiteral("test_g2_profile");
        case DiagId::G2TcpSettings:         return QStringLiteral("test_g2_tcp");
        case DiagId::G2DefaultGateway:      return QStringLiteral("test_g2_gateway");
        case DiagId::G2RoutingTable:        return QStringLiteral("test_g2_routing");
        case DiagId::G2ArpTable:            return QStringLiteral("test_g2_arp");
        case DiagId::G2ProxySettings:       return QStringLiteral("test_g2_proxy");
        case DiagId::G3NetskopeStatus:      return QStringLiteral("test_g3_netskope");
        case DiagId::G3DnsServers:          return QStringLiteral("test_g3_dns_servers");
        case DiagId::G3DnsCache:            return QStringLiteral("test_g3_dns_cache");
        case DiagId::G3DnsPollution:        return QStringLiteral("test_g3_dns_pollution");
        case DiagId::G3InternetSpeedTest:   return QStringLiteral("test_g3_speed");
        case DiagId::G4DnsResolution:       return QStringLiteral("test_g4_dns");
        case DiagId::G4Ping:                return QStringLiteral("test_g4_ping");
        case DiagId::G4Traceroute:          return QStringLiteral("test_g4_traceroute");
        case DiagId::G4PathPing:            return QStringLiteral("test_g4_pathping");
        case DiagId::G4MtuDiscovery:        return QStringLiteral("test_g4_mtu");
        case DiagId::G4PortScan:            return QStringLiteral("test_g4_portscan");
        case DiagId::G5UrlParsing:          return QStringLiteral("test_g5_url_parse");
        case DiagId::G5TcpConnect:          return QStringLiteral("test_g5_tcp_connect");
        case DiagId::G5ServiceBanner:       return QStringLiteral("test_g5_banner");
        case DiagId::G5CurlVerbose:         return QStringLiteral("test_g5_curl");
        case DiagId::G5HttpHeaders:         return QStringLiteral("test_g5_headers");
        case DiagId::G5SecurityHeaders:     return QStringLiteral("test_g5_security");
        case DiagId::G5SslCertificate:      return QStringLiteral("test_g5_ssl");
        case DiagId::G5HttpRedirect:        return QStringLiteral("test_g5_redirect");
        case DiagId::G5HttpCompression:     return QStringLiteral("test_g5_compression");
        case DiagId::G5HttpTiming:          return QStringLiteral("test_g5_timing");
        case DiagId::G5FtpDiagnostics:      return QStringLiteral("test_g5_ftp");
        case DiagId::G5SshDiagnostics:      return QStringLiteral("test_g5_ssh");
        case DiagId::G5EmailDiagnostics:    return QStringLiteral("test_g5_email");
        case DiagId::G5Telnet:             return QStringLiteral("test_g5_telnet");
        case DiagId::G5Mysql:              return QStringLiteral("test_g5_mysql");
        case DiagId::G5Postgres:           return QStringLiteral("test_g5_postgres");
        case DiagId::G5Redis:              return QStringLiteral("test_g5_redis");
        case DiagId::G5Mongodb:            return QStringLiteral("test_g5_mongodb");
        case DiagId::G5Ldap:               return QStringLiteral("test_g5_ldap");
        case DiagId::G5Mqtt:               return QStringLiteral("test_g5_mqtt");
    }
    return {};
}

// ── Utility: all test IDs ───────────────────────────────────────────────────
inline const QVector<DiagId>& allDiagIds() {
    static const QVector<DiagId> ids = {
        DiagId::G1NetworkAdapters, DiagId::G1NicAdvanced, DiagId::G1WifiDiagnostics,
        DiagId::G1WiredDiagnostics, DiagId::G1DhcpStatus, DiagId::G1IpConfiguration,
        DiagId::G1ActiveConnections, DiagId::G1CellularInfo,
        DiagId::G2NetworkProfile, DiagId::G2TcpSettings, DiagId::G2DefaultGateway,
        DiagId::G2RoutingTable, DiagId::G2ArpTable, DiagId::G2ProxySettings,
        DiagId::G3NetskopeStatus, DiagId::G3DnsServers, DiagId::G3DnsCache,
        DiagId::G3DnsPollution, DiagId::G3InternetSpeedTest, // G3InternetConnectivity merged into Speed Test
        DiagId::G4DnsResolution, DiagId::G4Ping, DiagId::G4Traceroute,
        DiagId::G4PathPing, DiagId::G4MtuDiscovery, DiagId::G4PortScan,
        DiagId::G5UrlParsing, DiagId::G5TcpConnect, DiagId::G5ServiceBanner,
        DiagId::G5CurlVerbose, DiagId::G5HttpHeaders, DiagId::G5SecurityHeaders,
        DiagId::G5SslCertificate, DiagId::G5HttpRedirect, DiagId::G5HttpCompression,
        DiagId::G5HttpTiming, DiagId::G5FtpDiagnostics, DiagId::G5SshDiagnostics,
        DiagId::G5EmailDiagnostics,
        DiagId::G5Telnet, DiagId::G5Mysql, DiagId::G5Postgres,
        DiagId::G5Redis, DiagId::G5Mongodb, DiagId::G5Ldap, DiagId::G5Mqtt,
    };
    return ids;
}

inline const QVector<DiagId>& diagIdsForGroup(DiagGroup g) {
    static const QMap<DiagGroup, QVector<DiagId>> cache = []() {
        QMap<DiagGroup, QVector<DiagId>> m;
        for (auto id : allDiagIds())
            m[diagGroup(id)].append(id);
        return m;
    }();
    static const QVector<DiagId> empty;
    auto it = cache.find(g);
    return (it != cache.end()) ? *it : empty;
}