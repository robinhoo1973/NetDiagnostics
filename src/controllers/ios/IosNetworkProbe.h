// =============================================================================
// IosNetworkProbe.h — iOS port scan via NWConnection (stub)
//   TODO(Phase 4): Replace with NWConnection concurrent connects
// =============================================================================
#pragma once
#include "controllers/INetworkProbe.h"

class IosNetworkProbe : public INetworkProbe {
public:
    IosNetworkProbe() = default;

    TcpConnectResult tcpConnect(const QString& host, int port, int timeoutMs = 5000) override;
    QVector<PortScanEntry> portScan(const QString& host,
                                    const QVector<int>& ports,
                                    int timeoutMs = 2000,
                                    int maxConcurrent = 64) override;
    QVector<int> commonDiagnosticPorts() const override;
    QMap<int, QString> wellKnownPorts() const override;

    bool isAvailable() const override { return true; }
};
