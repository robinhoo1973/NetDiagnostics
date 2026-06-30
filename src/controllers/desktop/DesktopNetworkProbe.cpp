// =============================================================================
// DesktopNetworkProbe.cpp — delegates to existing NetworkProbe
// =============================================================================
#include "controllers/desktop/DesktopNetworkProbe.h"
#include "engine/runner/NetworkProbe.h"

TcpConnectResult DesktopNetworkProbe::tcpConnect(const QString& host, int port, int timeoutMs) {
    NetworkProbe::TcpConnectResult r = NetworkProbe::tcpConnect(host, port, timeoutMs);
    TcpConnectResult out;
    out.connected = r.connected;
    out.latencyMs = r.latencyMs;
    out.error = r.error;
    return out;
}

QVector<PortScanEntry> DesktopNetworkProbe::portScan(const QString& host,
                                                       const QVector<int>& ports,
                                                       int timeoutMs,
                                                       int maxConcurrent) {
    auto results = NetworkProbe::portScan(host, ports, timeoutMs, maxConcurrent);
    QVector<PortScanEntry> out;
    for (const auto& e : results) {
        PortScanEntry pe;
        pe.port = e.port;
        pe.open = e.open;
        pe.error = e.error;
        pe.serviceName = e.serviceName;
        out.append(pe);
    }
    return out;
}

QVector<int> DesktopNetworkProbe::commonDiagnosticPorts() const {
    return NetworkProbe::commonDiagnosticPorts();
}

QMap<int, QString> DesktopNetworkProbe::wellKnownPorts() const {
    return NetworkProbe::wellKnownPorts();
}
