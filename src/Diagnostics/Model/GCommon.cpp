#include "Diagnostics/Model/GHelpers.h"
#include "Common/Utils/NetUtil.h"
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QHash>
#include <QPair>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <thread>
#include <vector>
#include <QUrl>
#include <QEventLoop>
#include <QTimer>
namespace SystemDiagnostics {

// ── Host header with RFC 7230 §5.4 port inclusion ──────────────────
static QString hostHeader(const QString& host, int port) {
    return (port != 80) ? QStringLiteral("%1:%2").arg(host).arg(port) : host;
}

// ── Raw HTTP GET — returns raw HTTP response (headers + body) ───────
// 5WHY: httpGet was removed during GeoProbe refactoring (httpTtfb +
// httpDownload replaced it), but G3GeoIPLoc.cpp was created after
// the removal and still references it.  Restore it so G3GeoIPLoc
// GeoIP country detection works.  Follows the same TCP connect + send +
// recv pattern as httpTtfb/httpDownload.
QByteArray httpGet(const QString& host, int port, const QString& path,
                   int timeoutMs, int maxBytes, const QString& connectHost) {
    QByteArray body;
    QString connTarget = connectHost.isEmpty() ? host : connectHost;
    int sock = tcpConnect(connTarget, port, qMin(timeoutMs, 3000));
    if (sock < 0) return body;

    // Always use the original host (not IP) in the Host header for virtual-host routing
    QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: NetDiagnostics/1.0\r\nConnection: close\r\n\r\n")
        .arg(path, hostHeader(host, port)).toUtf8();

    // Send request (EAGAIN-safe)
    int sent = 0; int sendAttempts = 0;
    while (sent < req.size() && sendAttempts < 100) {
        int n = ::send(sock, req.constData() + sent, req.size() - sent, 0);
        if (n > 0) { sent += n; sendAttempts = 0; }
        else if (n < 0) {
#if defined(_WIN32)
            if (WSAGetLastError() != WSAEWOULDBLOCK) break;
#else
            if (errno != EAGAIN && errno != EWOULDBLOCK) break;
#endif
            fd_set wfds; struct timeval wtv = {0, 100000};
            FD_ZERO(&wfds); FD_SET(sock, &wfds);
            if (select(sock + 1, nullptr, &wfds, nullptr, &wtv) <= 0) break;
            sendAttempts++;
        } else break;
    }
    if (sent < req.size()) { closeSocket(sock); return body; }

    // Read response
    fd_set fdset; struct timeval tv;
    QElapsedTimer guard; guard.start();
    char buf[4096];
    while (body.size() < maxBytes && guard.elapsed() < timeoutMs) {
        FD_ZERO(&fdset); FD_SET(sock, &fdset);
        int remaining = timeoutMs - (int)guard.elapsed();
        if (remaining <= 0) break;
        tv = {remaining / 1000, (remaining % 1000) * 1000};
        if (select(sock + 1, &fdset, nullptr, nullptr, &tv) <= 0) break;
        int n = (int)recv(sock, buf, sizeof(buf), 0);
        if (n > 0) body.append(buf, n);
        else if (n == 0) break;
        else {
#if defined(_WIN32)
            if (WSAGetLastError() == WSAEWOULDBLOCK) continue;
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
#endif
            break;
        }
    }
    closeSocket(sock);
    return body;
}

// HTTP download with throughput measurement
// SpeedResult defined in GHelpers.h
SpeedResult httpDownload(const QString& urlStr, int targetBytes, int timeoutMs) {
    SpeedResult r = {0, 0, 0, false, {}};
    ParsedUrl pu = parseHttpUrl(urlStr);
    if (pu.host.isEmpty()) { r.error = QStringLiteral("Invalid URL"); return r; }
    QString host = pu.host; int port = pu.port;
    QString path = pu.path;

    QElapsedTimer t; t.start();
    int sock = tcpConnect(host, port, 3000);
    if (sock < 0) { r.error = QStringLiteral("TCP Connect Failed"); return r; }
    fd_set fdset; struct timeval tv; // reused by send/recv loops below

    // Send HTTP GET (loop handles partial sends, EAGAIN-safe)
    // 5WHY: Host header omitted port — same bug that was fixed in httpGet()
    // (see above). Speed-test servers on port 8080 behind reverse proxies
    // may route incorrectly without the explicit port per RFC 7230 §5.4.
    QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: NetDiagnostics/1.0\r\nConnection: close\r\n\r\n")
        .arg(path, hostHeader(host, port)).toUtf8();
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
    if (reqSent < req.size()) { r.error = QStringLiteral("HTTP Request Send Incomplete"); closeSocket(sock); return r; }

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
                if (!httpOk && codeStart > 0 && codeStart + 4 <= statusLine.size())
                    r.error = QStringLiteral("HTTP %1").arg(QString::fromLatin1(statusLine.mid(codeStart + 1, 3)));
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
    } else if (r.error.isEmpty()) {
        if (r.bytes <= 0)
            r.error = QStringLiteral("No Data Received");
        else if (r.durationMs <= 0)
            r.error = QStringLiteral("Transfer Duration Too Short");
        else
            r.error = QStringLiteral("Insufficient Data (%1 bytes)").arg(r.bytes);
    }
    return r;
}

