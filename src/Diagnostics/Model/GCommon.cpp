#include "Diagnostics/Model/GHelpers.h"
#include "Common/Utils/NetUtil.h"
#include <QMutex>
#include <QMutexLocker>
namespace G1G2G3Native {
QByteArray httpGet(const QString& host, int port, const QString& path, int timeoutMs, int maxBytes) {
    // 5WHY: was 17 lines of socket+connect+select boilerplate,
    // duplicating tcpConnect() from NetUtil.h.  Now 1 call.
    int sock = tcpConnect(host, port, timeoutMs);
    if (sock < 0) return {};
    fd_set fdset; struct timeval tv; // reused by send/recv loops below

    // Send HTTP request (loop handles partial sends, EAGAIN-safe)
    // 5WHY: Host header omitted port number. Per RFC 7230 §5.4, the port
    // SHOULD be included when non-default (i.e. ≠ 80).  Speed-test servers
    // on port 8080 behind reverse proxies may route incorrectly without it.
    QString hostHeader = (port != 80) ? QStringLiteral("%1:%2").arg(host).arg(port) : host;
    QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: NetDiagnostics/1.0\r\nAccept: */*\r\nConnection: close\r\n\r\n")
        .arg(path, hostHeader).toUtf8();
    int sent = 0;
    QElapsedTimer sendGuard; sendGuard.start();
    while (sent < req.size()) {
        if (sendGuard.elapsed() > 30000) break; // 30s hard guard against stalled send
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
    if (sent < req.size()) { closeSocket(sock); return {}; } // incomplete send, don't wait for response

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
        int selectMs = qMax(remaining, 50); // floor at 50ms avoids tight spinning when budget nearly exhausted
        FD_ZERO(&fdset); FD_SET(sock, &fdset);
        tv = {selectMs / 1000, (selectMs % 1000) * 1000};
        if (select(sock + 1, &fdset, nullptr, nullptr, &tv) <= 0) break;
        // 5WHY: Wall-clock guard was AFTER recv(). If the guard fires when
        // recv() just read a valid chunk, that chunk was silently discarded.
        // Move the guard BEFORE recv() so it aborts the iteration without
        // losing already-received data — same pattern as httpDownload.
        // 5WHY: hardcoded 30000 overrides caller's timeoutMs — a 10s
        // caller could block for 30s.  Use the caller-specified timeout.
        if (recvTimer.elapsed() > timeoutMs) break;
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

    QElapsedTimer t; t.start();
    int sock = tcpConnect(host, port, 3000);
    if (sock < 0) return r;
    fd_set fdset; struct timeval tv; // reused by send/recv loops below

    // Send HTTP GET (loop handles partial sends, EAGAIN-safe)
    // 5WHY: Host header omitted port — same bug that was fixed in httpGet()
    // (see above). Speed-test servers on port 8080 behind reverse proxies
    // may route incorrectly without the explicit port per RFC 7230 §5.4.
    QString dlHostHeader = (port != 80) ? QStringLiteral("%1:%2").arg(host).arg(port) : host;
    QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: NetDiagnostics/1.0\r\nConnection: close\r\n\r\n")
        .arg(path, dlHostHeader).toUtf8();
    int reqSent = 0;
    QElapsedTimer sendGuard; sendGuard.start();
    while (reqSent < req.size()) {
        if (sendGuard.elapsed() > 30000) break; // 30s hard guard (matching httpGet)
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
    if (reqSent < req.size()) { closeSocket(sock); return r; } // incomplete send

    // Read with timing — measure throughput (wall-clock guarded)
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
        // 5WHY: hardcoded 60000 inconsistent with httpGet timeoutMs fix.
        // Use the caller's timeout as the wall-clock guard — a 10s caller
        // should not block for 60s.  Keep a 60s ceiling for large downloads.
        // qMIN (not qMAX): respect caller's timeout, with 60s safety net.
        int remaining = qMin(timeoutMs, 60000) - (int)recvGuard.elapsed();
        if (remaining <= 0) break;
        int selectMs = qMax(remaining, 50); // matching httpGet pattern
        FD_ZERO(&fdset); FD_SET(sock, &fdset);
        tv = {selectMs / 1000, (selectMs % 1000) * 1000};
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
                // 5WHY: statusLine.contains(" 200 ") was too strict — fails
                // on "HTTP/1.0 200" (no trailing OK) or "HTTP/1.1 200\r\n"
                // (no trailing space).  Now extracts the 3-digit code after
                // the first space and checks == "200".
                QByteArray hdrs = headerBuf.left(hdrEnd);
                int slEnd = hdrs.indexOf('\r');
                QByteArray statusLine = (slEnd > 0) ? hdrs.left(slEnd) : hdrs;
                int codeStart = statusLine.indexOf(' ');
                httpOk = (codeStart > 0 && codeStart + 4 <= statusLine.size()
                          && statusLine.mid(codeStart + 1, 3) == "200");
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
    // 5WHY: httpOk was too strict — CN speed-test servers on port 8080 may
    // respond with non-standard HTTP (302, chunked), custom binary protocol,
    // or just raw data without HTTP headers (\r\n\r\n never found).
    // Also handle case where headersDone=false — data accumulated in headerBuf
    // but never parsed as HTTP. Any response with meaningful data (>1KB) is
    // usable for throughput measurement.
    if (!headersDone && headerBuf.size() > 1000) {
        body = headerBuf;  // raw binary response, no HTTP parsing
        r.bytes = static_cast<int>(body.size());
    }
    bool usable = (httpOk || r.bytes > 1000) && r.bytes > 0 && r.durationMs > 0;
    if (usable) {
        double bits = r.bytes * 8.0;
        double secs = r.durationMs / 1000.0;
        r.mbps = bits / secs / 1000000.0;
        r.ok = true;
    }
    return r;
}

// TCP ping (simple connect RTT) — measures raw TCP handshake latency
int tcpPingMs(const QString& host, int port) {
    QElapsedTimer t; t.start();
    int sock = tcpConnect(host, port, 2000);
    int ms = static_cast<int>(t.elapsed());
    if (sock < 0) ms = -1;
    else closeSocket(sock);
    return ms;
}

// ── Calibrated TCP ping — MAD outlier rejection + Hodges-Lehmann ──
// 5WHY: 10% trimmed mean discards a fixed tail proportion regardless
// of data quality.  Clean data loses 10% of information for no reason;
// dirty data with mid-body contamination (e.g. 10 measurements at ~8ms
// among 40 at ~3ms) isn't caught because the contamination sits inside
// the 10%-90% band.  MAD (Median Absolute Deviation) adapts the
// rejection threshold to the actual spread of the data.
//
// Algorithm:
//   1. Quick reachability check (single connect)
//      → fail — server unreachable, return immediately
//   2. 50 connects, count successes vs transient failures
//   3. If < 5 successes — fallback to median, flag unusable
//   4. MAD filter (k=2.5 → ~1.2% false positive under normality)
//      → removes measurements beyond k × 1.4826 × MAD from median
//   5. Hodges-Lehmann on survivors — median of all pairwise averages
//      → 29% breakdown, 96% Gaussian efficiency
//      → natural companion to downstream Wilcoxon test
//
// Edge cases handled:
//   - 0 successes        → latencyMs=-1, usable=false
//   - < 5 successes      → median fallback, usable=false
//   - MAD == 0           → skip filter (all values identical)
//   - filter too narrow  → fallback to median (MAD overfit on clean data)
//   - single clean value → HL = self-average = clean[0]
// ── Shared TCP probe cache (120s TTL) ──────────────────────────────
// 5WHY: Both vpnStatus and speedTest probe overlapping servers.
// Without a shared cache, the same host:port gets probed twice
// (~100 redundant TCP connects).  This cache is shared between
// tcpPingAvg and tcpPingCalibrated — whichever probes first
// populates the entry; the second caller gets a cache hit.
//
// NOTE: get-then-put is not atomic — two callers may both miss and
// both compute.  Acceptable: bounded cost (~50 connects, ~1s) and
// both writers store approximately the same value.
namespace {
struct ProbeCache {
    QMap<QString, double> map;
    qint64 ts = 0;
    QMutex mtx;

    double get(const QString& key) {
        QMutexLocker lock(&mtx);
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - ts > 120000) { map.clear(); ts = now; }
        auto it = map.constFind(key);
        return (it != map.cend()) ? *it : -1.0;
    }
    void put(const QString& key, double val) {
        QMutexLocker lock(&mtx);
        map[key] = val;
    }
};
ProbeCache sProbeCache;
} // anonymous namespace

TcpPingResult tcpPingCalibrated(const QString& host, int port) {
    TcpPingResult r;

    // ── Cache check ───────────────────────────────────────────────
    QString cacheKey = QStringLiteral("%1:%2").arg(host).arg(port);
    double cached = sProbeCache.get(cacheKey);
    if (cached >= 0) {
        r.latencyMs = cached;
        r.successes = 50; r.failRate = 0.0; r.usable = true;
        return r;
    }

    // ── Reachability gate ─────────────────────────────────────────
    int first = tcpPingMs(host, port);
    if (first < 0) {
        r.successes = 0; r.failRate = 1.0;
        return r; // unreachable
    }

    // ── 50-connect measurement ────────────────────────────────────
    QVector<long long> measurements; // microseconds
    measurements.reserve(50);
    r.attempts = 50;
    for (int i = 0; i < 50; i++) {
        QElapsedTimer t; t.start();
        int sock = tcpConnect(host, port, 2000);
        if (sock >= 0) {
            measurements.append(t.nsecsElapsed() / 1000); // μs
            r.successes++;
        }
        closeSocket(sock); // safe on -1 (no-op in wrapper)
        // Failure: timeout (>2000ms), refused, or network error.
        // All are "transient" — server was proven reachable above.
    }
    r.failRate = (double)(r.attempts - r.successes) / r.attempts;

    int m = measurements.size();
    if (m == 0) return r; // all 50 failed — rare but possible

    // ── Insufficient data: fallback to simple median ──────────────
    if (m < 5) {
        std::sort(measurements.begin(), measurements.end());
        r.latencyMs = (m % 2 == 1) ? measurements[m/2] / 1000.0
                     : (measurements[m/2-1] + measurements[m/2]) / 2000.0;
        r.usable = (m >= 2); // need at least 2 for a meaningful estimate
        return r;
    }

    // ── MAD: robust scale estimate ────────────────────────────────
    std::sort(measurements.begin(), measurements.end());
    double med = (m % 2 == 1) ? (double)measurements[m/2]
                 : (measurements[m/2-1] + measurements[m/2]) / 2.0;

    QVector<double> absDev(m);
    for (int i = 0; i < m; i++)
        absDev[i] = std::abs((double)measurements[i] - med);
    std::sort(absDev.begin(), absDev.end());
    double mad = (m % 2 == 1) ? absDev[m/2]
                 : (absDev[m/2-1] + absDev[m/2]) / 2.0;

    // ── MAD == 0: all measurements identical ──────────────────────
    if (mad == 0) {
        r.latencyMs = med / 1000.0;
        r.usable = true;
        sProbeCache.put(cacheKey, r.latencyMs);
        return r;
    }

    // ── Adaptive threshold filtering ──────────────────────────────
    const double k = 2.5;           // ~1.2% false-positive rate
    double thresh = k * 1.4826 * mad; // MAD → σ → threshold
    QVector<long long> clean;
    clean.reserve(m);
    for (auto x : measurements)
        if (std::abs((double)x - med) <= thresh)
            clean.append(x);

    // Guard: if MAD filter removed too many, fall back to median
    if (clean.size() < 5) {
        r.latencyMs = med / 1000.0;
        r.usable = true;
        sProbeCache.put(cacheKey, r.latencyMs);
        return r;
    }

    // ── Hodges-Lehmann — median of all pairwise averages ──────────
    int c = clean.size();
    int npairs = c * (c + 1) / 2; // includes self-pairs (i=j)
    QVector<long long> hl; hl.reserve(npairs);
    for (int i = 0; i < c; i++)
        for (int j = i; j < c; j++)
            hl.append((clean[i] + clean[j]) / 2); // integer division OK (μs)
    std::sort(hl.begin(), hl.end());
    double hlVal = (npairs % 2 == 1) ? (double)hl[npairs/2]
                   : (hl[npairs/2-1] + hl[npairs/2]) / 2.0;

    r.latencyMs = hlVal / 1000.0; // μs → ms
    r.usable = true;
    sProbeCache.put(cacheKey, r.latencyMs);
    return r;
}

// ── TCP ping with 50x averaging for sub-ms differentiation ──
// 5WHY: single tcpPingMs() can't differentiate servers with ≤1ms RTT
// (timer resolution limit).  Running 50 connects and averaging gives
// ~0.02ms effective resolution — enough to rank nearby servers.
// 5WHY: Simple average is sensitive to outliers — a single 50ms spike
// among 49 sub-ms measurements inflates the mean by ~1ms.  Now uses a
// 10% trimmed mean: discard the fastest and slowest 10% of measurements,
// average the middle 80%.  Eliminates outlier distortion at zero cost.
// 5WHY: First call verifies reachability; if single latency > 1ms, return
// directly (already differentiated).  If ≤ 1ms, run 49 more (50 total).
double tcpPingAvg(const QString& host, int port) {
    QString cacheKey = QStringLiteral("%1:%2").arg(host).arg(port);
    double cached = sProbeCache.get(cacheKey);
    if (cached >= 0) return cached;

    double result = -1.0;
    int first = tcpPingMs(host, port);
    // 5WHY: tcpPingMs returns elapsed ms truncated to int — a 950μs
    // connect returns 0 (not -1).  first < 0 means true failure (timeout
    // or SO_ERROR).  first == 0 means "success at <1ms" — those need the
    // 50x averaging even more than first==1 servers.
    if (first < 0) { result = -1.0; }
    else if (first > 1) { result = (double)first; }
    else {
        // Sub-ms region: run 50 total connects, trimmed mean of middle 80%.
        // 5WHY: The first connect (tcpPingMs above) returns int-ms resolution.
        // When sub-ms precision is needed, we re-measure all 50 connects with
        // nsecsElapsed() in microseconds.  The first measurement is discarded
        // — one wasted connect (~2ms) is negligible vs. the 50-connect loop.
        QVector<long long> measurements;
        measurements.reserve(50);
        for (int i = 0; i < 50; i++) {
            QElapsedTimer t; t.start();
            int sock = tcpConnect(host, port, 2000);
            if (sock >= 0)
                measurements.append(t.nsecsElapsed() / 1000); // microseconds
            closeSocket(sock);
        }
        int M = measurements.size();
        if (M >= 10) {
            // 10% trimmed mean: discard fastest and slowest 10%
            std::sort(measurements.begin(), measurements.end());
            int trim = M / 10; // 10% from each tail
            long long total = 0;
            for (int i = trim; i < M - trim; i++)
                total += measurements[i];
            result = (total / (double)(M - 2 * trim)) / 1000.0; // ms
        } else if (M > 0) {
            // Too few for trimming, use simple average
            long long total = 0;
            for (auto us : measurements) total += us;
            result = (total / (double)M) / 1000.0;
        }
    }

    sProbeCache.put(cacheKey, result);
    return result;
}

}
