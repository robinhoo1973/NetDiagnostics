// =============================================================================
// DiagNames.h — Shared diagnostic display-name lookup (single source of truth)
//
// Used by AppState (report generation), ResultsModel (QML formatting), and
// any other code that needs human-readable diagnostic names.
// =============================================================================
#pragma once

#include "Common/Model/DiagId.h"
#include <QString>

inline QString diagDisplayName(DiagId id) {
    switch (id) {
        case DiagId::G1NetworkAdapters:  return QStringLiteral("Network Adapters");
        case DiagId::G1NicAdvanced:      return QStringLiteral("NIC Advanced");
        case DiagId::G1WifiDiagnostics:  return QStringLiteral("WiFi Information");
        case DiagId::G1WiredDiagnostics: return QStringLiteral("Wired Information");
        case DiagId::G1DhcpStatus:       return QStringLiteral("DHCP Status");
        case DiagId::G1IpConfiguration:  return QStringLiteral("IP Configuration");
        case DiagId::G1ActiveConnections: return QStringLiteral("Active Connections");
        case DiagId::G1CellularInfo:     return QStringLiteral("Cellular Information");
        case DiagId::G2NetworkProfile:   return QStringLiteral("Network Profile");
        case DiagId::G2TcpSettings:      return QStringLiteral("TCP Settings");
        case DiagId::G2DefaultGateway:   return QStringLiteral("Default Gateway");
        case DiagId::G2RoutingTable:     return QStringLiteral("Routing Table");
        case DiagId::G2ArpTable:         return QStringLiteral("ARP Table");
        case DiagId::G2ProxySettings:    return QStringLiteral("Proxy Settings");
        case DiagId::G3NetskopeStatus:   return QStringLiteral("Netskope Status");
        case DiagId::G3DnsServers:       return QStringLiteral("DNS Servers");
        case DiagId::G3DnsCache:         return QStringLiteral("DNS Cache");
        case DiagId::G3DnsIntegrity:     return QStringLiteral("DNS Integrity");
        case DiagId::G3GeoIPLoc:         return QStringLiteral("IP Geolocation");
        case DiagId::G3InternetConnectivity: return QStringLiteral("Internet Connectivity & Speed");
        case DiagId::G4DnsResolution:    return QStringLiteral("DNS Resolution");
        case DiagId::G4Ping:             return QStringLiteral("Ping");
        case DiagId::G4IPv6Connectivity: return QStringLiteral("IPv6 Connectivity");
        case DiagId::G4Traceroute:       return QStringLiteral("Traceroute");
        case DiagId::G4PathPing:         return QStringLiteral("PathPing");
        case DiagId::G4MtuDiscovery:     return QStringLiteral("MTU Discovery");
        case DiagId::G5UrlParsing:       return QStringLiteral("URL Parsing");
        case DiagId::G5TcpConnect:       return QStringLiteral("TCP Connect");
        case DiagId::G5ServiceBanner:    return QStringLiteral("Service Banner");
        case DiagId::G5CurlVerbose:      return QStringLiteral("HTTP Request");
        case DiagId::G5HttpHeaders:      return QStringLiteral("HTTP Headers");
        case DiagId::G5SecurityHeaders:  return QStringLiteral("Security Headers");
        case DiagId::G5SslCertificate:   return QStringLiteral("SSL Certificate");
        case DiagId::G5HttpRedirect:     return QStringLiteral("HTTP Redirect");
        case DiagId::G5HttpCompression:  return QStringLiteral("HTTP Compression");
        case DiagId::G5HttpTiming:       return QStringLiteral("HTTP Timing");
        case DiagId::G5FtpDiagnostics:   return QStringLiteral("FTP Diagnostics");
        case DiagId::G5SshDiagnostics:   return QStringLiteral("SSH Diagnostics");
        case DiagId::G5EmailDiagnostics: return QStringLiteral("Email Diagnostics");
        case DiagId::G5Telnet:           return QStringLiteral("Telnet");
        case DiagId::G5Mysql:            return QStringLiteral("MySQL");
        case DiagId::G5Postgres:         return QStringLiteral("PostgreSQL");
        case DiagId::G5Redis:            return QStringLiteral("Redis");
        case DiagId::G5Mongodb:          return QStringLiteral("MongoDB");
        case DiagId::G5Ldap:             return QStringLiteral("LDAP");
        case DiagId::G5Mqtt:             return QStringLiteral("MQTT");
    }
    return QStringLiteral("Unknown");
}