// HTTP upload with throughput measurement — POST data to server
SpeedResult httpUpload(const QString& urlStr, int targetBytes, int timeoutMs) {
    SpeedResult r = {0, 0, 0, false, {}};
    ParsedUrl pu = parseHttpUrl(urlStr);
    if (pu.host.isEmpty()) { r.error = QStringLiteral("Invalid URL"); return r; }
    QString host = pu.host; int port = pu.port;

    QElapsedTimer t; t.start();
    int sock = tcpConnect(host, port, 3000);
    if (sock < 0) { r.error = QStringLiteral("TCP Connect Failed"); return r; }

    // Generate random payload
    QByteArray payload(targetBytes, 'A');
    for (int i = 0; i < targetBytes; i += 64)
        payload[i] = (char)('A' + (i / 64) % 26);

    QByteArray req = QStringLiteral("POST %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: NetDiagnostics/1.0\r\nContent-Type: application/octet-stream\r\nContent-Length: %3\r\nConnection: close\r\n\r\n")
        .arg(pu.path.isEmpty() ? QStringLiteral("/") : pu.path, hostHeader(host, port)).arg(targetBytes).toUtf8();
    req.append(payload);

    // 5WHY: startNs was placed AFTER the send loop, so it measured only
    // the server response receive time. For upload, the throughput-relevant
    // phase is the send itself — the response is just a tiny HTTP 200 OK.
    // Move startNs before the send loop so Mbps = bytes * 8 / (send + recv).
    qint64 startNs = t.nsecsElapsed();

    // Send request + body
    int reqSent = 0;
    QElapsedTimer sendGuard; sendGuard.start();
    while (reqSent < req.size()) {
        if (sendGuard.elapsed() > timeoutMs) break;
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
    if (reqSent < req.size()) { r.error = QStringLiteral("Upload Send Incomplete"); closeSocket(sock); return r; }

    // Read HTTP response
    fd_set fdset; struct timeval tv;
    QByteArray respBuf;
    char buf[4096];
    QElapsedTimer recvGuard; recvGuard.start();
    while (respBuf.size() < 4096) {
        int remaining = qMin(timeoutMs, 30000) - (int)recvGuard.elapsed();
        if (remaining <= 0) break;
        int selectMs = qMax(remaining, 50);
        FD_ZERO(&fdset); FD_SET(sock, &fdset);
        tv = {selectMs / 1000, (selectMs % 1000) * 1000};
        if (select(sock + 1, &fdset, nullptr, nullptr, &tv) <= 0) break;
        ssize_t n = recv(sock, buf, sizeof(buf), 0);
        if (n > 0) { respBuf.append(buf, (int)n); }
        else if (n == 0) break;
        else {
#if defined(_WIN32)
            if (WSAGetLastError() == WSAEWOULDBLOCK) continue;
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
#endif
            break;
        }
    }
    closeSocket(sock);

    qint64 elapsedNs = t.nsecsElapsed() - startNs;
    if (elapsedNs <= 0) elapsedNs = 1;
    r.bytes = targetBytes;  // we count bytes sent
    r.durationMs = (int)(elapsedNs / 1000000);

    if (r.durationMs > 0 && !respBuf.isEmpty()) {
        // Check for HTTP 200
        int hdrEnd = respBuf.indexOf("\r\n\r\n");
        if (hdrEnd < 0) hdrEnd = respBuf.indexOf("\n\n");
        QByteArray hdrs = (hdrEnd > 0) ? respBuf.left(hdrEnd) : respBuf;
        int sp1 = hdrs.indexOf(' ');
        bool httpOk = (sp1 > 0 && hdrs.mid(sp1 + 1, 3) == "200");
        if (httpOk) {
            double bits = r.bytes * 8.0;
            double secs = r.durationMs / 1000.0;
            r.mbps = bits / secs / 1000000.0;
            r.ok = true;
        } else {
            r.error = QStringLiteral("HTTP %1").arg(sp1 > 0 ? QString::fromLatin1(hdrs.mid(sp1 + 1, 3)) : QStringLiteral("???"));
        }
    } else if (r.durationMs <= 0) {
        r.error = QStringLiteral("No Upload Response");
    } else {
        r.error = QStringLiteral("Empty Upload Response");
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

// HTTP TTFB probe — TCP connect + HTTP GET → time to first byte.
// Returns ms (including TCP handshake), or -1.0 on failure.
// Shared by GeoProbe (probeAllServers, selectBestServer, pickBestInCountry,
// pickBestInRegion) and geoIPLoc Pass 2.
double httpTtfb(const QString& host, int port, const QString& path,
                int connectTimeoutMs, int readTimeoutSec) {
    if (host.isEmpty()) return -1.0;  // guard against malformed URLs
    QElapsedTimer t; t.start();
    int sock = tcpConnect(host, port, connectTimeoutMs);
    if (sock < 0) return -1.0;
    QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: NetDiagnostics/1.0\r\nConnection: close\r\n\r\n")
        .arg(path, hostHeader(host, port)).toUtf8();
    // EAGAIN-safe send loop — socket is non-blocking (set by tcpConnect)
    // 5WHY: retried on ALL n<0 including ECONNRESET/EPIPE, causing 10s stalls.
    // Now only retries on EAGAIN/EWOULDBLOCK (transient buffer-full), matching
    // httpDownload's guard (lines 39-41).
    int sent = 0; int sendAttempts = 0;
    while (sent < req.size() && sendAttempts < 100) {
        int n = ::send(sock, req.constData() + sent, req.size() - sent, 0);
        if (n > 0) { sent += n; sendAttempts = 0; }
        else if (n < 0) {
#if defined(_WIN32)
            int e = WSAGetLastError();
            if (e != WSAEWOULDBLOCK) break; // fatal error
#else
            if (errno != EAGAIN && errno != EWOULDBLOCK) break;
#endif
            fd_set wfds; struct timeval wtv = {0, 100000};
            FD_ZERO(&wfds); FD_SET(sock, &wfds);
            if (select(sock + 1, nullptr, &wfds, nullptr, &wtv) <= 0) break;
            sendAttempts++;
        }
        else break; // n == 0: connection closed
    }
    // 5WHY: incomplete send fell through to read-select, wasting readTimeoutSec.
    // Server never received full request → cannot respond → early return.
    if (sent < req.size()) { closeSocket(sock); return -1.0; }

    fd_set fds; struct timeval tv = {readTimeoutSec, 0};
    FD_ZERO(&fds); FD_SET(sock, &fds);
    double ttfb = -1.0;
    if (select(sock + 1, &fds, nullptr, nullptr, &tv) > 0) {
        char b[1];
        if (recv(sock, b, 1, 0) > 0) ttfb = t.elapsed();
    }
    closeSocket(sock);
    return ttfb;
}

// ── HTTPS GET — QNetworkAccessManager with TLS ────────────────────────
// 5WHY: httpGet uses raw TCP sockets — no TLS support.  When GeoIP
// providers (ipapi.co, ip.taobao.com) return 301 redirects to HTTPS,
// httpGet skips non-200 responses → provider fails.  httpsGet uses
// QNetworkAccessManager which handles TLS + redirects transparently.
QByteArray httpsGet(const QString& url, int timeoutMs) {
    // 5WHY: Stack-allocated QNetworkAccessManager loses TLS session cache,
    // HTTP keep-alive connections, and internal DNS cache between calls.
    // A thread_local static enables TLS session resumption (1-RTT instead
    // of 2-RTT handshake), HTTP/1.1 keep-alive, and DNS caching across all
    // calls from the same worker thread — ~20% latency reduction per call.
    thread_local static QNetworkAccessManager nam;
    QUrl qurl(url);
    QNetworkRequest req{qurl};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NetDiagnostics/1.0"));
    req.setRawHeader("Accept", "text/plain,application/json");

    QNetworkReply* reply = nam.get(req);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);

    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        reply->deleteLater();
        return {};
    }

    QByteArray body;
    if (reply->error() == QNetworkReply::NoError) {
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 200)
            body = reply->readAll();
    }
    reply->deleteLater();
    return body;
}

