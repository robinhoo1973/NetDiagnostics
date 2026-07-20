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

// ── Test ID (45 values) ─────────────────────────────────────────────────────
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

    // G3 — Internet & DNS (6)
    G3NetskopeStatus,
    G3DnsServers,
    G3DnsCache,
    G3DnsPollution,
    G3GeoIPLoc,
    G3InternetConnectivity,

    // G4 — Remote Host (5)
    G4DnsResolution,
    G4Ping,
    G4Traceroute,
    G4PathPing,
    G4MtuDiscovery,

    // G5 — Protocol (20)
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
        case DiagId::G3GeoIPLoc:
        case DiagId::G3InternetConnectivity:
            return DiagGroup::G3;
        case DiagId::G4DnsResolution:
        case DiagId::G4Ping:
        case DiagId::G4Traceroute:
        case DiagId::G4PathPing:
        case DiagId::G4MtuDiscovery:
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

// ── Utility: all test IDs ───────────────────────────────────────────────────
inline const QVector<DiagId>& allDiagIds() {
    static const QVector<DiagId> ids = {
        DiagId::G1NetworkAdapters, DiagId::G1NicAdvanced, DiagId::G1WifiDiagnostics,
        DiagId::G1WiredDiagnostics, DiagId::G1DhcpStatus, DiagId::G1IpConfiguration,
        DiagId::G1ActiveConnections, DiagId::G1CellularInfo,
        DiagId::G2NetworkProfile, DiagId::G2TcpSettings, DiagId::G2DefaultGateway,
        DiagId::G2RoutingTable, DiagId::G2ArpTable, DiagId::G2ProxySettings,
        DiagId::G3NetskopeStatus, DiagId::G3DnsServers, DiagId::G3DnsCache,
        DiagId::G3DnsPollution,
        DiagId::G3InternetConnectivity, DiagId::G3GeoIPLoc,
        DiagId::G4DnsResolution, DiagId::G4Ping, DiagId::G4Traceroute,
        DiagId::G4PathPing, DiagId::G4MtuDiscovery,
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
    return (it != cache.end()) ? it.value() : empty;
}