// ── Icon lookup (single source of truth for per-group / per-test icons) ──
// 5WHY (2026-08-09): G1-G5 group headers and every test row now show a
// semantic icon.  Names map to white-stroke master SVGs in
// resources/icons/ffffff/ (colorized per palette by
// scripts/generate-colored-icons.py, selected by AppIcon.qml).  Keeping the
// mapping here mirrors diagDisplayName() so C++ and QML share one source.
inline QString groupIconName(DiagGroup g) {
    switch (g) {
        case DiagGroup::G1: return QStringLiteral("network-card");
        case DiagGroup::G2: return QStringLiteral("shield-network");
        case DiagGroup::G3: return QStringLiteral("internet-globe");
        case DiagGroup::G4: return QStringLiteral("remote-host");
        case DiagGroup::G5: return QStringLiteral("protocol-stack");
    }
    return QStringLiteral("circle");
}

inline QString diagIconName(DiagId id) {
    switch (id) {
        // G1 — System & Adapters
        case DiagId::G1NetworkAdapters:    return QStringLiteral("network-card");
        // 5WHY (icon vetting 2026-08-10): chip.svg was superseded by the
        // existing project cpu.svg (identical Lucide design) — reuse it.
        case DiagId::G1NicAdvanced:        return QStringLiteral("cpu");
        case DiagId::G1WifiDiagnostics:    return QStringLiteral("wifi");
        case DiagId::G1WiredDiagnostics:   return QStringLiteral("ethernet");
        case DiagId::G1DhcpStatus:         return QStringLiteral("dhcp");
        case DiagId::G1IpConfiguration:    return QStringLiteral("ip-config");
        case DiagId::G1ActiveConnections:  return QStringLiteral("connections");
        case DiagId::G1CellularInfo:       return QStringLiteral("cellular");
        // G2 — Connectivity & Security
        case DiagId::G2NetworkProfile:     return QStringLiteral("network-profile");
        case DiagId::G2TcpSettings:        return QStringLiteral("tcp-settings");
        case DiagId::G2DefaultGateway:     return QStringLiteral("gateway");
        case DiagId::G2RoutingTable:       return QStringLiteral("route-table");
        case DiagId::G2ArpTable:           return QStringLiteral("arp-table");
        case DiagId::G2ProxySettings:      return QStringLiteral("proxy");
        // G3 — Internet & DNS
        case DiagId::G3NetskopeStatus:     return QStringLiteral("cloud-shield");
        case DiagId::G3DnsServers:         return QStringLiteral("dns-server");
        case DiagId::G3DnsCache:           return QStringLiteral("dns-cache");
        case DiagId::G3DnsIntegrity:       return QStringLiteral("dns-shield");
        case DiagId::G3GeoIPLoc:           return QStringLiteral("geo-location");
        case DiagId::G3InternetConnectivity: return QStringLiteral("internet-check");
        // G4 — Remote Host
        case DiagId::G4DnsResolution:      return QStringLiteral("dns-resolve");
        case DiagId::G4Ping:               return QStringLiteral("ping");
        case DiagId::G4Traceroute:         return QStringLiteral("traceroute");
        case DiagId::G4PathPing:           return QStringLiteral("path-ping");
        case DiagId::G4MtuDiscovery:       return QStringLiteral("mtu");
        case DiagId::G4IPv6Connectivity:   return QStringLiteral("ipv6");
        // G5 — Protocol
        case DiagId::G5UrlParsing:         return QStringLiteral("url-parse");
        case DiagId::G5TcpConnect:         return QStringLiteral("tcp-connect");
        case DiagId::G5ServiceBanner:      return QStringLiteral("banner");
        case DiagId::G5CurlVerbose:        return QStringLiteral("curl-verbose");
        case DiagId::G5HttpHeaders:        return QStringLiteral("http-headers");
        case DiagId::G5SecurityHeaders:    return QStringLiteral("security-headers");
        case DiagId::G5SslCertificate:     return QStringLiteral("certificate");
        case DiagId::G5HttpRedirect:       return QStringLiteral("redirect");
        case DiagId::G5HttpCompression:    return QStringLiteral("compression");
        case DiagId::G5HttpTiming:         return QStringLiteral("http-timing");
        case DiagId::G5FtpDiagnostics:     return QStringLiteral("ftp");
        case DiagId::G5SshDiagnostics:     return QStringLiteral("ssh");
        case DiagId::G5EmailDiagnostics:   return QStringLiteral("mail");
        case DiagId::G5Telnet:             return QStringLiteral("telnet");
        case DiagId::G5Mysql:              return QStringLiteral("mysql");
        case DiagId::G5Postgres:           return QStringLiteral("postgres");
        case DiagId::G5Redis:              return QStringLiteral("redis");
        case DiagId::G5Mongodb:            return QStringLiteral("mongodb");
        case DiagId::G5Ldap:               return QStringLiteral("ldap");
        case DiagId::G5Mqtt:               return QStringLiteral("mqtt");
    }
    return QStringLiteral("circle");
}