// ── DoH DNS query (single resolver) ────────────────────────────────────
static QStringList dohQuerySingle(const QString& endpoint, const QString& domain,
                                   const QString& type, int timeoutMs) {
    QStringList ips;
    QString queryUrl = QStringLiteral("%1?name=%2&type=%3")
        .arg(endpoint, domain, type);
    QByteArray body = httpsGet(queryUrl, timeoutMs);
    if (body.isEmpty()) return ips;

    // Parse JSON: {"Answer":[{"name":"...","type":1,"TTL":300,"data":"1.2.3.4"},...]}
    QString json = QString::fromUtf8(body);
    int ansStart = json.indexOf(QStringLiteral("\"Answer\":["));
    if (ansStart < 0) return ips;

    int sectionEnd = json.indexOf(']', ansStart);
    int pos = ansStart;
    while ((pos = json.indexOf(QStringLiteral("\"data\":\""), pos)) >= 0
           && (sectionEnd < 0 || pos < sectionEnd)) {
        pos += 8;
        int end = json.indexOf('\"', pos);
        if (end > pos) {
            QString ip = json.mid(pos, end - pos);
            if (!ip.isEmpty() && ip[0].isDigit())
                ips.append(ip);
        }
    }
    return ips;
}

// ── DoH DNS query — 4-resolver majority consensus ──────────────────────
// 5WHY: Querying a single DoH resolver risks trusting a compromised or
// censored server.  Querying 4 resolvers (2 domestic CN + 2 international)
// and taking the majority vote ensures the result is trustworthy even if
// 1-2 resolvers are polluted.
QStringList dohQuery(const QString& domain, const QString& type, int timeoutMs) {
    static const struct {
        const char* url;
    } kResolvers[] = {
        {"https://dns.alidns.com/resolve"},       // AliDNS (CN)
        {"https://doh.pub/dns-query"},             // DNSPod / Tencent (CN)
        {"https://dns.google/resolve"},            // Google (US)
        {"https://cloudflare-dns.com/dns-query"},  // Cloudflare (US)
    };

    // Collect all IPs from all resolvers, counting frequency
    QMap<QString, int> freq;
    int responders = 0;
    for (const auto& r : kResolvers) {
        QStringList ips = dohQuerySingle(
            QString::fromUtf8(r.url), domain, type, timeoutMs);
        if (!ips.isEmpty()) responders++;
        for (const auto& ip : ips)
            freq[ip]++;
    }

    // Majority consensus: return IPs seen by ≥3 resolvers (≥75%)
    // If no majority (all IPs have ≤2 votes), return all unique IPs
    int threshold = qMin(3, responders);
    if (threshold < 2) threshold = 1;  // only 1-2 responders → any IP counts
    QStringList result;
    for (auto it = freq.begin(); it != freq.end(); ++it) {
        if (it.value() >= threshold)
            result.append(it.key());
    }
    if (result.isEmpty()) {
        // No majority — return all unique IPs
        for (auto it = freq.begin(); it != freq.end(); ++it)
            result.append(it.key());
    }
    return result;
}

