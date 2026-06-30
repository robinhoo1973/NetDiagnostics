// =============================================================================
// DesktopNetworkController.cpp — delegates to existing G1G2G3Native
// =============================================================================
#include "controllers/desktop/DesktopNetworkController.h"
#include "engine/diagnostic/G1G2G3Native.h"

DiagnosticResult DesktopNetworkController::networkAdapters(DiagId id)       { return G1G2G3Native::networkAdapters(id); }
DiagnosticResult DesktopNetworkController::nicAdvanced(DiagId id)            { return G1G2G3Native::nicAdvanced(id); }
DiagnosticResult DesktopNetworkController::wifiDiagnostics(DiagId id)       { return G1G2G3Native::wifiDiagnostics(id); }
DiagnosticResult DesktopNetworkController::wiredDiagnostics(DiagId id)      { return G1G2G3Native::wiredDiagnostics(id); }
DiagnosticResult DesktopNetworkController::dhcpStatus(DiagId id)            { return G1G2G3Native::dhcpStatus(id); }
DiagnosticResult DesktopNetworkController::ipConfiguration(DiagId id)       { return G1G2G3Native::ipConfiguration(id); }
DiagnosticResult DesktopNetworkController::activeConnections(DiagId id)     { return G1G2G3Native::activeConnections(id); }
DiagnosticResult DesktopNetworkController::cellularInfo(DiagId id)          { return G1G2G3Native::cellularInfo(id); }
DiagnosticResult DesktopNetworkController::networkProfile(DiagId id)        { return G1G2G3Native::networkProfile(id); }
DiagnosticResult DesktopNetworkController::tcpSettings(DiagId id)           { return G1G2G3Native::tcpSettings(id); }
DiagnosticResult DesktopNetworkController::defaultGateway(DiagId id)        { return G1G2G3Native::defaultGateway(id); }
DiagnosticResult DesktopNetworkController::routingTable(DiagId id)          { return G1G2G3Native::routingTable(id); }
DiagnosticResult DesktopNetworkController::arpTable(DiagId id)              { return G1G2G3Native::arpTable(id); }
DiagnosticResult DesktopNetworkController::proxySettings(DiagId id)         { return G1G2G3Native::proxySettings(id); }
DiagnosticResult DesktopNetworkController::netskopeStatus(DiagId id)        { return G1G2G3Native::netskopeStatus(id); }
DiagnosticResult DesktopNetworkController::dnsServers(DiagId id)            { return G1G2G3Native::dnsServers(id); }
DiagnosticResult DesktopNetworkController::dnsCache(DiagId id)              { return G1G2G3Native::dnsCache(id); }
DiagnosticResult DesktopNetworkController::dnsPollution(DiagId id)          { return G1G2G3Native::dnsPollution(id); }
DiagnosticResult DesktopNetworkController::speedTest(DiagId id)             { return G1G2G3Native::speedTest(id); }
