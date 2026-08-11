#pragma once
// =============================================================================
// G5Common.h — curl-style raw-socket HTTP diagnostics
// =============================================================================
#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif
#include "Diagnostics/Model/G5/G5WebsiteUrl.h"
#include "Diagnostics/Model/NetworkProbe.h"
#include "Common/Utils/Logger.h"
#include <QUrl>
#include <QHostInfo>
#include <QSslSocket>
#include <QSslCertificate>
#include <QDateTime>
#include <QElapsedTimer>
#include <QTcpSocket>
#include <cstdio>
#include "Common/Utils/NetUtil.h"  // cross-platform closeSocket, setSocketNonBlocking etc.

// 5WHY: iosHttpDiagnostic is ObjC and must be at global scope (not inside
// C++ namespace).  Only needed on iOS; guard prevents accidental use on
// desktop/Android where the definition does not exist.
#if defined(PLATFORM_IOS)
DiagnosticResult iosHttpDiagnostic(DiagId id, const QString& target); // global scope (ObjC)
#endif

namespace G5WebsiteUrl {


static DiagnosticResult g5Result(DiagId id, const QString& summary,
                                  DiagStatus status = DiagStatus::Pass) {
    DiagnosticResult r;
    r.id = id; r.group = DiagGroup::G5; r.status = status;
    r.summary = summary; r.timestamp = QDateTime::currentDateTime();
    // 5WHY (altitude fix): g5Result() never set errorOutput — 17+ G5 tests
    // with protocol-level failures (MySQL handshake, Redis PONG, MQTT CONNACK,
    // curl errors, SSL cert, URL parse) had invisible red error blocks on
    // DetailPage.  Auto-populate from summary so every Fail/Warning result
    // shows a visible error block.  Callers with richer error detail (socket
    // error string) can overwrite this default.
    if (status == DiagStatus::Fail || status == DiagStatus::Warning
        || status == DiagStatus::Error)
        r.errorOutput = summary;
    return r;
}

// ── Convenience wrappers for per-protocol diagnostics ──────────────────
static DiagnosticResult skipped(DiagId id, const QString& reason) {
    return g5Result(id, reason, DiagStatus::Skipped);
}
static DiagnosticResult result(DiagId id, const QString& summary,
                               DiagStatus status = DiagStatus::Pass,
                               const QString& details = {},
                               qint64 durationMs = 0) {
    DiagnosticResult r = g5Result(id, summary, status);
    if (!details.isEmpty()) { r.rawOutput = details; r.details = details; }
    if (durationMs > 0) r.durationMs = durationMs;
    // 5WHY: prefer details over summary for errorOutput — g5Result already
    // set errorOutput=summary on Fail.  If the caller passed richer detail
    // text, use that instead of the one-line summary.
    if (!details.isEmpty() && (status == DiagStatus::Fail
        || status == DiagStatus::Warning || status == DiagStatus::Error))
        r.errorOutput = details;
    return r;
}

// ── Shared TCP probe scaffolding (Query-template protocol tests) ────────
// 5WHY: FTP/SSH/Email/Telnet/Redis each hand-rolled the same raw QTcpSocket
// lifecycle (connect→waitForConnected→waitForReadyRead→readAll→disconnect)
// even though NetworkProbe::tcpProbe exists (it was extracted for exactly
// this) — half the G5 tests used it, half duplicated it → drift + dead code.
// g5Probe() owns the socket lifecycle AND the result scaffold
// (host/port/connected/latencyMs data) so each protocol file only keeps its
// protocol-specific parsing (banner syntax, handshake verification).
struct G5ProbeResult {
    bool connected = false;
    QByteArray banner;       // bytes read after optional sendData command
    qint64 durationMs = 0;
    int port = 0;
    QString error;           // socket error string on connection failure
};
static G5ProbeResult g5Probe(const QUrl& u, const QByteArray& sendData = {},
                             int connectTimeoutMs = 5000, int readTimeoutMs = 3000) {
    G5ProbeResult p;
    p.port = portForUrl(u);
    auto probe = NetworkProbe::tcpProbe(u.host(), p.port, connectTimeoutMs,
                                        readTimeoutMs, sendData);
    p.connected = probe.connected;
    p.banner = probe.data;
    p.durationMs = probe.elapsedMs;
    p.error = probe.error;
    return p;
}
// Builds the base Query result after a probe.  On connect failure the result
// is FINAL (Fail + "Connection failed") — the caller returns it immediately.
// On success the caller fills summary/status + protocol-specific data keys.
// 5WHY (review): the pre-refactor per-protocol fail paths each set EVERY data
// key (banner:"", version:"", ...) — g5ProbeResult must keep the shared
// banner key present on failure too, so consumers never read an undefined
// key on a failed result.
// 5WHY (terminal output regression): g5ProbeResult() set r.data["banner"]=QString()
// but never set r.details or r.rawOutput → DetailPage "Terminal Output"
// section was permanently empty for ALL 8 refactored G5 protocol tests.
// Now populates details/rawOutput from the probe banner on success so the
// terminal output section renders the raw server response.
static DiagnosticResult g5ProbeResult(DiagId id, const QUrl& u,
                                      const G5ProbeResult& p) {
    DiagnosticResult r = g5Result(id, p.connected ? QString()
                                                  : QStringLiteral("Connection failed"),
                                  p.connected ? DiagStatus::Pass : DiagStatus::Fail);
    r.durationMs = p.durationMs;
    r.data["host"] = u.host();
    r.data["port"] = p.port;
    r.data["connected"] = p.connected;
    r.data["banner"] = p.connected ? QString::fromUtf8(p.banner) : QString();
    r.data["latencyMs"] = p.durationMs;
    // Populate terminal output (details/rawOutput) from the raw probe banner.
    // Individual protocol tests can overwrite this with richer formatted
    // output (e.g. hex dumps for MQTT/MySQL handshake parsing).
    if (p.connected && !p.banner.isEmpty()) {
        r.details = QString::fromUtf8(p.banner);
        r.rawOutput = r.details;
    }
    // 5WHY: errorOutput was never set on connection failure — the DetailPage
    // error block was invisible for all 9 protocol tests.  Populate it so the
    // red error block renders on the detail page for failed connections.
    // 5WHY (error detail): the original fix used only host + port — no socket
    // error string.  Users saw "Connection to 192.168.1.1:80 failed" with no
    // indication of WHY (timeout vs refused vs unreachable).  Root cause:
    // NetworkProbe::TcpProbeResult lacked an error field.  Now includes the
    // captured socket error string so the error block shows the actual reason.
    // 5WHY (double-rendering): setting details=errorOutput on failure caused
    // the same error message to appear in BOTH the red error block AND the
    // dark terminal-output section on the detail page.  On connection failure
    // there is NO protocol data to show — terminal should be empty.
    if (!p.connected) {
        r.errorOutput = p.error.isEmpty()
            ? QStringLiteral("Connection to %1:%2 failed")
                  .arg(u.host()).arg(p.port)
            : QStringLiteral("Connection to %1:%2 failed: %3")
                  .arg(u.host()).arg(p.port).arg(p.error);
    }
    return r;
}

// ═════════════════════════════════════════════════════════════════════════════
// libcurl-based HTTP engine — full curl functionality
// ═════════════════════════════════════════════════════════════════════════════
#if !defined(NO_CURL)
#include <curl/curl.h>

struct CurlResult {
    QStringList lines;
    int statusCode = 0;
    double dnsMs = 0;
    double connectMs = 0;
    double appConnectMs = 0;
    double preTransferMs = 0;
    double firstByteMs = 0;
    double totalMs = 0;
    bool ok = false;
    QString error;
    QString redirectLocation;
};

// Callback: capture verbose debug output
static int curlDebugCallback(CURL*, curl_infotype type, char* data, size_t size, void* userp) {
    Q_UNUSED(type)
    QByteArray line = QByteArray(data, (int)size).trimmed();
    if (line.isEmpty()) return 0;
    QStringList* lines = static_cast<QStringList*>(userp);
    // Prefix for request/response headers
    char prefix = (type == CURLINFO_HEADER_OUT) ? '>' : (type == CURLINFO_HEADER_IN) ? '<' : '*';
    if (type == CURLINFO_TEXT || type == CURLINFO_HEADER_IN || type == CURLINFO_HEADER_OUT)
        lines->append(QStringLiteral("%1 %2").arg(prefix).arg(QString::fromUtf8(line)));
    return 0;
}

// Callback: capture response body (for status + redirect detection)
static size_t curlWriteCallback(char* ptr, size_t, size_t nmemb, void* userp) {
    QByteArray* body = static_cast<QByteArray*>(userp);
    body->append(ptr, (int)nmemb);
    return nmemb;
}

// 5WHY: Transient network issues (DNS timeout, TCP reset, SSL renegotiation
// failure) can cause HTTP diagnostics to fail.  Up to 5 attempts — first
// success returns immediately, 5 failures return the last error.
static CurlResult curlHttp(const QUrl& url, int timeoutMs, bool followRedirect = false,
                           int maxRedirects = 5) {
    static const int kMaxAttempts = 5;
    auto perform = [&](CurlResult& cr, int attempt) -> bool {
        cr = CurlResult{};
        CURL* curl = curl_easy_init();
        if (!curl) { cr.error = QStringLiteral("curl_easy_init() failed"); return false; }

        QByteArray urlBytes = url.toString().toUtf8();
        QByteArray responseBody;

        curl_easy_setopt(curl, CURLOPT_URL, urlBytes.constData());
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)timeoutMs);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, (long)(timeoutMs / 3));
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "NetDiagnostics/1.0 (libcurl)");
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, curlDebugCallback);
        curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &cr.lines);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
        if (followRedirect) {
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_MAXREDIRS, (long)maxRedirects);
            curl_easy_setopt(curl, CURLOPT_POSTREDIR, (long)(CURL_REDIR_POST_301|CURL_REDIR_POST_302|CURL_REDIR_POST_303));
        } else {
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD request
        }
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        CURLcode res = curl_easy_perform(curl);

        if (res == CURLE_OK) {
            long httpCode = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
            cr.statusCode = (int)httpCode;
            cr.ok = true;

            // 5WHY: CURLINFO_*_TIME_T requires a curl_off_t* (64-bit int) output
            // argument, but the struct fields are double. Passing &double let
            // curl write an integer bit-pattern into the double — garbage timing
            // values (huge/denormal) after the *1000 scaling. Use curl_off_t
            // temporaries, then convert to double (seconds → ms).
            curl_off_t tDns = 0, tConn = 0, tAppConn = 0, tPre = 0, tFirst = 0, tTotal = 0;
            curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME_T, &tDns);      cr.dnsMs       = (double)tDns * 1000.0;
            curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME_T,   &tConn);      cr.connectMs   = (double)tConn * 1000.0;
            curl_easy_getinfo(curl, CURLINFO_APPCONNECT_TIME_T,&tAppConn);   cr.appConnectMs= (double)tAppConn * 1000.0;
            curl_easy_getinfo(curl, CURLINFO_PRETRANSFER_TIME_T,&tPre);      cr.preTransferMs= (double)tPre * 1000.0;
            curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME_T,&tFirst);  cr.firstByteMs = (double)tFirst * 1000.0;
            curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME_T,     &tTotal);     cr.totalMs     = (double)tTotal * 1000.0;

            char* redirectUrl = nullptr;
            curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &redirectUrl);
            if (redirectUrl) cr.redirectLocation = QString::fromUtf8(redirectUrl);

            if (attempt > 0)
                cr.lines.prepend(QStringLiteral("* (Attempt %1 succeeded)").arg(attempt + 1));

            cr.lines.append(QString());
            cr.lines.append(QStringLiteral("  %1  %2  %3  %4  %5  %6")
                .arg(QStringLiteral("time_namelookup:"),  -22).arg(QStringLiteral("time_connect:"),  -20)
                .arg(QStringLiteral("time_appconnect:"),  -22).arg(QStringLiteral("time_pretransfer:"),  -22)
                .arg(QStringLiteral("time_starttransfer:"), -22).arg(QStringLiteral("time_total:"),  -18));
            cr.lines.append(QStringLiteral("  %1  %2  %3  %4  %5  %6")
                .arg(QStringLiteral("%1 ms").arg(cr.dnsMs, 0, 'f', 1), -22)
                .arg(QStringLiteral("%1 ms").arg(cr.connectMs, 0, 'f', 1), -20)
                .arg(QStringLiteral("%1 ms").arg(cr.appConnectMs, 0, 'f', 1), -22)
                .arg(QStringLiteral("%1 ms").arg(cr.preTransferMs, 0, 'f', 1), -22)
                .arg(QStringLiteral("%1 ms").arg(cr.firstByteMs, 0, 'f', 1), -22)
                .arg(QStringLiteral("%1 ms").arg(cr.totalMs, 0, 'f', 1), -18));

            if (!responseBody.isEmpty()) {
                cr.lines.append(QString());
                cr.lines.append(QStringLiteral("* Body: %1 bytes").arg(responseBody.size()));
                QByteArray preview = responseBody.left(500);
                cr.lines.append(QStringLiteral("{"));
                for (const auto& line : QString::fromUtf8(preview).split('\n')) {
                    if (!line.trimmed().isEmpty())
                        cr.lines.append(QStringLiteral("  %1").arg(line.left(120)));
                }
                if (responseBody.size() > 500)
                    cr.lines.append(QStringLiteral("  ... (%1 more bytes)").arg(responseBody.size() - 500));
                cr.lines.append(QStringLiteral("}"));
            }
        } else {
            cr.error = QStringLiteral("curl error: %1").arg(QString::fromUtf8(curl_easy_strerror(res)));
        }

        curl_easy_cleanup(curl);
        return cr.ok;
    };

    CurlResult cr;
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        if (perform(cr, attempt)) return cr;       // success — return immediately
    }
    return cr;                                       // all 5 attempts failed
}
#endif // NO_CURL

} // namespace G5WebsiteUrl
