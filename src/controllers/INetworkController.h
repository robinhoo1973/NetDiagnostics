// =============================================================================
// INetworkController.h — Interface for G1/G2/G3 system diagnostics
// =============================================================================
#pragma once
#include "models/DiagnosticResult.h"
#include "models/DiagId.h"

class INetworkController {
public:
    virtual ~INetworkController() = default;

    // G1 — System & Adapters
    virtual DiagnosticResult networkAdapters(DiagId id) = 0;
    virtual DiagnosticResult nicAdvanced(DiagId id) = 0;
    virtual DiagnosticResult wifiDiagnostics(DiagId id) = 0;
    virtual DiagnosticResult wiredDiagnostics(DiagId id) = 0;
    virtual DiagnosticResult dhcpStatus(DiagId id) = 0;
    virtual DiagnosticResult ipConfiguration(DiagId id) = 0;
    virtual DiagnosticResult activeConnections(DiagId id) = 0;
    virtual DiagnosticResult cellularInfo(DiagId id) = 0;

    // G2 — Connectivity & Security
    virtual DiagnosticResult networkProfile(DiagId id) = 0;
    virtual DiagnosticResult tcpSettings(DiagId id) = 0;
    virtual DiagnosticResult defaultGateway(DiagId id) = 0;
    virtual DiagnosticResult routingTable(DiagId id) = 0;
    virtual DiagnosticResult arpTable(DiagId id) = 0;
    virtual DiagnosticResult proxySettings(DiagId id) = 0;

    // G3 — Internet & DNS
    virtual DiagnosticResult netskopeStatus(DiagId id) = 0;
    virtual DiagnosticResult dnsServers(DiagId id) = 0;
    virtual DiagnosticResult dnsCache(DiagId id) = 0;
    virtual DiagnosticResult dnsPollution(DiagId id) = 0;
    virtual DiagnosticResult speedTest(DiagId id) = 0;

    virtual bool isAvailable() const = 0;
    virtual QString controllerName() const = 0;
};
