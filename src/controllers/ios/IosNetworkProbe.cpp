// =============================================================================
// IosNetworkProbe.cpp — Stub: delegates to existing NetworkProbe
//   TODO(Phase 4): NWConnection implementation — removes raw socket dependency
// =============================================================================
#include "controllers/ios/IosNetworkProbe.h"
#include "engine/runner/NetworkProbe.h"

TcpConnectResult IosNetworkProbe::tcpConnect(const QString& host, int port, int timeoutMs) {
    NetworkProbe::TcpConnectResult r = NetworkProbe::tcpConnect(host, port, timeoutMs);
    return {r.connected, r.latencyMs, r.error};
}

QVector<PortScanEntry> IosNetworkProbe::portScan(const QString& host,
                                                   const QVector<int>& ports,
                                                   int timeoutMs, int maxConcurrent) {
    QVector<PortScanEntry> out;
    for (const auto& e : NetworkProbe::portScan(host, ports, timeoutMs, maxConcurrent)) {
        out.append({e.port, e.open, e.error, e.serviceName});
    }
    return out;
}

QVector<int> IosNetworkProbe::commonDiagnosticPorts() const { return NetworkProbe::commonDiagnosticPorts(); }
QMap<int, QString> IosNetworkProbe::wellKnownPorts() const { return NetworkProbe::wellKnownPorts(); }
