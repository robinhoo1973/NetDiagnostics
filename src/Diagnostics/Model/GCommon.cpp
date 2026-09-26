#include "Diagnostics/Model/GHelpers.h"
#include "Common/Utils/NetUtil.h"
#include <QHash>
#include <QPair>
#include <QUrl>   // httpDownloadCurl/httpUploadCurl 主机校验
#if !defined(NO_CURL)
#include <curl/curl.h>   // 5WHY (2026-09-26 铁律): 测速 HTTP 桌面路径走 curl easy API
#endif
namespace SystemDiagnostics {

// ── Socket 兜底专用辅助（5WHY 2026-09-26 条件编译）──────────────────
// 仅 NO_CURL 平台的 socket 实现使用——桌面 curl 构建不再编译 ~60 行死代码
// （iOS CI 以 NO_CURL 编译提供覆盖）。
#if defined(NO_CURL)

// ── Host header with RFC 7230 §5.4 port inclusion ──────────────────
static QString hostHeader(const QString& host, int port) {
    return (port != 80) ? QStringLiteral("%1:%2").arg(host).arg(port) : host;
}

// ── Shared EAGAIN-safe socket helpers ────────────────────────────────
// 5WHY: httpDownload/httpUpload/httpTtfb each hand-rolled the same
// non-blocking send loop (3 different timeout schemes: hardcoded 30s /
// caller timeoutMs / 100-attempt×100ms) and the same select+recv loop
// with duplicated Win32-vs-POSIX errno branches.  They had already
// drifted (e.g. httpDownload's 30s send guard vs httpUpload's timeoutMs).
// Extracted once so every EAGAIN/timeout fix applies everywhere.

// Send the whole buffer, retrying on EAGAIN/EWOULDBLOCK until the
// wall-clock guard expires.  Returns true iff all bytes were sent.
static bool sendAll(int sock, const QByteArray& data, int timeoutMs) {
    int sent = 0;
    QElapsedTimer guard; guard.start();
    while (sent < data.size()) {
        if (guard.elapsed() > timeoutMs) return false;
        auto n = ::send(sock, data.constData() + sent, data.size() - sent, 0);
        if (n > 0) { sent += n; continue; }
        if (n == 0) return false;  // connection closed
#if defined(_WIN32)
        if (WSAGetLastError() != WSAEWOULDBLOCK) return false;  // fatal error
#else
        if (errno != EAGAIN && errno != EWOULDBLOCK) return false;
#endif
        fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf);
        struct timeval wfTv = {0, 100000};  // 100ms select wait
        if (select(sock + 1, nullptr, &wf, nullptr, &wfTv) <= 0) return false;
    }
    return true;
}

// Wait until the socket is readable or the budget expires.
// Returns true iff readable.
static bool waitReadable(int sock, int budgetMs) {
    fd_set fdset; struct timeval tv;
    int selectMs = qMax(budgetMs, 50);
    FD_ZERO(&fdset); FD_SET(sock, &fdset);
    tv = {selectMs / 1000, (selectMs % 1000) * 1000};
    return select(sock + 1, &fdset, nullptr, nullptr, &tv) > 0;
}

// Read one chunk.  Returns bytes read (>0), 0 on EOF/real error,
// -1 on EAGAIN/EWOULDBLOCK (caller should retry after waitReadable).
static ssize_t recvChunk(int sock, char* buf, int bufSize) {
    ssize_t n = recv(sock, buf, bufSize, 0);
    if (n < 0) {
#if defined(_WIN32)
        if (WSAGetLastError() == WSAEWOULDBLOCK) return -1;
#else
        if (errno == EAGAIN || errno == EWOULDBLOCK) return -1;
#endif
        return 0;
    }
    return n;
}
#endif // NO_CURL


