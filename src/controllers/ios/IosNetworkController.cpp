// =============================================================================
// IosNetworkController.cpp — Stub: delegates to G1G2G3Native for now
//   TODO(Phase 4): Replace with Network.framework / NWPathMonitor implementations
// =============================================================================
#include "controllers/ios/IosNetworkController.h"
#include "engine/diagnostic/G1G2G3Native.h"

IosNetworkController::IosNetworkController() {}

DiagnosticResult IosNetworkController::networkAdapters(DiagId id)    { return G1G2G3Native::networkAdapters(id); }
DiagnosticResult IosNetworkController::nicAdvanced(DiagId id)         { return G1G2G3Native::nicAdvanced(id); }
DiagnosticResult IosNetworkController::wifiDiagnostics(DiagId id)    { return G1G2G3Native::wifiDiagnostics(id); }
DiagnosticResult IosNetworkController::wiredDiagnostics(DiagId id)   { return G1G2G3Native::wiredDiagnostics(id); }
DiagnosticResult IosNetworkController::dhcpStatus(DiagId id)         { return G1G2G3Native::dhcpStatus(id); }
DiagnosticResult IosNetworkController::ipConfiguration(DiagId id)    { return G1G2G3Native::ipConfiguration(id); }
DiagnosticResult IosNetworkController::activeConnections(DiagId id)  { return G1G2G3Native::activeConnections(id); }
DiagnosticResult IosNetworkController::cellularInfo(DiagId id)       { return G1G2G3Native::cellularInfo(id); }
DiagnosticResult IosNetworkController::networkProfile(DiagId id)     { return G1G2G3Native::networkProfile(id); }
DiagnosticResult IosNetworkController::tcpSettings(DiagId id)        { return G1G2G3Native::tcpSettings(id); }
DiagnosticResult IosNetworkController::defaultGateway(DiagId id)     { return G1G2G3Native::defaultGateway(id); }
DiagnosticResult IosNetworkController::routingTable(DiagId id)       { return G1G2G3Native::routingTable(id); }
DiagnosticResult IosNetworkController::arpTable(DiagId id)           { return G1G2G3Native::arpTable(id); }
DiagnosticResult IosNetworkController::proxySettings(DiagId id)      { return G1G2G3Native::proxySettings(id); }
DiagnosticResult IosNetworkController::netskopeStatus(DiagId id)     { return G1G2G3Native::netskopeStatus(id); }
DiagnosticResult IosNetworkController::dnsServers(DiagId id)         { return G1G2G3Native::dnsServers(id); }
DiagnosticResult IosNetworkController::dnsCache(DiagId id)           { return G1G2G3Native::dnsCache(id); }
DiagnosticResult IosNetworkController::dnsPollution(DiagId id)       { return G1G2G3Native::dnsPollution(id); }
DiagnosticResult IosNetworkController::speedTest(DiagId id)          { return G1G2G3Native::speedTest(id); }