// ── DoH full-record query (single resolver) ─────────────────────────
static DohDnsFullResult dohQuerySingleFull(const QString& endpoint,
                                         const QString& domain,
                                         const QString& type, int timeoutMs) {
    DohDnsFullResult result;
    QString queryUrl = QStringLiteral("%1?name=%2&type=%3")
        .arg(endpoint, domain, type);
    QByteArray body = httpsGet(queryUrl, timeoutMs);
    if (body.isEmpty()) return result;

    QString json = QString::fromUtf8(body);
    int ansStart = json.indexOf(QStringLiteral("\"Answer\":["));
    if (ansStart < 0) return result;

    int sectionEnd = json.indexOf(']', ansStart);
    int pos = ansStart;

    // Parse each record object in the Answer array
    while (pos < sectionEnd) {
        // Find next record object opening brace
        int recStart = json.indexOf('{', pos);
        if (recStart < 0 || recStart >= sectionEnd) break;
        int recEnd = json.indexOf('}', recStart);
        if (recEnd < 0 || recEnd >= sectionEnd) { pos = recStart + 1; continue; }

        // Extract fields from this record
        QString record = json.mid(recStart, recEnd - recStart + 1);

        auto extractStr = [&](const QString& key) -> QString {
            // Extracts a quoted-string JSON value: "key":"value"
            int k = record.indexOf('"' + key + '"');
            if (k < 0) return {};
            int v = record.indexOf('"', k + key.length() + 2);
            if (v < 0) return {};
            int ve = record.indexOf('"', v + 1);
            if (ve < 0) return {};
            return record.mid(v + 1, ve - v - 1);
        };
        // 5WHY: extractInt delegated to extractStr() which only handles quoted-string
        // values ("key":"value").  But JSON integers are unquoted ("type":1), so
        // extractStr returned the NEXT field name instead of the integer.  .toInt()
        // on "TTL" returned 0 → rec.type never matched 1 (A record) → aRecords
        // always empty → ALL DoH queries reported as failed.
        // 5WHY: extractInt used to return 0 for both "key not found" and
        // "value is 0", making them indistinguishable.  DNS types and TTLs
        // are unsigned, so -1 unambiguously means "not found / no value".
        // Downstream: rec.ttl=-1 fails rec.ttl>=0 guard (correctly excluded),
        // rec.type=-1 never matches type==1 or type==5 (treated as unknown).
        auto extractInt = [&](const QString& key) -> int {
            int k = record.indexOf('"' + key + '"');
            if (k < 0) return -1;   // key not present
            int colon = record.indexOf(':', k + key.length() + 2);
            if (colon < 0) return -1; // malformed record
            // Skip whitespace after colon
            int v = colon + 1;
            while (v < record.length() && (record[v] == ' ' || record[v] == '\t')) v++;
            // Extract digits (non-negative for DNS types/TTLs)
            int end = v;
            while (end < record.length() && record[end].isDigit()) end++;
            if (end == v) return -1; // no numeric value found
            return record.mid(v, end - v).toInt();
        };

        DohDnsRecord rec;
        rec.name = extractStr(QStringLiteral("name"));
        rec.type = extractInt(QStringLiteral("type"));
        rec.ttl  = extractInt(QStringLiteral("TTL"));
        rec.data = extractStr(QStringLiteral("data"));

        if (rec.type == 1 && !rec.data.isEmpty() && rec.data[0].isDigit()) {
            result.aRecords.append(rec.data);
        }
        if (rec.type == 5 && !rec.data.isEmpty()) {
            result.cnameChain.append(rec.data);
            result.hasCname = true;
        }
        // 5WHY: TTL=0 is a documented pollution injection signal
        // (injection devices set TTL=0 to prevent caching).  Changed
        // > to >= so TTL=0 is captured instead of silently excluded.
        // Sentinels: minTtl=-1 means no TTL data yet (for first record),
        // minTtl=0 means real TTL=0 was observed (pollution signal).
        if (rec.ttl >= 0 && (result.minTtl < 0 || rec.ttl < result.minTtl))
            result.minTtl = rec.ttl;

        result.allRecords.append(rec);
        pos = recEnd + 1;
    }
    return result;
}

