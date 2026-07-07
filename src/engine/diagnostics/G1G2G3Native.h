// =============================================================================
// G1G2G3Native.h — Pure C++ implementations for G1/G2/G3 diagnostics
// Zero shell commands. Uses native OS APIs (getifaddrs, /proc, netlink on Linux;
// GetAdaptersAddresses, GetExtendedTcpTable etc. on Windows).
// Output formatted to match Windows CLI tools (ipconfig, route print, arp -a,
// netstat, netsh, etc.).
// =============================================================================
#pragma once

#include "models/DiagnosticResult.h"
#include "models/DiagId.h"
#include <QString>
#include <QStringList>
#include <QMap>
#include <QVariantMap>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QRegularExpression>
#include <QProcess>
#include <cstring>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <wlanapi.h>
#include <winhttp.h>
#include <tlhelp32.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

namespace G1G2G3Native {

// ── G1: System & Adapters ──────────────────────────────────────────────

/// Network adapters list — output matches "ipconfig /all" adapter enumeration
DiagnosticResult networkAdapters(DiagId id);

/// NIC advanced properties — speed, duplex, MTU, driver info
/// Output matches "wmic nic get Name,Speed,MACAddress,..." format
DiagnosticResult nicAdvanced(DiagId id);

/// WiFi diagnostics — SSID, signal, channel, security
/// Output matches "netsh wlan show interfaces" format
DiagnosticResult wifiDiagnostics(DiagId id);

/// Wired Ethernet diagnostics — link status, speed, duplex
DiagnosticResult wiredDiagnostics(DiagId id);

/// DHCP status — lease info, server address
/// Output matches "ipconfig /all" DHCP section format
DiagnosticResult dhcpStatus(DiagId id);

/// Full IP configuration — addresses, netmasks, gateways per adapter
/// Output matches "ipconfig /all" full format
DiagnosticResult ipConfiguration(DiagId id);

/// Active TCP/UDP connections
/// Output matches "netstat -an" format
DiagnosticResult activeConnections(DiagId id);

/// Cellular network info — carrier, radio access (5G/LTE/3G), MCC-MNC
DiagnosticResult cellularInfo(DiagId id);

// ── G2: Connectivity & Security ────────────────────────────────────────

/// Network profile / firewall profile
/// Output matches "netsh advfirewall show currentprofile" / "nmcli" format
DiagnosticResult networkProfile(DiagId id);

/// TCP stack settings — congestion control, window scaling, timestamps
/// Output matches "netsh int tcp show global" format
DiagnosticResult tcpSettings(DiagId id);

/// Default gateway info
DiagnosticResult defaultGateway(DiagId id);

/// Full routing table
/// Output matches "route print" format
DiagnosticResult routingTable(DiagId id);

/// ARP / neighbour table
/// Output matches "arp -a" format
DiagnosticResult arpTable(DiagId id);

/// Proxy settings — HTTP/HTTPS/SOCKS, PAC URL
DiagnosticResult proxySettings(DiagId id);

// ── G3: Internet & DNS ─────────────────────────────────────────────────

/// Netskope / ZScaler client detection (check for proxy process)
DiagnosticResult netskopeStatus(DiagId id);

/// DNS server configuration per adapter
/// Output matches "ipconfig /all" DNS section format
DiagnosticResult dnsServers(DiagId id);

/// DNS cache statistics — entries, hits, TTL
DiagnosticResult dnsCache(DiagId id);

/// DNS pollution / hijacking check — compare resolution across DNS servers
DiagnosticResult dnsPollution(DiagId id);

/// Combined Internet Connectivity & Speed Test
/// Phase 0: TCP connectivity check to well-known hosts
/// Phase 1-4: Speedtest.net protocol download/upload bandwidth test
/// Output matches "speedtest-cli" format with connectivity verdict in summary
DiagnosticResult speedTest(DiagId id);

} // namespace G1G2G3Native