// =============================================================================
// INetworkProbe.h — Interface for port scan / TCP connect
// =============================================================================
#pragma once
#include <QString>
#include <QVector>
#include <QMap>

struct PortScanEntry {
    int port = 0;
    bool open = false;
    QString error;
    QString serviceName;
};

struct TcpConnectResult {
    bool connected = false;
    int latencyMs = 0;
    QString error;
};

class INetworkProbe {
public:
    virtual ~INetworkProbe() = default;

    virtual TcpConnectResult tcpConnect(const QString& host, int port, int timeoutMs = 5000) = 0;
    virtual QVector<PortScanEntry> portScan(const QString& host,
                                            const QVector<int>& ports,
                                            int timeoutMs = 2000,
                                            int maxConcurrent = 64) = 0;
    virtual QVector<int> commonDiagnosticPorts() const = 0;
    virtual QMap<int, QString> wellKnownPorts() const = 0;

    virtual bool isAvailable() const = 0;
};
