
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
