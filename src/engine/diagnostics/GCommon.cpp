#include "GHelpers.h"
#include "util/NetUtil.h"
namespace G1G2G3Native {
QByteArray httpGet(const QString& host, int port, const QString& path, int timeoutMs, int maxBytes) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return {};
    struct sockaddr_in addr;
    if (!hostToAddr(host, port, addr)) { closeSocket(sock); return {}; }

#ifdef _WIN32
    u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
    ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
    struct timeval tv = {timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    if (select(sock + 1, nullptr, &fdset, nullptr, &tv) <= 0) { closeSocket(sock); return {}; }
    int err = 0; socklen_t len = sizeof(err);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
    if (err != 0) { closeSocket(sock); return {}; }

    // Send HTTP request (loop handles partial sends, EAGAIN-safe)
    QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: NetDiagnostics/1.0\r\nAccept: */*\r\nConnection: close\r\n\r\n")
        .arg(path, host).toUtf8();
    int sent = 0;
    while (sent < req.size()) {
        auto n = ::send(sock, req.constData() + sent, req.size() - sent, 0);
        if (n < 0) {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); struct timeval wfTv = {1,0}; select(sock+1, nullptr, &wf, nullptr, &wfTv); continue; }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); struct timeval wfTv = {1,0}; select(sock+1, nullptr, &wf, nullptr, &wfTv); continue; }
#endif
            break;
        }
        if (n == 0) break;
        sent += n;
    }

    // Read response with wall-clock timeout.
    // 5WHY: recv() returning EAGAIN on a non-blocking socket after select()
    // reports readability is a known kernel race. The old code treated ANY
    // n<=0 as end-of-stream, truncating HTTP responses mid-stream. Now we
    // retry select+recv on EAGAIN instead of breaking.
    QByteArray response; char buf[8192];
    QElapsedTimer recvTimer; recvTimer.start();
    while (response.size() < maxBytes) {
        // 5WHY: using the full timeoutMs for every select() iteration meant
        // EAGAIN retries could each wait timeoutMs again, turning a 3s budget
        // into 30s+. Compute remaining time so the total never exceeds the
        // caller's timeoutMs (clamped to 100ms min to avoid tight spinning).
        int remaining = timeoutMs - (int)recvTimer.elapsed();
        if (remaining <= 0) break;
        int selectMs = qMax(remaining, 100);
        FD_ZERO(&fdset); FD_SET(sock, &fdset);
        tv = {selectMs / 1000, (selectMs % 1000) * 1000};
        if (select(sock + 1, &fdset, nullptr, nullptr, &tv) <= 0) break;
        ssize_t n = recv(sock, buf, sizeof(buf), 0);
        // Wall-clock guard: abort if total recv time exceeds 30 s.
        // MUST come before EAGAIN handling — a continue on EAGAIN would
        // otherwise skip this guard, risking an infinite loop if select()
        // keeps reporting readable but recv() keeps returning EAGAIN.
        if (recvTimer.elapsed() > 30000) break;
        if (n > 0) {
            response.append(buf, (int)n);
        } else if (n == 0) {
            break; // orderly shutdown
        } else {
            // n < 0: could be EAGAIN (retry) or a real error
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK) continue;
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
#endif
            break; // real error
        }
    }
    closeSocket(sock);
    return response;
}

// 闁冲厜鍋撻柍鍏夊亾 HTTP download with throughput measurement 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋?
// SpeedResult defined in GHelpers.h
SpeedResult httpDownload(const QString& urlStr, int targetBytes, int timeoutMs) {
    SpeedResult r = {0, 0, 0, false};
    // Parse URL 闁?host, port, path
    QString u = urlStr;
    if (!u.startsWith("http://")) return r;
    u = u.mid(7); // strip "http://"
    auto slash = u.indexOf('/');
    QString hostPort = (slash > 0) ? u.left(slash) : u;
    QString path = (slash > 0) ? u.mid(slash) : "/";

    QString host = hostPort; int port = 80;
    auto colon = hostPort.lastIndexOf(':');
    if (colon > 0) { host = hostPort.left(colon); port = hostPort.mid(colon + 1).toInt(); }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return r;
    struct sockaddr_in addr;
    if (!hostToAddr(host, port, addr)) { closeSocket(sock); return r; }

#ifdef _WIN32
    u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
    QElapsedTimer t; t.start();
    ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
    struct timeval tv = {3, 0};
    if (select(sock + 1, nullptr, &fdset, nullptr, &tv) <= 0) { closeSocket(sock); return r; }
    int err = 0; socklen_t len = sizeof(err);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
    if (err != 0) { closeSocket(sock); return r; }

    // Send HTTP GET (loop handles partial sends, EAGAIN-safe)
    QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: NetDiagnostics/1.0\r\nConnection: close\r\n\r\n")
        .arg(path, host).toUtf8();
    int reqSent = 0;
    while (reqSent < req.size()) {
        auto n = ::send(sock, req.constData() + reqSent, req.size() - reqSent, 0);
        if (n < 0) {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); struct timeval wfTv = {1,0}; select(sock+1, nullptr, &wf, nullptr, &wfTv); continue; }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); struct timeval wfTv = {1,0}; select(sock+1, nullptr, &wf, nullptr, &wfTv); continue; }
#endif
            break;
        }
        if (n == 0) break;
        reqSent += n;
    }

    // Read with timing 闁?measure throughput (wall-clock guarded)
    qint64 startNs = t.nsecsElapsed();
    QByteArray body;
    QByteArray headerBuf;
    bool headersDone = false;
    bool httpOk = false;
    char buf[32768];
    QElapsedTimer recvGuard; recvGuard.start();
    while (body.size() < targetBytes + 65536) {
        // 5WHY: wall-clock guard was placed after recv() but before data
        // processing. If the guard fired after a successful recv(), the
        // just-read chunk in buf was silently discarded — undercounting
        // bytes or dropping the HTTP status line entirely. Move the guard
        // BEFORE select() so previously-processed data is safe and only
        // NEW reads are prevented.
        if (recvGuard.elapsed() > 60000) break;
        FD_ZERO(&fdset); FD_SET(sock, &fdset);
        tv = {timeoutMs / 1000, (timeoutMs % 1000) * 1000};
        if (select(sock + 1, &fdset, nullptr, nullptr, &tv) <= 0) break;
        ssize_t n = recv(sock, buf, sizeof(buf), 0);
        // 5WHY: treat EAGAIN as retry, not end-of-stream
        if (n > 0) {
            /* process below */
        } else if (n == 0) {
            break; // orderly shutdown
        } else {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK) continue;
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
#endif
            break; // real error
        }
        if (!headersDone) {
            headerBuf.append(buf, (int)n);
            auto hdrEnd = headerBuf.indexOf("\r\n\r\n");
            if (hdrEnd >= 0) {
                // 5WHY: httpDownload accepted ANY HTTP response (200, 404, 500)
                // as valid, measuring error page throughput as download speed.
                // Validate HTTP 2xx by checking the status line only — avoids
                // false match on "200 " appearing in header values.
                QByteArray hdrs = headerBuf.left(hdrEnd);
                int slEnd = hdrs.indexOf('\r');
                QByteArray statusLine = (slEnd > 0) ? hdrs.left(slEnd) : hdrs;
                httpOk = statusLine.contains(" 200 ");
                body = headerBuf.mid(hdrEnd + 4);
                headersDone = true;
                startNs = t.nsecsElapsed(); // reset timer to body start
            }
        } else {
            body.append(buf, (int)n);
        }
    }
    closeSocket(sock);

    qint64 elapsedNs = t.nsecsElapsed() - startNs;
    if (elapsedNs <= 0) elapsedNs = 1;
    r.bytes = static_cast<int>(body.size());
    r.durationMs = (int)(elapsedNs / 1000000);
    // 5WHY: previously only checked bytes>0 && duration>0, which accepted
    // HTTP error pages (404, 500) as valid downloads. Now requires HTTP 2xx.
    if (httpOk && r.bytes > 0 && r.durationMs > 0) {
        double bits = r.bytes * 8.0;
        double secs = r.durationMs / 1000.0;
        r.mbps = bits / secs / 1000000.0;
        r.ok = true;
    }
    return r;
}

