// =============================================================================
// NetworkProbe.cpp -- High-level Qt socket wrappers for G4/G5 tests
//
// 5WHY: Three local raw-socket helpers (resolveIPv4, setNonBlocking,
// connectSuccess) were dead code -- this file uses QTcpSocket/QSslSocket
// exclusively. They were leftovers from an earlier implementation and
// duplicated equivalents in NetUtil.h. Removed to reduce maintenance burden.
//
// 5WHY: The file header said "Raw socket wrappers" but all methods use
// QTcpSocket/QSslSocket -- the Qt event-loop-integrated socket classes.
// Renamed to accurately describe the implementation.
// =============================================================================
#include "Diagnostics/Model/NetworkProbe.h"
#include <QTcpSocket>
#include <QSslSocket>
#include <QElapsedTimer>
#include <QDateTime>
#include <QCryptographicHash>
#include <algorithm>

// =============================================================================
// TCP Connect
// =============================================================================
TcpConnectResult NetworkProbe::tcpConnect(const QString& host, int port, int timeoutMs) {
    TcpConnectResult result;
    QTcpSocket socket;
    QElapsedTimer timer;
    timer.start();
    socket.connectToHost(host, port);
    if (socket.waitForConnected(timeoutMs)) {
        result.connected = true;
        result.latencyMs = static_cast<int>(timer.elapsed());
    } else {
        result.error = socket.errorString();
    }
    socket.disconnectFromHost();
    return result;
}

// 5WHY: iOS Clang (strict C++17) requires the fully-qualified return type
// when defining a member function outside the class body.  Desktop GCC/Clang
// resolve TcpProbeResult unqualified via the class scope, but Apple Clang
// for arm64 iOS does not.
NetworkProbe::TcpProbeResult NetworkProbe::tcpProbe(const QString& host, int port,
                                       int connectTimeoutMs, int readTimeoutMs,
                                       const QByteArray& sendData) {
    TcpProbeResult r;
    QElapsedTimer t; t.start();
    QTcpSocket sock;
    sock.connectToHost(host, port);
    if (!sock.waitForConnected(connectTimeoutMs)) {
        r.elapsedMs = t.elapsed();
        return r;
    }
    r.connected = true;
    if (!sendData.isEmpty())
        sock.write(sendData);
    // 5WHY: a single waitForReadyRead + readAll returned partial frames on
    // slow/segmented connections — protocol tests (MySQL handshake, MQTT
    // CONNACK, service banners) misread them as truncated/failed.  Accumulate
    // until the read deadline expires or the peer pauses 300ms after sending
    // data, so a response delivered in multiple TCP segments is captured whole.
    QElapsedTimer read; read.start();
    qint64 idleSince = -1;
    while (read.elapsed() < readTimeoutMs) {
        qint64 remaining = qMax<qint64>(1, readTimeoutMs - read.elapsed());
        if (!sock.waitForReadyRead(remaining)) break;
        qint64 before = r.data.size();
        r.data.append(sock.readAll());
        if (r.data.size() > before) {
            idleSince = read.elapsed();
        } else {
            // 5WHY: after the peer sends its response and closes, waitForReadyRead
            // returns true immediately (disconnect is "readable") and readAll()
            // yields nothing — the loop would busy-spin until the 300ms idle
            // threshold.  Break as soon as the socket is closed with no new data.
            if (sock.state() == QAbstractSocket::UnconnectedState) break;
            if (idleSince >= 0 && read.elapsed() - idleSince >= 300) break;
        }
    }
    sock.disconnectFromHost();
    r.elapsedMs = t.elapsed();
    return r;
}

// =============================================================================
// SSL Certificate Info (used by G5SslCertificate)
// =============================================================================

SslCertInfo NetworkProbe::sslCertInfo(const QString& host, int port, int timeoutMs) {
    SslCertInfo info;
    QSslSocket socket;
    socket.connectToHostEncrypted(host, port);
    if (!socket.waitForEncrypted(timeoutMs)) return info;
    const auto certs = socket.peerCertificateChain();
    if (certs.isEmpty()) { socket.disconnectFromHost(); return info; }
    const auto& cert = certs.first();
    info.subject = cert.subjectInfo(QSslCertificate::CommonName).join(", ");
    info.issuer = cert.issuerInfo(QSslCertificate::CommonName).join(", ");
    info.validFrom = cert.effectiveDate();
    info.validTo = cert.expiryDate();
    info.daysLeft = QDateTime::currentDateTime().daysTo(info.validTo);
    info.thumbprint = QString::fromUtf8(cert.digest(QCryptographicHash::Sha256).toHex());
    info.subjectAltNames = cert.subjectAlternativeNames().values();
    info.valid = true;
    socket.disconnectFromHost();
    return info;
}

// =============================================================================
// Well-known Port Names
// =============================================================================
const QMap<int, QString>& NetworkProbe::wellKnownPorts() {
    static const QMap<int, QString> map = {
        {21, "ftp"},       {22, "ssh"},        {23, "telnet"},
        {25, "smtp"},      {53, "dns"},         {80, "http"},
        {110, "pop3"},     {135, "epmap"},      {139, "netbios"},
        {143, "imap"},     {443, "https"},      {445, "smb"},
        {993, "imaps"},    {995, "pop3s"},      {1433, "mssql"},
        {1521, "oracle"},  {1723, "pptp"},      {3306, "mysql"},
        {3389, "rdp"},     {5432, "postgresql"},{5900, "vnc"},
        {6379, "redis"},   {8080, "http-proxy"},{8443, "https-alt"},
        {27017, "mongodb"}
    };
    return map;
}
