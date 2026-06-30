// =============================================================================
// IosNetworkController.h — iOS G1/G2/G3 via Apple frameworks (stub)
// =============================================================================
#pragma once
#include "controllers/INetworkController.h"

class IosNetworkController : public INetworkController {
public:
    IosNetworkController();

    DiagnosticResult networkAdapters(DiagId id) override;
    DiagnosticResult nicAdvanced(DiagId id) override;
    DiagnosticResult wifiDiagnostics(DiagId id) override;
    DiagnosticResult wiredDiagnostics(DiagId id) override;
    DiagnosticResult dhcpStatus(DiagId id) override;
    DiagnosticResult ipConfiguration(DiagId id) override;
    DiagnosticResult activeConnections(DiagId id) override;
    DiagnosticResult cellularInfo(DiagId id) override;
    DiagnosticResult networkProfile(DiagId id) override;
    DiagnosticResult tcpSettings(DiagId id) override;
    DiagnosticResult defaultGateway(DiagId id) override;
    DiagnosticResult routingTable(DiagId id) override;
    DiagnosticResult arpTable(DiagId id) override;
    DiagnosticResult proxySettings(DiagId id) override;
    DiagnosticResult netskopeStatus(DiagId id) override;
    DiagnosticResult dnsServers(DiagId id) override;
    DiagnosticResult dnsCache(DiagId id) override;
    DiagnosticResult dnsPollution(DiagId id) override;
    DiagnosticResult speedTest(DiagId id) override;

    bool isAvailable() const override { return true; }
    QString controllerName() const override { return QStringLiteral("IosNetwork"); }
};