// ── Animation type classification (Living Diagnostics L4) ─────────────────
// Maps each DiagId to an animation category used by DiagAnimator.qml.
// Values match the Loader switch cases in DiagAnimator.qml.
enum class DiagAnimType {
    Jiggle = 0,   // rotation ±2.5° + scale pulse (~26 tests, default)
    Bounce = 1,   // dot bouncing between endpoints (6 tests)
    Path   = 2,   // nodes appear sequentially (6 tests)
    Pulse  = 3,   // opacity glow pulse (9 tests)
    Type   = 4,   // sequential row reveal (6 tests)
    Lock   = 5    // stamp drop-and-settle (4 tests)
};

inline DiagAnimType diagAnimationType(DiagId id) {
    switch (id) {
        // ── Bounce (6) ──────────────────────────────────────────────────
        case DiagId::G4Ping:               return DiagAnimType::Bounce;
        case DiagId::G1DhcpStatus:         return DiagAnimType::Bounce;
        case DiagId::G3GeoIPLoc:           return DiagAnimType::Bounce;
        case DiagId::G5Mqtt:               return DiagAnimType::Bounce;
        case DiagId::G4DnsResolution:      return DiagAnimType::Bounce;
        case DiagId::G4PathPing:           return DiagAnimType::Bounce;

        // ── Path (6) ────────────────────────────────────────────────────
        case DiagId::G4Traceroute:         return DiagAnimType::Path;
        case DiagId::G2RoutingTable:       return DiagAnimType::Path;
        case DiagId::G2ProxySettings:      return DiagAnimType::Path;
        case DiagId::G5TcpConnect:         return DiagAnimType::Path;
        case DiagId::G5HttpRedirect:       return DiagAnimType::Path;
        case DiagId::G1ActiveConnections:  return DiagAnimType::Path;

        // ── Pulse (9+) ──────────────────────────────────────────────────
        case DiagId::G1NetworkAdapters:    return DiagAnimType::Pulse;
        case DiagId::G1NicAdvanced:        return DiagAnimType::Pulse;
        case DiagId::G3DnsServers:         return DiagAnimType::Pulse;
        case DiagId::G5Mysql:              return DiagAnimType::Pulse;
        case DiagId::G5Postgres:           return DiagAnimType::Pulse;
        case DiagId::G5Redis:              return DiagAnimType::Pulse;
        case DiagId::G5Mongodb:            return DiagAnimType::Pulse;
        case DiagId::G5FtpDiagnostics:     return DiagAnimType::Pulse;
        case DiagId::G5Telnet:             return DiagAnimType::Pulse;

        // ── Type (6+) ───────────────────────────────────────────────────
        case DiagId::G2ArpTable:           return DiagAnimType::Type;
        case DiagId::G5UrlParsing:         return DiagAnimType::Type;
        case DiagId::G5HttpHeaders:        return DiagAnimType::Type;
        case DiagId::G5CurlVerbose:        return DiagAnimType::Type;
        case DiagId::G5SshDiagnostics:     return DiagAnimType::Type;
        case DiagId::G5Ldap:               return DiagAnimType::Type;

        // ── Lock (4) ────────────────────────────────────────────────────
        case DiagId::G5SslCertificate:     return DiagAnimType::Lock;
        case DiagId::G3DnsIntegrity:       return DiagAnimType::Lock;
        case DiagId::G5SecurityHeaders:    return DiagAnimType::Lock;
        case DiagId::G3InternetConnectivity: return DiagAnimType::Lock;

        // ── Jiggle (default, ~26 remaining) ─────────────────────────────
        default:                           return DiagAnimType::Jiggle;
    }
}

