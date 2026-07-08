#pragma once
// =============================================================================
// G5WebsiteUrl.cpp — curl-style raw-socket HTTP diagnostics
// =============================================================================
#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif
#include "engine/diagnostics/G5/G5WebsiteUrl.h"
#include "engine/diagnostics/NetworkProbe.h"
#include "util/Logger.h"
#include <QUrl>
#include <QHostInfo>
#include <QSslSocket>
#include <QSslCertificate>
#include <QDateTime>
#include <QElapsedTimer>
#include <QTcpSocket>
#include <cstring>
#include <cstdio>
#include "util/NetUtil.h"  // cross-platform closeSocket, setSocketNonBlocking etc.

DiagnosticResult iosHttpDiagnostic(DiagId id, const QString& target); // global scope (ObjC)

namespace G5WebsiteUrl {


static DiagnosticResult g5Result(DiagId id, const QString& summary,
                                  DiagStatus status = DiagStatus::Pass) {
    DiagnosticResult r;
    r.id = id; r.group = DiagGroup::G5; r.status = status;
    r.summary = summary; r.timestamp = QDateTime::currentDateTime();
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
    return r;
}

// ═════════════════════════════════════════════════════════════════════════════
// libcurl-based HTTP engine — full curl functionality
// ═════════════════════════════════════════════════════════════════════════════
#ifndef NO_CURL
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

static CurlResult curlHttp(const QUrl& url, int timeoutMs, bool followRedirect = false,
                           int maxRedirects = 5) {
    CurlResult cr;
    (void)maxRedirects;
    CURL* curl = curl_easy_init();
    if (!curl) { cr.error = QStringLiteral("curl_easy_init() failed"); return cr; }

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
    // Disable SSL verification for diagnostics (like curl -k)
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    CURLcode res = curl_easy_perform(curl);

    if (res == CURLE_OK) {
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        cr.statusCode = (int)httpCode;
        cr.ok = true;

        // curl-compatible timing
        curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME_T, &cr.dnsMs);    cr.dnsMs *= 1000;
        curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME_T, &cr.connectMs);    cr.connectMs *= 1000;
        curl_easy_getinfo(curl, CURLINFO_APPCONNECT_TIME_T, &cr.appConnectMs); cr.appConnectMs *= 1000;
        curl_easy_getinfo(curl, CURLINFO_PRETRANSFER_TIME_T, &cr.preTransferMs); cr.preTransferMs *= 1000;
        curl_easy_getinfo(curl, CURLINFO_STARTTRANSFER_TIME_T, &cr.firstByteMs); cr.firstByteMs *= 1000;
        curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME_T, &cr.totalMs);      cr.totalMs *= 1000;

        // Redirect URL
        char* redirectUrl = nullptr;
        curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &redirectUrl);
        if (redirectUrl) cr.redirectLocation = QString::fromUtf8(redirectUrl);

        // Timing footer
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
    return cr;
}
#endif // NO_CURL

// ═════════════════════════════════════════════════════════════════════════════

// ── G5.1 URL Parsing ─────────────────────────────────────────────────────
// AndroidNetworkInfo.h — Android native diagnostics
#pragma once
#include "models/DiagnosticResult.h"
#include "models/DiagId.h"
DiagnosticResult androidDnsDiag(DiagId id, const QString& target);
DiagnosticResult androidWifiDiag(DiagId id);
DiagnosticResult androidCellularDiag(DiagId id);
DiagnosticResult androidDhcpDiag(DiagId id);
DiagnosticResult androidGatewayDiag(DiagId id);
DiagnosticResult androidHttpDiag(DiagId id, const QString& target);
