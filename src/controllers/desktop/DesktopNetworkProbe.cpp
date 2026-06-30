// =============================================================================
// DesktopNetworkProbe.cpp — delegates to existing NetworkProbe
// =============================================================================
#include "controllers/desktop/DesktopNetworkProbe.h"
#include "engine/runner/NetworkProbe.h"

namespace {

INetworkProbe::TcpConnectResult toInterface(const NetworkProbe::TcpConnectResult& r) {
    return {r.connected, r.latencyMs, r.error};
}

INetworkProbe::PortScanEntry toInterface(const NetworkProbe::PortScanEntry& e) {
    return {e.port, e.open, e.error, e.serviceName};
}

} // anonymous namespace

TcpConnectResult DesktopNetworkProbe::tcpConnect(const QString& host, int port, int timeoutMs) {
    return toInterface(NetworkProbe::tcpConnect(host, port, timeoutMs));
}

QVector<PortScanEntry> DesktopNetworkProbe::portScan(const QString& host,
                                                       const QVector<int>& ports,
                                                       int timeoutMs,
                                                       int maxConcurrent) {
    QVector<PortScanEntry> out;
    for (const auto& e : NetworkProbe::portScan(host, ports, timeoutMs, maxConcurrent))
        out.append(toInterface(e));
    return out;
}

QVector<int> DesktopNetworkProbe::commonDiagnosticPorts() const { return NetworkProbe::commonDiagnosticPorts(); }
QMap<int, QString> DesktopNetworkProbe::wellKnownPorts() const { return NetworkProbe::wellKnownPorts(); }