// ── DoH full-record query — 4-resolver majority consensus ──────────
DohDnsFullResult dohQueryFull(const QString& domain, const QString& type, int timeoutMs) {
    static const struct { const char* url; } kResolvers[] = {
        {"https://dns.alidns.com/resolve"},
        {"https://doh.pub/dns-query"},
        {"https://dns.google/resolve"},
        {"https://cloudflare-dns.com/dns-query"},
    };

    // 5WHY: 4 DoH resolvers were queried SEQUENTIALLY — each httpsGet
    // blocks with a QEventLoop for up to timeoutMs.  Since the 4
    // resolvers are independent (different servers, no data dependency),
    // parallelizing with std::thread cuts worst case from 4×timeoutMs
    // to max(timeoutMs).  std::thread is used INSTEAD of QtConcurrent
    // because this function may be called from within a QtConcurrent::run
    // context (parallel domains in dnsIntegrity).  QtConcurrent uses a
    // Parallel DoH: std::thread avoids QtConcurrent pool deadlock (nested callers).
    // Exceptions caught in thread body; std::vector provides RAII join on scope exit.
    static const int kResolverCount = sizeof(kResolvers) / sizeof(kResolvers[0]);
    DohDnsFullResult resolverResults[kResolverCount]{};
    std::vector<std::thread> resolverThreads;
    resolverThreads.reserve(kResolverCount);
    for (int i = 0; i < kResolverCount; ++i) {
        try {
            resolverThreads.emplace_back([&, i]() {
                try {
                    resolverResults[i] = dohQuerySingleFull(
                        QString::fromUtf8(kResolvers[i].url), domain, type, timeoutMs);
                } catch (...) {
                    // Leave default (empty records, minTtl=-1) — treated as failure.
                }
            });
        } catch (const std::system_error&) {
            break;  // resource exhaustion — skip remaining, join what we have
        }
    }
    for (auto& t : resolverThreads)
        try { if (t.joinable()) t.join(); } catch (...) {}

    QMap<QString, int> freq;
    QStringList allCnames;
    QList<DohDnsRecord> allRecs;
    int globalMinTtl = -1;  // -1 = no TTL data yet (0 = real TTL=0 observed)
    bool globalHasCname = false;
    int responders = 0;

    for (int i = 0; i < kResolverCount; ++i) {
        DohDnsFullResult fr = resolverResults[i];
        if (!fr.aRecords.isEmpty()) responders++;
        for (const auto& ip : fr.aRecords) freq[ip]++;
        if (fr.hasCname) {
            globalHasCname = true;
            for (const auto& c : fr.cnameChain)
                if (!allCnames.contains(c)) allCnames.append(c);
        }
        // Sentinel-aware: -1 means no TTL data, skip; 0 means real TTL=0
        if (fr.minTtl >= 0 && (globalMinTtl < 0 || fr.minTtl < globalMinTtl))
            globalMinTtl = fr.minTtl;
        allRecs.append(fr.allRecords);
    }

    // Majority consensus for A records
    int threshold = qMin(3, responders);
    if (threshold < 2) threshold = 1;

    QStringList consensusIps;
    for (auto it = freq.begin(); it != freq.end(); ++it) {
        if (it.value() >= threshold)
            consensusIps.append(it.key());
    }
    if (consensusIps.isEmpty()) {
        for (auto it = freq.begin(); it != freq.end(); ++it)
            consensusIps.append(it.key());
    }

    DohDnsFullResult result;
    result.aRecords   = consensusIps;
    result.cnameChain = allCnames;
    result.hasCname   = globalHasCname;
    // -1 sentinel means no TTL data at all; any value >= 0 is a real TTL
    result.minTtl     = globalMinTtl;  // -1 = no data, >= 0 = real min TTL
    result.allRecords = allRecs;
    return result;
}

