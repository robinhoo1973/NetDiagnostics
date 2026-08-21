// =============================================================================
// NetworkDiagnostics.h — Android native diagnostic function declarations
//
// Implemented via JNI (QJniObject) in NetworkDiagnostics.cpp.  On Android
// every G1-G5 test that can produce real data has a dedicated Android
// implementation — platform-unsupported ones are reported honestly instead
// of returning empty/hardcoded output.
// =============================================================================
#pragma once

#include "Common/Model/DiagnosticResult.h"
#include "Common/Model/DiagId.h"

// ── Existing entry points (G4 DNS / G5 HTTP) ───────────────────────────
DiagnosticResult androidDnsDiag(DiagId id, const QString& target);
DiagnosticResult androidHttpDiag(DiagId id, const QString& target);

// ── G1: System & Adapters ──────────────────────────────────────────────
DiagnosticResult androidNetworkAdaptersDiag(DiagId id);     // G1NetworkAdapters
DiagnosticResult androidWifiDiag(DiagId id);                // G1WifiDiagnostics
DiagnosticResult androidNicAdvancedDiag(DiagId id);         // G1NicAdvanced
DiagnosticResult androidWiredDiagnosticsDiag(DiagId id);    // G1WiredDiagnostics
DiagnosticResult androidDhcpDiag(DiagId id);                // G1DhcpStatus
DiagnosticResult androidIpConfigurationDiag(DiagId id);     // G1IpConfiguration
DiagnosticResult androidActiveConnectionsDiag(DiagId id);   // G1ActiveConnections
DiagnosticResult androidCellularDiag(DiagId id);            // G1CellularInfo

// ── G2: Connectivity & Security ────────────────────────────────────────
DiagnosticResult androidNetworkProfileDiag(DiagId id);      // G2NetworkProfile
DiagnosticResult androidGatewayDiag(DiagId id);             // G2DefaultGateway
DiagnosticResult androidRoutingTableDiag(DiagId id);        // G2RoutingTable
DiagnosticResult androidArpTableDiag(DiagId id);            // G2ArpTable
DiagnosticResult androidProxyDiag(DiagId id);               // G2ProxySettings

// ── G3: Internet & DNS ─────────────────────────────────────────────────
DiagnosticResult androidDnsServersDiag(DiagId id);          // G3DnsServers
DiagnosticResult androidDnsCacheDiag(DiagId id);            // G3DnsCache
