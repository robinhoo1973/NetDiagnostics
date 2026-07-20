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
        case DiagId::G3DnsPollution:     return QStringLiteral("DNS Pollution");
        case DiagId::G3GeoIPLoc:         return QStringLiteral("IP Geo Location");
        case DiagId::G3InternetConnectivity: return QStringLiteral("Internet Connectivity");
        case DiagId::G4DnsResolution:    return QStringLiteral("DNS Resolution");
        case DiagId::G4Ping:             return QStringLiteral("Ping");
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
        case DiagId::G5FtpDiagnostics:   return QStringLiteral("FTP");
        case DiagId::G5SshDiagnostics:   return QStringLiteral("SSH");
        case DiagId::G5EmailDiagnostics: return QStringLiteral("Email");
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