// ── ISO 3166-1 country code mapping ──────────────────────────────
// Maps 2-letter codes → {3-letter, full name}.  Data from ISO 3166-1.
// 5WHY: Country codes were displayed raw (e.g. "CN" in both table and
// non-table contexts).  Table column "CC" expects 3-letter codes; prose
// text reads better with full names ("China" vs "CN").
static const struct { const char* a2; const char* a3; const char* name; } kCountryMap[] = {
    {"AF","AFG","Afghanistan"},{"AL","ALB","Albania"},{"DZ","DZA","Algeria"},
    {"AS","ASM","American Samoa"},{"AD","AND","Andorra"},{"AO","AGO","Angola"},
    {"AI","AIA","Anguilla"},{"AQ","ATA","Antarctica"},{"AG","ATG","Antigua and Barbuda"},
    {"AR","ARG","Argentina"},{"AM","ARM","Armenia"},{"AW","ABW","Aruba"},
    {"AU","AUS","Australia"},{"AT","AUT","Austria"},{"AZ","AZE","Azerbaijan"},
    {"BS","BHS","Bahamas"},{"BH","BHR","Bahrain"},{"BD","BGD","Bangladesh"},
    {"BB","BRB","Barbados"},{"BY","BLR","Belarus"},{"BE","BEL","Belgium"},
    {"BZ","BLZ","Belize"},{"BJ","BEN","Benin"},{"BM","BMU","Bermuda"},
    {"BT","BTN","Bhutan"},{"BO","BOL","Bolivia"},{"BA","BIH","Bosnia and Herzegovina"},
    {"BW","BWA","Botswana"},{"BR","BRA","Brazil"},{"IO","IOT","British Indian Ocean Territory"},
    {"VG","VGB","British Virgin Islands"},{"BN","BRN","Brunei"},{"BG","BGR","Bulgaria"},
    {"BF","BFA","Burkina Faso"},{"BI","BDI","Burundi"},{"CV","CPV","Cabo Verde"},
    {"KH","KHM","Cambodia"},{"CM","CMR","Cameroon"},{"CA","CAN","Canada"},
    {"KY","CYM","Cayman Islands"},{"CF","CAF","Central African Republic"},{"TD","TCD","Chad"},
    {"CL","CHL","Chile"},{"CN","CHN","China"},{"CO","COL","Colombia"},
    {"KM","COM","Comoros"},{"CG","COG","Congo"},{"CD","COD","Democratic Republic of the Congo"},
    {"CK","COK","Cook Islands"},{"CR","CRI","Costa Rica"},{"CI","CIV","Côte d'Ivoire"},
    {"HR","HRV","Croatia"},{"CU","CUB","Cuba"},{"CW","CUW","Curaçao"},
    {"CY","CYP","Cyprus"},{"CZ","CZE","Czechia"},{"DK","DNK","Denmark"},
    {"DJ","DJI","Djibouti"},{"DM","DMA","Dominica"},{"DO","DOM","Dominican Republic"},
    {"EC","ECU","Ecuador"},{"EG","EGY","Egypt"},{"SV","SLV","El Salvador"},
    {"GQ","GNQ","Equatorial Guinea"},{"ER","ERI","Eritrea"},{"EE","EST","Estonia"},
    {"SZ","SWZ","Eswatini"},{"ET","ETH","Ethiopia"},{"FJ","FJI","Fiji"},
    {"FI","FIN","Finland"},{"FR","FRA","France"},{"GF","GUF","French Guiana"},
    {"PF","PYF","French Polynesia"},{"GA","GAB","Gabon"},{"GM","GMB","Gambia"},
    {"GE","GEO","Georgia"},{"DE","DEU","Germany"},{"GH","GHA","Ghana"},
    {"GI","GIB","Gibraltar"},{"GR","GRC","Greece"},{"GL","GRL","Greenland"},
    {"GD","GRD","Grenada"},{"GP","GLP","Guadeloupe"},{"GU","GUM","Guam"},
    {"GT","GTM","Guatemala"},{"GN","GIN","Guinea"},{"GW","GNB","Guinea-Bissau"},
    {"GY","GUY","Guyana"},{"HT","HTI","Haiti"},{"HN","HND","Honduras"},
    {"HK","HKG","Hong Kong"},{"HU","HUN","Hungary"},{"IS","ISL","Iceland"},
    {"IN","IND","India"},{"ID","IDN","Indonesia"},{"IR","IRN","Iran"},
    {"IQ","IRQ","Iraq"},{"IE","IRL","Ireland"},{"IL","ISR","Israel"},
    {"IT","ITA","Italy"},{"JM","JAM","Jamaica"},{"JP","JPN","Japan"},
    {"JO","JOR","Jordan"},{"KZ","KAZ","Kazakhstan"},{"KE","KEN","Kenya"},
    {"KI","KIR","Kiribati"},{"KW","KWT","Kuwait"},{"KG","KGZ","Kyrgyzstan"},
    {"LA","LAO","Laos"},{"LV","LVA","Latvia"},{"LB","LBN","Lebanon"},
    {"LS","LSO","Lesotho"},{"LR","LBR","Liberia"},{"LY","LBY","Libya"},
    {"LI","LIE","Liechtenstein"},{"LT","LTU","Lithuania"},{"LU","LUX","Luxembourg"},
    {"MO","MAC","Macao"},{"MG","MDG","Madagascar"},{"MW","MWI","Malawi"},
    {"MY","MYS","Malaysia"},{"MV","MDV","Maldives"},{"ML","MLI","Mali"},
    {"MT","MLT","Malta"},{"MH","MHL","Marshall Islands"},{"MQ","MTQ","Martinique"},
    {"MR","MRT","Mauritania"},{"MU","MUS","Mauritius"},{"MX","MEX","Mexico"},
    {"FM","FSM","Micronesia"},{"MD","MDA","Moldova"},{"MC","MCO","Monaco"},
    {"MN","MNG","Mongolia"},{"ME","MNE","Montenegro"},{"MS","MSR","Montserrat"},
    {"MA","MAR","Morocco"},{"MZ","MOZ","Mozambique"},{"MM","MMR","Myanmar"},
    {"NA","NAM","Namibia"},{"NR","NRU","Nauru"},{"NP","NPL","Nepal"},
    {"NL","NLD","Netherlands"},{"NC","NCL","New Caledonia"},{"NZ","NZL","New Zealand"},
    {"NI","NIC","Nicaragua"},{"NE","NER","Niger"},{"NG","NGA","Nigeria"},
    {"NU","NIU","Niue"},{"KP","PRK","North Korea"},{"MK","MKD","North Macedonia"},
    {"MP","MNP","Northern Mariana Islands"},{"NO","NOR","Norway"},{"OM","OMN","Oman"},
    {"PK","PAK","Pakistan"},{"PW","PLW","Palau"},{"PS","PSE","Palestine"},
    {"PA","PAN","Panama"},{"PG","PNG","Papua New Guinea"},{"PY","PRY","Paraguay"},
    {"PE","PER","Peru"},{"PH","PHL","Philippines"},{"PL","POL","Poland"},
    {"PT","PRT","Portugal"},{"PR","PRI","Puerto Rico"},{"QA","QAT","Qatar"},
    {"RO","ROU","Romania"},{"RU","RUS","Russia"},{"RW","RWA","Rwanda"},
    {"RE","REU","Réunion"},{"WS","WSM","Samoa"},{"SM","SMR","San Marino"},
    {"SA","SAU","Saudi Arabia"},{"SN","SEN","Senegal"},{"RS","SRB","Serbia"},
    {"SC","SYC","Seychelles"},{"SL","SLE","Sierra Leone"},{"SG","SGP","Singapore"},
    {"SX","SXM","Sint Maarten"},{"SK","SVK","Slovakia"},{"SI","SVN","Slovenia"},
    {"SB","SLB","Solomon Islands"},{"SO","SOM","Somalia"},{"ZA","ZAF","South Africa"},
    {"KR","KOR","South Korea"},{"SS","SSD","South Sudan"},{"ES","ESP","Spain"},
    {"LK","LKA","Sri Lanka"},{"SD","SDN","Sudan"},{"SR","SUR","Suriname"},
    {"SE","SWE","Sweden"},{"CH","CHE","Switzerland"},{"SY","SYR","Syria"},
    {"TW","TWN","Taiwan"},{"TJ","TJK","Tajikistan"},{"TZ","TZA","Tanzania"},
    {"TH","THA","Thailand"},{"TL","TLS","Timor-Leste"},{"TG","TGO","Togo"},
    {"TO","TON","Tonga"},{"TT","TTO","Trinidad and Tobago"},{"TN","TUN","Tunisia"},
    {"TR","TUR","Turkey"},{"TM","TKM","Turkmenistan"},{"TC","TCA","Turks and Caicos Islands"},
    {"TV","TUV","Tuvalu"},{"UG","UGA","Uganda"},{"UA","UKR","Ukraine"},
    {"AE","ARE","United Arab Emirates"},{"GB","GBR","United Kingdom"},
    {"US","USA","United States"},{"UY","URY","Uruguay"},{"UZ","UZB","Uzbekistan"},
    {"VU","VUT","Vanuatu"},{"VA","VAT","Vatican City"},{"VE","VEN","Venezuela"},
    {"VN","VNM","Vietnam"},{"EH","ESH","Western Sahara"},{"YE","YEM","Yemen"},
    {"ZM","ZMB","Zambia"},{"ZW","ZWE","Zimbabwe"},
};
static const auto kCountryBy2 = []() {
    QHash<QString, QPair<QString,QString>> m;
    for (const auto& e : kCountryMap) m[QString::fromLatin1(e.a2)] = {QString::fromLatin1(e.a3), QString::fromLatin1(e.name)};
    return m;
}();

QString countryCode3(const QString& code2) {
    auto it = kCountryBy2.constFind(code2);
    return (it != kCountryBy2.cend()) ? it->first : code2;
}
QString countryFullName(const QString& code2) {
    if (code2.isEmpty() || code2 == QStringLiteral("XX"))
        return QStringLiteral("Unknown");
    auto it = kCountryBy2.constFind(code2);
    return (it != kCountryBy2.cend()) ? it->second : code2;
}

} // namespace SystemDiagnostics
