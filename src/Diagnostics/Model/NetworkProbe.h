// =============================================================================
// NetworkProbe.h — High-level Qt socket wrappers for G4/G5 tests (QTcpSocket/QSslSocket)
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

    /// TCP connect + read: connects, optionally sends probe data, reads response,
    /// and disconnects.  Returns the received data and elapsed time.
    /// 5WHY: 10 G5 protocol diagnostics each duplicated the same 5-line socket
    /// lifecycle (QElapsedTimer + QTcpSocket + connectToHost + waitForReadyRead
    /// + readAll + disconnectFromHost).  Extract once so timeout handling and
    /// error reporting are centrally maintained.
    struct TcpProbeResult {
        bool connected = false;
        QByteArray data;
        qint64 elapsedMs = 0;
    };
    static TcpProbeResult tcpProbe(const QString& host, int port,
                                    int connectTimeoutMs = 5000,
                                    int readTimeoutMs = 3000,
                                    const QByteArray& sendData = QByteArray());

    /// Get SSL certificate info for host:port.
    static SslCertInfo sslCertInfo(const QString& host, int port = 443, int timeoutMs = 10000);
};