// =============================================================================
// NetworkProbe.h — Raw socket wrappers for G4/G5 tests
// =============================================================================
#pragma once

#include <QString>
#include <QDateTime>
#include <QHostAddress>
#include <QUrl>
#include <QVector>
#include <QMap>
#include <functional>

struct TcpConnectResult {
    bool connected = false;
    QString error;
    int latencyMs = 0;
};

struct PortScanEntry {
    int port = 0;
    bool open = false;
    QString error;
    QString serviceName;  // well-known port name, e.g. "http", "ssh"
};

struct SslCertInfo {
    QString subject;
    QString issuer;
    QDateTime validFrom;
    QDateTime validTo;
    qint64 daysLeft = 0;
    QString thumbprint;  // SHA-256
    QStringList subjectAltNames;
    bool valid = false;
};

struct HttpTimingResult {
    int statusCode = 0;
    qint64 dnsMs = 0;
    qint64 connectMs = 0;
    qint64 tlsMs = 0;
    qint64 firstByteMs = 0;  // TTFB
    qint64 downloadMs = 0;
    qint64 totalMs = 0;
    qint64 bodyBytes = 0;
    QString error;
};

class NetworkProbe {
public:
    /// TCP connect to host:port with timeout (ms). Returns result.
    static TcpConnectResult tcpConnect(const QString& host, int port, int timeoutMs = 5000);

    /// Concurrent port scan using raw non-blocking sockets + select().
    /// Scans all ports in the supplied list with true parallelism.
    /// @param host        Target hostname or IP
    /// @param ports       List of ports to scan
    /// @param timeoutMs   Per-connect timeout
    /// @param maxConcurrent  Max simultaneous connections (default 64)
    static QVector<PortScanEntry> portScan(const QString& host,
                                            const QVector<int>& ports,
                                            int timeoutMs = 2000,
                                            int maxConcurrent = 64);

    /// Get SSL certificate info for host:port.
    static SslCertInfo sslCertInfo(const QString& host, int port = 443, int timeoutMs = 10000);

    /// HTTP GET with full timing breakdown.
    static HttpTimingResult httpTiming(const QUrl& url, int timeoutMs = 30000);

    /// Common diagnostic ports with service names.
    static QVector<int> commonDiagnosticPorts();

    /// Well-known port → service name map (e.g. 80→"http", 443→"https").
    static const QMap<int, QString>& wellKnownPorts();
};