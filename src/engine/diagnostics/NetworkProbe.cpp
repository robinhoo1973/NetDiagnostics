// =============================================================================
// NetworkProbe.cpp 鈥?Raw socket wrappers for G4/G5 tests
// =============================================================================
#include "engine/diagnostics/NetworkProbe.h"
#include <QTcpSocket>
#include <QSslSocket>
#include <QHostInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QMutex>
#include <QtConcurrent/QtConcurrent>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#define close closesocket
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif
#include "util/DnsResolver.h"


// 鈹€鈹€ Helper: resolve hostname 鈫?IPv4 (host byte order) 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
static quint32 resolveIPv4(const QString& host) {
    return DnsResolver::resolveIPv4(host, 3000);
}

// 鈹€鈹€ Helper: set socket non-blocking 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
static bool setNonBlocking(int sock) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

// 鈹€鈹€ Helper: check if non-blocking connect succeeded 鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€鈹€
static bool connectSuccess(int sock) {
    // Verify connection actually completed (not still EINPROGRESS)
    struct sockaddr_in peer;
    socklen_t peerLen = sizeof(peer);
    if (getpeername(sock, (struct sockaddr*)&peer, &peerLen) < 0) {
        // Not connected yet 鈥?still in progress
        return false;
    }
    // Connection completed 鈥?check for errors
    int err = 0;
    socklen_t len = sizeof(err);
#ifdef _WIN32
    getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
#else
    getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len);
#endif
    return err == 0;
}

// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?// TCP Connect
// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?
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
// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?// Well-known Port Names
// 鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺愨晲鈺?
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
