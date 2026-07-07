
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
