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

    /// Get SSL certificate info for host:port.
    static SslCertInfo sslCertInfo(const QString& host, int port = 443, int timeoutMs = 10000);

    /// HTTP GET with full timing breakdown.
    static HttpTimingResult httpTiming(const QUrl& url, int timeoutMs = 30000);

    /// Well-known port → service name map (e.g. 80→"http", 443→"https").
    static const QMap<int, QString>& wellKnownPorts();
};