// 闁冲厜鍋撻柍鍏夊亾 TCP ping (simple connect RTT) 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋?
int tcpPingMs(const QString& host, int port) {
    QElapsedTimer t; t.start();
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    struct sockaddr_in addr;
    if (!hostToAddr(host, port, addr)) { closeSocket(sock); return -1; }
#ifdef _WIN32
    u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
    ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
    struct timeval tv = {2, 0};
    int sel = select(sock + 1, nullptr, &fdset, nullptr, &tv);
    int ms = static_cast<int>(t.elapsed());
    if (sel > 0) { int err = 0; socklen_t len = sizeof(err); getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len); if (err != 0) ms = -1; } else ms = -1;
    closeSocket(sock);
    return ms;
}

// 闁冲厜鍋撻柍鍏夊亾 HTTP latency via tiny file download (speedtest-cli style latency.txt) 闁冲厜鍋撻柍鍏夊亾
// Measures real application-layer RTT: DNS + TCP connect + HTTP request/response
// Much better predictor of download throughput than raw TCP ping.
int httpLatencyMs(const QString& urlStr, int timeoutMs) {
    QElapsedTimer t; t.start();
    QString u = urlStr;
    if (!u.startsWith("http://")) return -1;
    u = u.mid(7);
    auto slash = u.indexOf('/');
    QString hostPort = (slash > 0) ? u.left(slash) : u;
    QString host = hostPort; int port = 80;
    auto colon = hostPort.lastIndexOf(':');
    if (colon > 0) { host = hostPort.left(colon); port = hostPort.mid(colon + 1).toInt(); }

    // Download latency.txt from server root 闁?speedtest-cli uses the root path
    // regardless of the download/upload URL structure
    QString latPath = QStringLiteral("/latency.txt");
    QByteArray resp = httpGet(host, port, latPath, timeoutMs, 4096);
    if (resp.isEmpty()) return -1;

    // Parse HTTP response 闁?extract body after \r\n\r\n header terminator
    auto hdrEnd = resp.indexOf("\r\n\r\n");
    if (hdrEnd < 0) return -1;
    // 5WHY: httpLatencyMs accepted ANY HTTP response (200, 404, 500) as valid,
    // treating error pages as latency measurements. Validate HTTP 2xx by
    // checking the status line only — avoids false match in header values.
    QByteArray headers = resp.left(hdrEnd);
    int slEnd = headers.indexOf('\r');
    QByteArray statusLine = (slEnd > 0) ? headers.left(slEnd) : headers;
    if (!statusLine.contains(" 200 ")) return -1;
    QByteArray body = resp.mid(hdrEnd + 4);
    // latency.txt should contain a small text like "test=...", we just need the time
    if (body.trimmed().isEmpty()) return -1;

    return static_cast<int>(t.elapsed());
}

}
