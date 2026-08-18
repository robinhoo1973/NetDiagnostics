// =============================================================================
// DiagNames.h — Display names, icons, template/animation enums (contract layer)
// =============================================================================
#pragma once

#include "Common/Model/DiagId.h"
#include <QString>

// ── L5 template + L4 animation categories ────────────────────────────────
enum class DiagTemplateType { System, Ping, Path, Handshake, Request, Query };
// L4 动画（AppState.diagAnimationUrl → qrc:/qt/qml/widgets/animations/）：
//   Pulse 呼吸 / Jiggle 抖动 / Bounce 往返 / Type 键入 / Path 逐跳
//   Lock 盖章落下 / Check 盾牌打勾 / Meter 表针摆动 / Converge 箭头聚拢
enum class DiagAnimType     { Pulse, Jiggle, Bounce, Type, Path, Lock, Check, Meter, Converge };

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
        case DiagId::_G3Reserved17_Deprecated: return QStringLiteral("(removed)");
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
