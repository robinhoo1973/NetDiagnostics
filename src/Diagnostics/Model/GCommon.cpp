#include "Diagnostics/Model/GHelpers.h"
#include "Common/Utils/NetUtil.h"
namespace G1G2G3Native {
QByteArray httpGet(const QString& host, int port, const QString& path, int timeoutMs, int maxBytes) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return {};
    struct sockaddr_in addr;
    if (!hostToAddr(host, port, addr)) { closeSocket(sock); return {}; }

#if defined(_WIN32)
    u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
    ::connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
    struct timeval tv = {timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    if (select(sock + 1, nullptr, &fdset, nullptr, &tv) <= 0) { closeSocket(sock); return {}; }
    int err = 0; socklen_t len = sizeof(err);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len);
    if (err != 0) { closeSocket(sock); return {}; }

    // Send HTTP request (loop handles partial sends, EAGAIN-safe)
    // 5WHY: Host header omitted port number. Per RFC 7230 §5.4, the port
    // SHOULD be included when non-default (i.e. ≠ 80).  Speed-test servers
    // on port 8080 behind reverse proxies may route incorrectly without it.
    QString hostHeader = (port != 80) ? QStringLiteral("%1:%2").arg(host).arg(port) : host;
    QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: NetDiagnostics/1.0\r\nAccept: */*\r\nConnection: close\r\n\r\n")
        .arg(path, hostHeader).toUtf8();
    int sent = 0;
    while (sent < req.size()) {
        auto n = ::send(sock, req.constData() + sent, req.size() - sent, 0);
        if (n < 0) {
#if defined(_WIN32)
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
        // caller's timeoutMs. Use a 50ms floor to avoid tight spinning when
        // remaining is tiny, but always use the full remaining budget — never
        // cap the per-iteration timeout. Capping (e.g. at 500ms) causes
        // premature break on slow connections where the server takes >500ms
        // to respond, even though the caller's budget still has time.
        int remaining = timeoutMs - (int)recvTimer.elapsed();
        if (remaining <= 0) break;
        int selectMs = qMax(remaining, 50);
        FD_ZERO(&fdset); FD_SET(sock, &fdset);
        tv = {selectMs / 1000, (selectMs % 1000) * 1000};
        if (select(sock + 1, &fdset, nullptr, nullptr, &tv) <= 0) break;
        // 5WHY: Wall-clock guard was AFTER recv(). If the guard fires when
        // recv() just read a valid chunk, that chunk was silently discarded.
        // Move the guard BEFORE recv() so it aborts the iteration without
        // losing already-received data — same pattern as httpDownload.
        if (recvTimer.elapsed() > 30000) break;
        ssize_t n = recv(sock, buf, sizeof(buf), 0);
        if (n > 0) {
            // 5WHY: recv() can return up to sizeof(buf) bytes, which may
            // overshoot maxBytes by up to 8191 bytes. Truncate the append
            // so the caller's maxBytes cap is actually respected.
            int remain = maxBytes - response.size();
            response.append(buf, qMin((int)n, remain));
        } else if (n == 0) {
            break; // orderly shutdown
        } else {
            // n < 0: could be EAGAIN (retry) or a real error
#if defined(_WIN32)
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

// HTTP download with throughput measurement
// SpeedResult defined in GHelpers.h
SpeedResult httpDownload(const QString& urlStr, int targetBytes, int timeoutMs) {
    SpeedResult r = {0, 0, 0, false};
    // Parse URL -- host, port, path
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

#if defined(_WIN32)
    u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
    QElapsedTimer t; t.start();
    ::connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
    struct timeval tv = {3, 0};
    if (select(sock + 1, nullptr, &fdset, nullptr, &tv) <= 0) { closeSocket(sock); return r; }
    int err = 0; socklen_t len = sizeof(err);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len);
    if (err != 0) { closeSocket(sock); return r; }

    // Send HTTP GET (loop handles partial sends, EAGAIN-safe)
    // 5WHY: Host header omitted port — same bug that was fixed in httpGet()
    // (see above). Speed-test servers on port 8080 behind reverse proxies
    // may route incorrectly without the explicit port per RFC 7230 §5.4.
    QString dlHostHeader = (port != 80) ? QStringLiteral("%1:%2").arg(host).arg(port) : host;
    QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: NetDiagnostics/1.0\r\nConnection: close\r\n\r\n")
        .arg(path, dlHostHeader).toUtf8();
    int reqSent = 0;
    while (reqSent < req.size()) {
        auto n = ::send(sock, req.constData() + reqSent, req.size() - reqSent, 0);
        if (n < 0) {
#if defined(_WIN32)
            if (WSAGetLastError() == WSAEWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); struct timeval wfTv = {1,0}; select(sock+1, nullptr, &wf, nullptr, &wfTv); continue; }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); struct timeval wfTv = {1,0}; select(sock+1, nullptr, &wf, nullptr, &wfTv); continue; }
#endif
            break;
        }
        if (n == 0) break;
        reqSent += n;
    }

    // Read with timing 闂?measure throughput (wall-clock guarded)
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
        // just-read chunk in buf was silently discarded 鈥?undercounting
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
#if defined(_WIN32)
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
                // Validate HTTP 2xx by checking the status line only 鈥?avoids
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

// 闂佸啿鍘滈崑鎾绘煃閸忓浜?TCP ping (simple connect RTT) 闂佸啿鍘滈崑鎾绘煃閸忓浜鹃梺鍐插帨閸嬫捇鏌嶉崗澶婁壕闂佸啿鍘滈崑鎾绘煃閸忓浜鹃梺鍐插帨閸嬫捇鏌嶉崗澶婁壕闂佸啿鍘滈崑鎾绘煃閸忓浜鹃梺鍐插帨閸嬫捇鏌嶉崗澶婁壕闂佸啿鍘滈崑鎾绘煃閸忓浜鹃梺鍐插帨閸嬫捇鏌嶉崗澶婁壕闂佸啿鍘滈崑鎾绘煃閸忓浜鹃梺鍐插帨閸嬫捇鏌嶉崗澶婁壕闂佸啿鍘滈崑鎾绘煃閸忓浜鹃梺鍐插帨閸嬫捇鏌嶉崗澶婁壕闂佸啿鍘滈崑鎾绘煃閸忓浜鹃梺鍐插帨閸嬫捇鏌嶉崗澶婁壕闂佸啿鍘滈崑鎾绘煃閸忓浜鹃梺鍐插帨閸嬫捇鏌嶉崗澶婁壕闂佸啿鍘滈崑鎾绘煃閸忓浜鹃梺鍐插帨閸嬫捇鏌嶉崗澶婁壕闂佸啿鍘滈崑鎾绘煃閸忓浜鹃梺鍐插帨閸?
int tcpPingMs(const QString& host, int port) {
    QElapsedTimer t; t.start();
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    struct sockaddr_in addr;
    if (!hostToAddr(host, port, addr)) { closeSocket(sock); return -1; }
#if defined(_WIN32)
    u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
    ::connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
    struct timeval tv = {2, 0};
    int sel = select(sock + 1, nullptr, &fdset, nullptr, &tv);
    int ms = static_cast<int>(t.elapsed());
    if (sel > 0) { int err = 0; socklen_t len = sizeof(err); getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len); if (err != 0) ms = -1; } else ms = -1;
    closeSocket(sock);
    return ms;
}

// ── TCP ping with 50x averaging for sub-ms differentiation ──
// 5WHY: single tcpPingMs() can't differentiate servers with ≤1ms RTT
// (timer resolution limit).  Running 50 connects and averaging gives
// ~0.02ms effective resolution — enough to rank nearby servers.
// First call verifies reachability; if single latency > 1ms, return
// directly (already differentiated).  If ≤ 1ms, run 49 more (50 total).
double tcpPingAvg(const QString& host, int port) {
    int first = tcpPingMs(host, port);
    // 5WHY: tcpPingMs returns elapsed ms truncated to int — a 950μs
    // connect returns 0 (not -1).  first < 0 means true failure (timeout
    // or SO_ERROR).  first == 0 means "success at <1ms" — those need the
    // 50x averaging even more than first==1 servers.
    if (first < 0) return -1.0;         // truly unreachable
    if (first > 1) return (double)first; // already differentiated

    // Sub-ms region: run 50 total connects, average the results
    long long totalUs = 0;
    int count = 0;
    for (int i = 0; i < 50; i++) {
        QElapsedTimer t; t.start();
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;
        struct sockaddr_in addr;
        if (!hostToAddr(host, port, addr)) { closeSocket(sock); continue; }
#if defined(_WIN32)
        u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
#else
        int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
        ::connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
        fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
        struct timeval tv = {2, 0};
        int sel = select(sock + 1, nullptr, &fdset, nullptr, &tv);
        if (sel > 0) {
            int err = 0; socklen_t len = sizeof(err);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len);
            if (err == 0) {
                totalUs += t.nsecsElapsed() / 1000; // ns → us
                count++;
            }
        }
        closeSocket(sock);
    }
    if (count == 0) return -1.0;
    // Return average in ms (μs / 1000.0)
    return (totalUs / (double)count) / 1000.0;
}

// 闂佸啿鍘滈崑鎾绘煃閸忓浜?HTTP latency via tiny file download (speedtest-cli style latency.txt) 闂佸啿鍘滈崑鎾绘煃閸忓浜?// Measures real application-layer RTT: DNS + TCP connect + HTTP request/response
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

    // Download latency.txt from server root 闂?speedtest-cli uses the root path
    // regardless of the download/upload URL structure
    QString latPath = QStringLiteral("/latency.txt");
    QByteArray resp = httpGet(host, port, latPath, timeoutMs, 4096);
    if (resp.isEmpty()) return -1;

    // Parse HTTP response 闂?extract body after \r\n\r\n header terminator
    auto hdrEnd = resp.indexOf("\r\n\r\n");
    if (hdrEnd < 0) return -1;
    // 5WHY: httpLatencyMs accepted ANY HTTP response (200, 404, 500) as valid,
    // treating error pages as latency measurements. Validate HTTP 2xx by
    // checking the status line only 鈥?avoids false match in header values.
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
