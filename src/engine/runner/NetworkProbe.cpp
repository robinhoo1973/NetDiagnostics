// =============================================================================
// NetworkProbe.cpp — Raw socket wrappers for G4/G5 tests
// =============================================================================
#include "engine/runner/NetworkProbe.h"
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
#include <thread>
#include <chrono>

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

// ── DNS resolution with timeout (3s) — prevents indefinite getaddrinfo blocking ──
static QString resolveHostWithTimeout(const QString& host, int timeoutMs = 3000) {
    struct in_addr ip4;
    if (inet_pton(AF_INET, host.toUtf8().constData(), &ip4) == 1)
        return host;

    static QMutex cacheMutex;
    static QHash<QString, QString> dnsCache;
    {
        QMutexLocker locker(&cacheMutex);
        if (dnsCache.contains(host))
            return dnsCache[host];
    }

    struct ResolveState { std::atomic<bool> done{false}; QString ip; };
    ResolveState st;
    // Capture host by value — avoid dangling reference when caller passes a temporary.
    std::thread t([&st, host]() {
        struct addrinfo hints = {}, *res;
        hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
        QByteArray hb = host.toUtf8();
        if (getaddrinfo(hb.constData(), nullptr, &hints, &res) == 0) {
            char ip[INET_ADDRSTRLEN];
            auto* sa = (struct sockaddr_in*)res->ai_addr;
            inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
            st.ip = QString::fromLatin1(ip);
            freeaddrinfo(res);
        }
        st.done.store(true, std::memory_order_release);
    });

    auto start = std::chrono::steady_clock::now();
    while (!st.done.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeoutMs) break;
    }
    t.detach();
    // Only read st.ip if the thread completed — avoids data race on non-atomic QString
    if (!st.done.load(std::memory_order_acquire))
        return {}; // timeout: thread still writing st.ip, return empty
    {
        QMutexLocker locker(&cacheMutex);
        if (!st.ip.isEmpty())
            dnsCache[host] = st.ip; // only cache successful lookups
    }
    return st.ip;
}

// ── Helper: resolve hostname → IPv4 (host byte order) ────────────────────────
static quint32 resolveIPv4(const QString& host) {
    QHostInfo info = QHostInfo::fromName(host);
    if (!info.addresses().isEmpty()) {
        quint32 ip = info.addresses().first().toIPv4Address();
        if (ip) return ntohl(ip); // QHostInfo returns NBO → convert to HBO
    }
    // 2. Fallback to getaddrinfo with timeout (libc resolver)
    QString ipStr = resolveHostWithTimeout(host, 3000);
    if (!ipStr.isEmpty()) {
        struct in_addr a;
        if (inet_pton(AF_INET, ipStr.toUtf8().constData(), &a) == 1)
            return ntohl(a.s_addr);
    }
    return 0;
}

// ── Helper: set socket non-blocking ──────────────────────────────────────────
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

// ── Helper: check if non-blocking connect succeeded ──────────────────────────
static bool connectSuccess(int sock) {
    // Verify connection actually completed (not still EINPROGRESS)
    struct sockaddr_in peer;
    socklen_t peerLen = sizeof(peer);
    if (getpeername(sock, (struct sockaddr*)&peer, &peerLen) < 0) {
        // Not connected yet — still in progress
        return false;
    }
    // Connection completed — check for errors
    int err = 0;
    socklen_t len = sizeof(err);
#ifdef _WIN32
    getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
#else
    getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len);
#endif
    return err == 0;
}

// ═════════════════════════════════════════════════════════════════════════════
// TCP Connect
// ═════════════════════════════════════════════════════════════════════════════

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

// ═════════════════════════════════════════════════════════════════════════════
// Port Scan — raw non-blocking socket + select(), true concurrent
// ═════════════════════════════════════════════════════════════════════════════