// ── curl 实现（桌面, !NO_CURL）───────────────────────────────────────────
// 5WHY (2026-09-26 铁律禁造轮子): 手写 socket HTTP（select+recv EAGAIN 循环、
// ">1KB 即算成功"启发式把非 HTTP 字节当有效下载、4 轮状态码提取 5WHY）由
// libcurl 取代——成熟解析/chunked/302 处理/超时。NO_CURL（iOS/Android/无库
// 桌面）保留原 socket 实现作兜底（GeoProbe 测速在移动端同样运行）。
// ── 测速共享助手（5WHY 2026-09-26 双份收敛）──────────────────────────
// 终态尾（usable 判定 + 50% 完成度门 + Mbps 换算 + 错误映射）与确定性载荷
// 曾在 curl/socket 变体各写一份（"与原实现同门"注释靠纪律同步）。单一来源，
// 四个变体共享；ok/error 互斥（usable 成立不再携带错误串）。
namespace {
QByteArray speedPayload(int targetBytes) {
    QByteArray payload(targetBytes, 'A');
    for (int i = 0; i < targetBytes; i += 64)
        payload[i] = (char)('A' + (i / 64) % 26);
    return payload;
}

SpeedResult finalizeDownloadResult(SpeedResult r, int targetBytes, bool httpOk, long statusCode) {
    // 5WHY (复核 2026-08-21 用户 "下载测试出错"): 慢速/停滞服务器在超时前只
    // 送达请求字节的一小部分——完成度 < 50% 判失败，把服务器问题与用户
    // 带宽分离（业界测速惯例）。">1KB 即算成功"启发式仅对非 HTTP 二进制
    // 响应保留（socket 兜底路径），curl 路径经 HTTP 校验不落此支。
    const bool usable = (httpOk || r.bytes > 1000) && r.bytes > 0 && r.durationMs > 0;
    if (!usable) {
        if (!httpOk && r.error.isEmpty())
            r.error = QStringLiteral("HTTP %1").arg(statusCode);
        if (r.error.isEmpty()) {
            if (r.bytes <= 0) r.error = QStringLiteral("No Data Received");
            else if (r.durationMs <= 0) r.error = QStringLiteral("Transfer Duration Too Short");
            else r.error = QStringLiteral("Insufficient Data (%1 bytes)").arg(r.bytes);
        }
        return r;
    }
    if (targetBytes > 0 && (double)r.bytes / (double)targetBytes < 0.5) {
        r.error = QStringLiteral("Incomplete Download: %1/%2 bytes (slow server)")
            .arg(r.bytes).arg(targetBytes);
        return r;
    }
    r.mbps = (r.bytes * 8.0) / (r.durationMs / 1000.0) / 1000000.0;
    r.ok = true;
    return r;
}
} // namespace

#if !defined(NO_CURL)
namespace {
struct SpeedCapture {
    QElapsedTimer* timer = nullptr;
    qint64 firstBodyNs = 0;
    qint64 lastBodyNs = 0;
    int bodyCap = 0;
    QByteArray body;

    static size_t writeCb(char* ptr, size_t size, size_t nmemb, void* ud) {
        auto* cap = static_cast<SpeedCapture*>(ud);
        if (cap->body.size() >= cap->bodyCap) return 0;   // 达量提前终止（CURLE_WRITE_ERROR）
        const size_t n = qMin<size_t>(size * nmemb, (size_t)(cap->bodyCap - cap->body.size()));
        cap->body.append(ptr, (int)n);
        const qint64 now = cap->timer->nsecsElapsed();
        if (cap->firstBodyNs == 0) cap->firstBodyNs = now;
        cap->lastBodyNs = now;
        return n;
    }
};

CURL* newSpeedCurl(const QString& urlStr, int connectMs, long timeoutMs,
                   curl_write_callback writeCb, void* writeData) {
    CURL* curl = curl_easy_init();
    if (!curl) return nullptr;
    curl_easy_setopt(curl, CURLOPT_URL, urlStr.toUtf8().constData());
    // 共享基线（NOSIGNAL/HTTP1.1/UA/VerifyNone/超时）单一来源（5WHY 2026-09-26）
    SystemDiagnostics::configureCurlBasics(curl, static_cast<long>(connectMs), timeoutMs);
    // 5WHY (2026-09-26 重定向): 测速服务器常见 302 跳转到真实文件——旧 socket
    // 路径把重定向体当下载数据（伪 Mbps），curl 跟随重定向测真实吞吐。
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, writeData);
    return curl;
}
} // namespace

