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
        case DiagId::G1NicAdvanced:        return QStringLiteral("chip");
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