QVector<PortScanEntry> NetworkProbe::portScan(const QString& host,
                                                const QVector<int>& ports,
                                                int timeoutMs,
                                                int maxConcurrent) {
    QVector<PortScanEntry> results;
    if (ports.isEmpty()) return results;

    quint32 targetIp = resolveIPv4(host);
    if (!targetIp) {
        for (int port : ports) {
            PortScanEntry e; e.port = port; e.open = false;
            e.error = QStringLiteral("DNS resolution failed");
            e.serviceName = wellKnownPorts().value(port);
            results.append(e);
        }
        return results;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(targetIp);

    const auto& wkPorts = wellKnownPorts();
    auto total = ports.size();
    int nextIdx = 0;

    // Process ports in batches of maxConcurrent
    while (nextIdx < total) {
        auto batchSize = qMin(maxConcurrent, total - nextIdx);

        // Create non-blocking sockets and initiate connects
        QVector<int> sockets(batchSize, -1);
        QVector<QElapsedTimer> timers(batchSize);
        int maxFd = -1;

        for (int i = 0; i < batchSize; ++i) {
            int port = ports[nextIdx + i];
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                PortScanEntry e; e.port = port; e.open = false;
                e.error = QStringLiteral("socket() failed");
                e.serviceName = wkPorts.value(port);
                results.append(e);
                sockets[i] = -1;
                continue;
            }
            sockets[i] = sock;
            setNonBlocking(sock);

            addr.sin_port = htons(port);
            timers[i].start();
            ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));

#ifdef _WIN32
            if (sock > maxFd) maxFd = sock;
#else
            if (sock > maxFd) maxFd = sock;
#endif
        }

        // Wait with select() for connection results
        QElapsedTimer batchTimer; batchTimer.start();
        auto remaining = batchSize;
        int elapsedTotal = 0;

        while (remaining > 0 && elapsedTotal < timeoutMs + 100) {
            fd_set wfds, efds;
            FD_ZERO(&wfds); FD_ZERO(&efds);
            int fdCount = 0;

            for (int i = 0; i < batchSize; ++i) {
                int sock = sockets[i];
                if (sock < 0) continue;
                FD_SET(sock, &wfds);
                FD_SET(sock, &efds);
                fdCount++;
            }
            if (fdCount == 0) break;

            // Calculate remaining time
            int remainingMs = timeoutMs - elapsedTotal;
            if (remainingMs < 5) remainingMs = 5;
            struct timeval tv = {remainingMs / 1000, (remainingMs % 1000) * 1000};

            int sel = select(maxFd + 1, nullptr, &wfds, &efds, &tv);
            elapsedTotal = (int)batchTimer.elapsed();

            if (sel <= 0) break; // timeout or error

            // Process ready sockets
            for (int i = 0; i < batchSize; ++i) {
                int sock = sockets[i];
                if (sock < 0) continue;

                bool ready = FD_ISSET(sock, &wfds) || FD_ISSET(sock, &efds);
                if (!ready) continue;

                int port = ports[nextIdx + i];
                int rtt = (int)timers[i].elapsed();
                bool open = connectSuccess(sock);

                PortScanEntry e;
                e.port = port;
                e.open = open;
                e.serviceName = wkPorts.value(port);
                if (!open) e.error = QStringLiteral("closed/timeout");
                results.append(e);

                close(sock);
                sockets[i] = -1;
                remaining--;
            }

            if (elapsedTotal >= timeoutMs) break;
        }

        // Close any remaining sockets (timeouts)
        for (int i = 0; i < batchSize; ++i) {
            int sock = sockets[i];
            if (sock < 0) continue;
            int port = ports[nextIdx + i];
            PortScanEntry e;
            e.port = port;
            e.open = false;
            e.error = QStringLiteral("timeout");
            e.serviceName = wkPorts.value(port);
            results.append(e);
            close(sock);
            sockets[i] = -1;
        }

        nextIdx += batchSize;
    }

    // Sort by port number
    std::sort(results.begin(), results.end(),
              [](const PortScanEntry& a, const PortScanEntry& b) { return a.port < b.port; });

    return results;
}

// ═════════════════════════════════════════════════════════════════════════════
// SSL Certificate
// ═════════════════════════════════════════════════════════════════════════════

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

// ═════════════════════════════════════════════════════════════════════════════
// HTTP Timing
// ═════════════════════════════════════════════════════════════════════════════

HttpTimingResult NetworkProbe::httpTiming(const QUrl& url, int timeoutMs) {
    HttpTimingResult result;
    QElapsedTimer totalTimer; totalTimer.start();
    QElapsedTimer t; t.start();
    QHostInfo hi = QHostInfo::fromName(url.host());
    result.dnsMs = t.elapsed();
    QNetworkAccessManager mgr;
    QNetworkRequest req(url);
    req.setRawHeader("Accept-Encoding", "gzip, deflate");
    QNetworkReply* reply = mgr.get(req);
    QEventLoop loop;
    QTimer timer; timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();
    if (timer.isActive()) {
        timer.stop();
        result.totalMs = totalTimer.elapsed();
        result.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result.bodyBytes = reply->readAll().size();
        result.firstByteMs = result.totalMs - result.dnsMs;
    } else {
        result.error = "Request timeout";
        reply->abort();
    }
    return result;
}

// ═════════════════════════════════════════════════════════════════════════════
// Common Diagnostic Ports
// ═════════════════════════════════════════════════════════════════════════════

QVector<int> NetworkProbe::commonDiagnosticPorts() {
    return {21,22,23,25,53,80,110,135,139,143,443,445,993,995,
            1433,1521,1723,3306,3389,5432,5900,6379,8080,8443,27017};
}

// ═════════════════════════════════════════════════════════════════════════════
// Well-known Port Names
// ═════════════════════════════════════════════════════════════════════════════

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