// =============================================================================
// G5WebsiteUrl.cpp — curl-style raw-socket HTTP diagnostics
// =============================================================================
#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif
#include "engine/diagnostic/G5WebsiteUrl.h"
#include "engine/runner/NetworkProbe.h"
#include "util/Logger.h"
#include <QUrl>
#include <QHostInfo>
#include <QSslSocket>
#include <QSslCertificate>
#include <QDateTime>
#include <cstring>
#include <cstdio>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
// Use an inline helper to avoid renaming Qt's QAbstractSocket::close() via macro
static inline void closeSocket(int fd) { closesocket((SOCKET)(uintptr_t)fd); }
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
static inline void closeSocket(int fd) { ::close(fd); }
#endif

namespace G5WebsiteUrl {

static const QMap<QString, int> s_defaultPorts = {
    {"http",80},{"https",443},{"ftp",21},{"ftps",990},{"sftp",22},{"ssh",22},
    {"telnet",23},{"rdp",3389},{"smtp",25},{"smtps",465},{"imap",143},{"imaps",993},
    {"pop3",110},{"pop3s",995},{"mysql",3306},{"postgresql",5432},{"redis",6379},
    {"mongodb",27017},{"mssql",1433},{"ldap",389},{"ldaps",636},{"mqtt",1883},{"mqtts",8883}
};

int defaultPortForScheme(const QString& scheme) {
    return s_defaultPorts.value(scheme.toLower(), 80);
}

QStringList knownSchemes() {
    return s_defaultPorts.keys();
}

QUrl validate(const QString& target) {
    QUrl u(target, QUrl::StrictMode);
    if (u.isValid() && !u.scheme().isEmpty()) return u;
    if (target.contains(':') && !target.contains("://")) {
        QString bracketed = target.startsWith('[') ? target : QStringLiteral("[%1]").arg(target);
        u = QUrl(QStringLiteral("http://") + bracketed, QUrl::StrictMode);
        if (u.isValid()) return u;
    }
    u = QUrl(QStringLiteral("http://") + target);
    return u.isValid() ? u : QUrl();
}

int portForUrl(const QUrl& u) {
    return u.port() > 0 ? u.port() : s_defaultPorts.value(u.scheme(), 80);
}

static DiagnosticResult g5Result(DiagId id, const QString& summary,
                                  DiagStatus status = DiagStatus::Pass) {
    DiagnosticResult r;
    r.id = id; r.group = DiagGroup::G5; r.status = status;
    r.summary = summary; r.timestamp = QDateTime::currentDateTime();
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