// ── Detail page template classification (Living Diagnostics L5) ──────────
// Eliminates the duck-typing in DetailPage._template — the diagnostic
// declares which template to use, not inferred from data key existence.
enum class DiagTemplateType {
    Ping       = 0,  // RTT/latency: bar chart + loss gauge
    Path       = 1,  // hop-by-hop: path graph + hop table
    Handshake  = 2,  // TLS/auth: certificate chain + timing diagram
    Request    = 3,  // HTTP waterfall: phase timing bars
    Query      = 4,  // DB/protocol service: connect info + banner
    System     = 5   // G1/G2 system properties: adapter/route tables
};

inline DiagTemplateType diagTemplateType(DiagId id) {
    switch (id) {
        // ── Ping/latency (G4 remote probes) ────────────────────────────
        // 5WHY: G4DnsResolution was Ping, but it produces a single DNS
        // query result (queryTimeMs + answerCount) — no individualRtts[].
        // The Ping chart requires individualRtts.length > 0; DnsResolution
        // never had a chart.  System template (properties table) better fits
        // its output shape.
        case DiagId::G4Ping:
            return DiagTemplateType::Ping;

        // ── Path (hop-by-hop trace) ────────────────────────────────────
        // 5WHY: G4PathPing was classified Ping, but it emits hop data
        // (hops[] with per-hop rttMs), not per-packet individualRtts — the
        // Ping template's chart looked for a key PathPing never writes, so
        // PathPing never got a chart.  Hop data ⇒ Path template.
        case DiagId::G4Traceroute:
        case DiagId::G4PathPing:
            return DiagTemplateType::Path;

        // ── Handshake/security ──────────────────────────────────────────
        case DiagId::G5SslCertificate:
        case DiagId::G3DnsIntegrity:
        case DiagId::G5SecurityHeaders:
        case DiagId::G3InternetConnectivity:
            return DiagTemplateType::Handshake;

        // ── Request/HTTP timing ─────────────────────────────────────────
        // 5WHY: G5UrlParsing was Request, but it parses URL components
        // (scheme/host/port) — no HTTP timing.  System template avoids
        // the broken waterfall chart.
        case DiagId::G5HttpTiming:
        case DiagId::G5HttpHeaders:
        case DiagId::G5HttpRedirect:
        case DiagId::G5HttpCompression:
        case DiagId::G5CurlVerbose:
            return DiagTemplateType::Request;

        // ── Query/DB service ────────────────────────────────────────────
        case DiagId::G5TcpConnect:
        case DiagId::G5ServiceBanner:
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
            return DiagTemplateType::Query;

        // ── System properties (G1-G2, default) ──────────────────────────
        // 5WHY: G2RoutingTable was classified Path, but it dumps routes[]
        // (dest/gateway/metric) — no hop RTTs.  Forcing the Path chart
        // schema (hops[]) on it produced nothing; it's a property table.
        case DiagId::G2RoutingTable:
            return DiagTemplateType::System;
        default:
            return DiagTemplateType::System;
    }
}
