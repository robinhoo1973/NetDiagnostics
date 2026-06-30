// =============================================================================
// DesktopNetworkController.cpp — delegates to existing G1G2G3Native
// =============================================================================
#include "controllers/desktop/DesktopNetworkController.h"
#include "engine/diagnostic/G1G2G3Native.h"

#define DELEGATE(method) \
    DiagnosticResult DesktopNetworkController::method(DiagId id) { return G1G2G3Native::method(id); }

DELEGATE(networkAdapters)
DELEGATE(nicAdvanced)
DELEGATE(wifiDiagnostics)
DELEGATE(wiredDiagnostics)
DELEGATE(dhcpStatus)
DELEGATE(ipConfiguration)
DELEGATE(activeConnections)
DELEGATE(cellularInfo)
DELEGATE(networkProfile)
DELEGATE(tcpSettings)
DELEGATE(defaultGateway)
DELEGATE(routingTable)
DELEGATE(arpTable)
DELEGATE(proxySettings)
DELEGATE(netskopeStatus)
DELEGATE(dnsServers)
DELEGATE(dnsCache)
DELEGATE(dnsPollution)
DELEGATE(speedTest)

#undef DELEGATE