static SpeedResult httpDownloadCurl(const QString& urlStr, int targetBytes, int timeoutMs) {
    SpeedResult r = {0, 0, 0, false, {}};
    // 5WHY (2026-09-26): 曾全量 parseHttpUrl 后只取 host 判空——QUrl 解析/
    // 端口/路径组装全部白算（curl 自行解析 URL）。仅需主机非空校验。
    const QUrl urlCheck(urlStr.trimmed());
    if (urlCheck.host().isEmpty()) { r.error = QStringLiteral("Invalid URL"); return r; }
    SpeedCapture cap;
    cap.bodyCap = targetBytes + 65536;   // 与原 socket 读循环同上限
    QElapsedTimer t; t.start();
    cap.timer = &t;
    CURL* curl = newSpeedCurl(urlStr, 3000, qMin(timeoutMs, 60000),
                              SpeedCapture::writeCb, &cap);
    if (!curl) { r.error = QStringLiteral("curl_easy_init failed"); return r; }
    const CURLcode code = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);

    // 达量提前终止（writeCb 返回 0）视为成功——与原"读到目标量即停"同语义
    const bool capReached = (code == CURLE_WRITE_ERROR && cap.body.size() >= targetBytes);
    if (code != CURLE_OK && !capReached) {
        r.error = QString::fromLatin1(curl_easy_strerror(code));
        return r;
    }
    r.bytes = cap.body.size();
    const qint64 durNs = (cap.lastBodyNs > cap.firstBodyNs) ? cap.lastBodyNs - cap.firstBodyNs : 0;
    // 5WHY: 与 socket 路径同门——任何收到的字节不得误报 durationMs=0
    r.durationMs = qMax<qint64>(1, durNs / 1000000);
    // 终态统一：usable 判定 + 50% 完成度门 + Mbps 换算（单一来源，5WHY 2026-09-26）
    return finalizeDownloadResult(r, targetBytes, status == 200, status);
}

static SpeedResult httpUploadCurl(const QString& urlStr, int targetBytes, int timeoutMs) {
    SpeedResult r = {0, 0, 0, false, {}};
    const QUrl urlCheck(urlStr.trimmed());
    if (urlCheck.host().isEmpty()) { r.error = QStringLiteral("Invalid URL"); return r; }
    const QByteArray payload = speedPayload(targetBytes);   // 确定性载荷单一来源

    QElapsedTimer t; t.start();
    SpeedCapture cap;   // 响应体（200 OK）截留即可；计时走 t
    cap.bodyCap = 4096;
    cap.timer = &t;
    CURL* curl = newSpeedCurl(urlStr, 3000, static_cast<long>(timeoutMs),
                              SpeedCapture::writeCb, &cap);
    if (!curl) { r.error = QStringLiteral("curl_easy_init failed"); return r; }
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.constData());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)payload.size());
    SystemDiagnostics::CurlSlist hdrs;   // RAII：perform 后自动释放
    hdrs.list = curl_slist_append(nullptr, "Content-Type: application/octet-stream");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs.list);
    const CURLcode code = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);

    r.bytes = targetBytes;
    // 5WHY: 同 socket 路径——吞吐相关相位是发送本身（perform 含 send+recv）
    r.durationMs = qMax<qint64>(1, t.nsecsElapsed() / 1000000);
    if (code != CURLE_OK) {
        r.error = QString::fromLatin1(curl_easy_strerror(code));
        return r;
    }
    if (status == 200) {
        r.mbps = (r.bytes * 8.0) / (r.durationMs / 1000.0) / 1000000.0;
        r.ok = true;
    } else {
        r.error = QStringLiteral("HTTP %1").arg(status);
    }
    return r;
}

static double httpTtfbCurl(const QString& host, int port, const QString& path,
                           int connectTimeoutMs, int readTimeoutSec) {
    if (host.isEmpty()) return -1.0;
    const QString urlStr = QStringLiteral("http://%1:%2%3")
        .arg(host).arg(port).arg(path.isEmpty() ? QStringLiteral("/") : path);
    // 丢弃响应体——TTFB 只需 STARTTRANSFER
    CURL* curl = newSpeedCurl(urlStr, connectTimeoutMs,
                              static_cast<long>(connectTimeoutMs + readTimeoutSec * 1000),
                              +[](char*, size_t sz, size_t n, void*) -> size_t { return sz * n; }, nullptr);
    if (!curl) return -1.0;
    const CURLcode code = curl_easy_perform(curl);
    double ttfb = -1.0;
    if (code == CURLE_OK) {
        double tStart = 0.0;
        curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME, &tStart);
        ttfb = tStart * 1000.0;
    }
    curl_easy_cleanup(curl);
    return ttfb;
}
#endif // !NO_CURL

#if defined(NO_CURL)
// HTTP download socket 兜底（仅 NO_CURL 平台编译——iOS/Android/无库桌面）
// SpeedResult defined in GHelpers.h
static SpeedResult httpDownloadSocket(const QString& urlStr, int targetBytes, int timeoutMs) {
    SpeedResult r = {0, 0, 0, false, {}};
    ParsedUrl pu = parseHttpUrl(urlStr);
    if (pu.host.isEmpty()) { r.error = QStringLiteral("Invalid URL"); return r; }
    QString host = pu.host; int port = pu.port;
    QString path = pu.path;

    QElapsedTimer t; t.start();
    int sock = tcpConnect(host, port, 3000);
    if (sock < 0) { r.error = QStringLiteral("TCP Connect Failed"); return r; }

    // Send HTTP GET (shared EAGAIN-safe helper, 30s hard guard preserved)
    // 5WHY: Host header omitted port — same bug that was fixed in httpGet()
    // (see above). Speed-test servers on port 8080 behind reverse proxies
    // may route incorrectly without the explicit port per RFC 7230 §5.4.
    QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: NetDiagnostics/1.0\r\nConnection: close\r\n\r\n")
        .arg(path, hostHeader(host, port)).toUtf8();
    if (!sendAll(sock, req, 30000)) {
        r.error = QStringLiteral("HTTP Request Send Incomplete"); closeSocket(sock); return r;
    }

    // Read with timing — measure throughput (wall-clock guarded)
    qint64 startNs = t.nsecsElapsed();
    QByteArray body;
    QByteArray headerBuf;
    bool headersDone = false;
    bool httpOk = false;
    long httpStatus = 0;
    char buf[32768];
    QElapsedTimer recvGuard; recvGuard.start();
    while (body.size() < targetBytes + 65536) {
        // 5WHY: wall-clock guard was placed after recv() but before data
        // processing. If the guard fired after a successful recv(), the
        // just-read chunk in buf was silently discarded —undercounting
        // bytes or dropping the HTTP status line entirely. Move the guard
        // BEFORE select() so previously-processed data is safe and only
        // NEW reads are prevented.
        // 5WHY: hardcoded 60000 inconsistent with httpGet timeoutMs fix.
        // Use the caller's timeout as the wall-clock guard — a 10s caller
        // should not block for 60s.  Keep a 60s ceiling for large downloads.
        // qMIN (not qMAX): respect caller's timeout, with 60s safety net.
        int remaining = qMin(timeoutMs, 60000) - (int)recvGuard.elapsed();
        if (remaining <= 0) break;
        if (!waitReadable(sock, remaining)) break;
        ssize_t n = recvChunk(sock, buf, sizeof(buf));
        if (n < 0) continue;  // EAGAIN — retry
        if (n == 0) break;    // EOF or real error
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
                httpStatus = (codeStart > 0 && codeStart + 4 <= statusLine.size())
                    ? statusLine.mid(codeStart + 1, 3).toLong() : 0;
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
    // 5WHY: elapsedNs was floored to 1 NANOSECOND, so a fast localhost/LAN
    // download (headers + body in one recv) still produced durationMs=0 →
    // "Transfer Duration Too Short" despite a complete transfer.  Floor the
    // millisecond value at 1 so any received bytes are never misreported.
    r.durationMs = qMax<qint64>(1, elapsedNs / 1000000);
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
    // 终态统一（与 curl 路径同门，5WHY 2026-09-26）：usable 判定 + 50%
    // 完成度门 + Mbps 换算 + 错误映射单一来源。httpOk=false 且 bytes>1KB
    // 的非 HTTP 二进制响应仍按启发式接受（兜底路径语义）。
    return finalizeDownloadResult(r, targetBytes, httpOk, httpStatus);
}

// HTTP upload socket 兜底（仅 NO_CURL 平台编译）
static SpeedResult httpUploadSocket(const QString& urlStr, int targetBytes, int timeoutMs) {
    SpeedResult r = {0, 0, 0, false, {}};
    ParsedUrl pu = parseHttpUrl(urlStr);
    if (pu.host.isEmpty()) { r.error = QStringLiteral("Invalid URL"); return r; }
    QString host = pu.host; int port = pu.port;

    QElapsedTimer t; t.start();
    int sock = tcpConnect(host, port, 3000);
    if (sock < 0) { r.error = QStringLiteral("TCP Connect Failed"); return r; }

    const QByteArray payload = speedPayload(targetBytes);   // 确定性载荷单一来源

    QByteArray req = QStringLiteral("POST %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: NetDiagnostics/1.0\r\nContent-Type: application/octet-stream\r\nContent-Length: %3\r\nConnection: close\r\n\r\n")
        .arg(pu.path.isEmpty() ? QStringLiteral("/") : pu.path, hostHeader(host, port)).arg(targetBytes).toUtf8();
    req.append(payload);

    // 5WHY: startNs was placed AFTER the send loop, so it measured only
    // the server response receive time. For upload, the throughput-relevant
    // phase is the send itself — the response is just a tiny HTTP 200 OK.
    // Move startNs before the send loop so Mbps = bytes * 8 / (send + recv).
    qint64 startNs = t.nsecsElapsed();

    // Send request + body (shared EAGAIN-safe helper)
    if (!sendAll(sock, req, timeoutMs)) {
        r.error = QStringLiteral("Upload Send Incomplete"); closeSocket(sock); return r;
    }

    // Read HTTP response (shared select+recv helpers)
    QByteArray respBuf;
    char buf[4096];
    QElapsedTimer recvGuard; recvGuard.start();
    while (respBuf.size() < 4096) {
        int remaining = qMin(timeoutMs, 30000) - (int)recvGuard.elapsed();
        if (remaining <= 0) break;
        if (!waitReadable(sock, remaining)) break;
        ssize_t n = recvChunk(sock, buf, sizeof(buf));
        if (n < 0) continue;   // EAGAIN — retry
        if (n == 0) break;     // EOF or real error
        respBuf.append(buf, (int)n);
    }
    closeSocket(sock);

    qint64 elapsedNs = t.nsecsElapsed() - startNs;
    if (elapsedNs <= 0) elapsedNs = 1;
    r.bytes = targetBytes;  // we count bytes sent
    // 5WHY: same sub-millisecond floor as httpDownload — a fast upload must
    // never be misreported as "No Upload Response" via durationMs=0.
    r.durationMs = qMax<qint64>(1, elapsedNs / 1000000);

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

#endif // NO_CURL

// TCP ping (simple connect RTT) — measures raw TCP handshake latency
// （G3 Internet 探针使用；socket 语义全平台保留）
int tcpPingMs(const QString& host, int port) {
    QElapsedTimer t; t.start();
    int sock = tcpConnect(host, port, 2000);
    int ms = static_cast<int>(t.elapsed());
    if (sock < 0) ms = -1;
    else closeSocket(sock);
    return ms;
}

#if defined(NO_CURL)
// HTTP TTFB probe — TCP connect + HTTP GET → time to first byte.
// Returns ms (including TCP handshake), or -1.0 on failure.
// Shared by GeoProbe (probeAllServers, selectBestServer, pickBestInCountry,
// pickBestInRegion) and geoIPLoc Pass 2.
static double httpTtfbSocket(const QString& host, int port, const QString& path,
                          int connectTimeoutMs, int readTimeoutSec) {
    if (host.isEmpty()) return -1.0;  // guard against malformed URLs
    QElapsedTimer t; t.start();
    int sock = tcpConnect(host, port, connectTimeoutMs);
    if (sock < 0) return -1.0;
    QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: NetDiagnostics/1.0\r\nConnection: close\r\n\r\n")
        .arg(path, hostHeader(host, port)).toUtf8();
    // EAGAIN-safe send (shared helper; 10s wall-clock guard preserves the
    // old 100-attempt x 100ms budget).  Socket is non-blocking (tcpConnect).
    // 5WHY: incomplete send used to fall through to read-select, wasting
    // readTimeoutSec — server never received the full request, so it cannot
    // respond.  sendAll returns false on incomplete send → early return.
    if (!sendAll(sock, req, 10000)) { closeSocket(sock); return -1.0; }

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


#endif // NO_CURL

// ── 调度：桌面 !NO_CURL → curl easy；NO_CURL → socket 兜底 ────────────────
SpeedResult httpDownload(const QString& urlStr, int targetBytes, int timeoutMs) {
#if !defined(NO_CURL)
    return httpDownloadCurl(urlStr, targetBytes, timeoutMs);
#else
    return httpDownloadSocket(urlStr, targetBytes, timeoutMs);
#endif
}

SpeedResult httpUpload(const QString& urlStr, int targetBytes, int timeoutMs) {
#if !defined(NO_CURL)
    return httpUploadCurl(urlStr, targetBytes, timeoutMs);
#else
    return httpUploadSocket(urlStr, targetBytes, timeoutMs);
#endif
}

double httpTtfb(const QString& host, int port, const QString& path,
                int connectTimeoutMs, int readTimeoutSec) {
#if !defined(NO_CURL)
    return httpTtfbCurl(host, port, path, connectTimeoutMs, readTimeoutSec);
#else
    return httpTtfbSocket(host, port, path, connectTimeoutMs, readTimeoutSec);
#endif
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
// 5WHY (2026-09-05 SIOF): 哈希曾是命名空间作用域静态初始化（~250 条目
// QHash 在 main() 前构造）——跨 TU 初始化顺序未定义（iOS dyld 顺序不定），
// 静态初始化期调用 countryCode3/countryFullName 会读到未构造哈希。与
// DiagId.h/DiagnosticMeta/GeoProbe 同惯例：Meyer's function-local static。
const QHash<QString, QPair<QString, QString>>& countryBy2() {
    static const QHash<QString, QPair<QString, QString>> kCountryBy2 = []() {
        QHash<QString, QPair<QString,QString>> m;
        for (const auto& e : kCountryMap) m[QString::fromLatin1(e.a2)] = {QString::fromLatin1(e.a3), QString::fromLatin1(e.name)};
        return m;
    }();
    return kCountryBy2;
}

QString countryCode3(const QString& code2) {
    const auto& by2 = countryBy2();
    auto it = by2.constFind(code2);
    return (it != by2.cend()) ? it->first : code2;
}
QString countryFullName(const QString& code2) {
    if (code2.isEmpty() || code2 == QStringLiteral("XX"))
        return QStringLiteral("Unknown");
    const auto& by2 = countryBy2();
    auto it = by2.constFind(code2);
    return (it != by2.cend()) ? it->second : code2;
}

} // namespace SystemDiagnostics
