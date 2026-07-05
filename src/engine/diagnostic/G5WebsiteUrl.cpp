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

static QUrl validate(const QString& target) {
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

static int portForUrl(const QUrl& u) {
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
DiagnosticResult urlParsing(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5UrlParsing, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (!u.isValid()) return g5Result(DiagId::G5UrlParsing, "Invalid URL", DiagStatus::Fail);
    auto r = g5Result(DiagId::G5UrlParsing, QStringLiteral("Scheme=%1 Host=%2 Port=%3").arg(u.scheme(), u.host()).arg(portForUrl(u)));
    r.rawOutput = QStringLiteral("Scheme: %1\nHost: %2\nPort: %3\nPath: %4\nQuery: %5")
        .arg(u.scheme(), u.host()).arg(portForUrl(u)).arg(u.path(), u.query());
    r.details = r.rawOutput;
    return r;
}

// ── G5.2 TCP Connect ─────────────────────────────────────────────────────
DiagnosticResult tcpConnect(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5TcpConnect, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (!u.isValid() || u.host().isEmpty())
        return g5Result(DiagId::G5TcpConnect, "Invalid target", DiagStatus::Fail);
    int port = portForUrl(u);
    auto cr = NetworkProbe::tcpConnect(u.host(), port, 5000);
    auto r = g5Result(DiagId::G5TcpConnect,
        cr.connected ? QStringLiteral("Connected in %1ms").arg(cr.latencyMs)
                     : QStringLiteral("Failed: %1").arg(cr.error),
        cr.connected ? DiagStatus::Pass : DiagStatus::Fail);
    r.properties.append(ResultProperty("Host", u.host()));
    r.properties.append(ResultProperty("Port", QString::number(port)));
    r.durationMs = cr.latencyMs;
    return r;
}

// ── G5.3 Service Banner ──────────────────────────────────────────────────
DiagnosticResult serviceBanner(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5ServiceBanner, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (!u.isValid() || u.host().isEmpty())
        return g5Result(DiagId::G5ServiceBanner, "Invalid target", DiagStatus::Fail);
    int port = portForUrl(u);
    // Raw socket banner grab
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return g5Result(DiagId::G5ServiceBanner, "socket() failed", DiagStatus::Fail);
    struct addrinfo hints = {}, *res;
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    char ps[16]; snprintf(ps, sizeof(ps), "%d", port);
    QByteArray hb = u.host().toUtf8();
    if (getaddrinfo(hb.constData(), ps, &hints, &res) != 0) { closeSocket(sock); return g5Result(DiagId::G5ServiceBanner, "DNS failed", DiagStatus::Fail); }
    struct sockaddr_in addr; memcpy(&addr, res->ai_addr, sizeof(addr)); freeaddrinfo(res);
    // Non-blocking connect
#ifdef _WIN32
    u_long m=1; ioctlsocket(sock, FIONBIO, &m);
#else
    int fl = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, fl | O_NONBLOCK);
#endif
    ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
    struct timeval tv = {5, 0};
    if (select(sock+1, nullptr, &fdset, nullptr, &tv) <= 0) { closeSocket(sock); return g5Result(DiagId::G5ServiceBanner, "Connection timeout", DiagStatus::Fail); }
    // Read banner
    tv = {2, 0}; FD_ZERO(&fdset); FD_SET(sock, &fdset);
    char buf[4096]; QByteArray banner;
    if (select(sock+1, &fdset, nullptr, nullptr, &tv) > 0) {
        ssize_t n = recv(sock, buf, sizeof(buf)-1, 0);
        if (n > 0) banner = QByteArray(buf, (int)n);
    }
    closeSocket(sock);
    auto r = g5Result(DiagId::G5ServiceBanner,
        banner.isEmpty() ? "No banner received" : "Banner received",
        banner.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass);
    r.rawOutput = QString::fromUtf8(banner).left(500);
    return r;
}

// ── G5.4 HTTP Headers (HEAD request, curl-style) ────────────────────────
#ifndef NO_CURL
DiagnosticResult httpHeaders(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5HttpHeaders, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "http" && u.scheme() != "https")
        return g5Result(DiagId::G5HttpHeaders, "Not HTTP(S)", DiagStatus::Skipped);
    auto cr = curlHttp(u, 15000, false);
    if (!cr.ok) return g5Result(DiagId::G5HttpHeaders, cr.error, DiagStatus::Fail);
    auto r = g5Result(DiagId::G5HttpHeaders,
        QStringLiteral("HTTP %1").arg(cr.statusCode),
        cr.statusCode >= 200 && cr.statusCode < 300 ? DiagStatus::Pass :
        cr.statusCode >= 300 && cr.statusCode < 400 ? DiagStatus::Warning :
        DiagStatus::Fail);
    r.rawOutput = cr.lines.join('\n');
    r.details = r.rawOutput;
    r.durationMs = cr.totalMs;
    r.properties.append({QStringLiteral("HTTP Status"), QString::number(cr.statusCode)});
    r.properties.append({QStringLiteral("Response Time"), QStringLiteral("%1 ms").arg(cr.totalMs, 0, 'f', 1)});
    // Count response headers from < -prefixed lines
    int headerCount = 0;
    for (const auto& line : cr.lines)
        if (line.startsWith('<') && line.contains(':')) ++headerCount;
    r.properties.append({QStringLiteral("Response Headers"), QString::number(headerCount)});
    return r;
}

// ── G5.5 Curl Verbose (full GET, curl -v style complete output) ──────────
DiagnosticResult curlVerbose(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5CurlVerbose, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "http" && u.scheme() != "https")
        return g5Result(DiagId::G5CurlVerbose, "Not HTTP(S)", DiagStatus::Skipped);
    auto cr = curlHttp(u, 60000, true); // GET with body
    if (!cr.ok) return g5Result(DiagId::G5CurlVerbose, cr.error, DiagStatus::Fail);
    auto r = g5Result(DiagId::G5CurlVerbose,
        QStringLiteral("HTTP %1, %2ms total").arg(cr.statusCode).arg(cr.totalMs),
        cr.statusCode >= 200 && cr.statusCode < 400 ? DiagStatus::Pass : DiagStatus::Fail);
    r.rawOutput = cr.lines.join('\n');
    r.details = r.rawOutput;
    r.durationMs = cr.totalMs;
    return r;
}

// ── G5.6 Security Headers (HEAD + check security header presence) ───────
DiagnosticResult securityHeaders(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5SecurityHeaders, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "http" && u.scheme() != "https")
        return g5Result(DiagId::G5SecurityHeaders, "Not HTTP(S)", DiagStatus::Skipped);
    auto cr = curlHttp(u, 15000, false);
    if (!cr.ok) return g5Result(DiagId::G5SecurityHeaders, cr.error, DiagStatus::Fail);
    // Parse headers from curl output for security headers
    QStringList found;
    for (const auto& line : cr.lines) {
        if (!line.startsWith('<') || line.startsWith("< HTTP")) continue;
        auto colon = line.indexOf(':');
        if (colon > 2) found.append(line.mid(2, colon - 2).toLower().trimmed());
    }
    QStringList required = {"strict-transport-security","content-security-policy",
        "x-frame-options","x-content-type-options","x-xss-protection",
        "referrer-policy","permissions-policy"};
    QStringList missing;
    for (const auto& h : required)
        if (!found.contains(h)) missing.append(h);
    auto r = g5Result(DiagId::G5SecurityHeaders,
        missing.isEmpty() ? "All 7 present" : QStringLiteral("%1 missing").arg(missing.size()),
        missing.isEmpty() ? DiagStatus::Pass :
        missing.size() <= 4 ? DiagStatus::Warning : DiagStatus::Fail);
    r.rawOutput = cr.lines.join('\n');
    r.details = r.rawOutput;
    r.durationMs = cr.totalMs;
    return r;
}
#endif // NO_CURL

// ── G5.7 SSL Certificate ─────────────────────────────────────────────────
DiagnosticResult sslCertificate(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5SslCertificate, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "https")
        return g5Result(DiagId::G5SslCertificate, "Not HTTPS", DiagStatus::Skipped);
    int port = u.port() > 0 ? u.port() : 443;
    auto cert = NetworkProbe::sslCertInfo(u.host(), port, 10000);
    if (!cert.valid)
        return g5Result(DiagId::G5SslCertificate, "Failed to get certificate", DiagStatus::Fail);
    DiagStatus st = DiagStatus::Pass;
    QString summary = QStringLiteral("%1 days left").arg(cert.daysLeft);
    if (cert.daysLeft < 0) { st = DiagStatus::Fail; summary = "EXPIRED"; }
    else if (cert.daysLeft < 30) { st = DiagStatus::Warning; }
    QStringList lines;
    lines.append(QStringLiteral("* SSL connection established"));

    // Build a 2-column table with auto-width
    QStringList names = {
        QStringLiteral("subject"), QStringLiteral("issuer"),
        QStringLiteral("valid from"), QStringLiteral("valid to"),
        QStringLiteral("days left"), QStringLiteral("SAN count"),
        QStringLiteral("thumbprint")};
    QStringList vals = {
        cert.subject, cert.issuer,
        cert.validFrom.toString("yyyy-MM-dd"), cert.validTo.toString("yyyy-MM-dd"),
        QString::number(cert.daysLeft), QString::number(cert.subjectAltNames.size()),
        cert.thumbprint.left(40)};

    int nw = static_cast<int>(QStringLiteral("thumbprint").length());
    for (const auto& s : names) nw = qMax(nw, static_cast<int>(s.length()));
    int vw = 0;
    for (const auto& s : vals) vw = qMax(vw, static_cast<int>(s.length()));

    lines.append(QStringLiteral("*  %1  %2")
        .arg(QStringLiteral("Property"), -nw)
        .arg(QStringLiteral("Value"), -vw));
    lines.append(QStringLiteral("*  %1  %2")
        .arg(QString(nw, '-'))
        .arg(QString(vw, '-')));
    for (int i = 0; i < names.size(); ++i)
        lines.append(QStringLiteral("*  %1  %2")
            .arg(names[i], -nw)
            .arg(vals[i], -vw));
    auto r = g5Result(DiagId::G5SslCertificate, summary, st);
    r.rawOutput = lines.join('\n'); r.details = r.rawOutput;
    r.properties.append(ResultProperty("Subject", cert.subject));
    r.properties.append(ResultProperty("Issuer", cert.issuer));
    return r;
}

// ── G5.8 HTTP Redirect ───────────────────────────────────────────────────
#ifndef NO_CURL
DiagnosticResult httpRedirect(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5HttpRedirect, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "http" && u.scheme() != "https")
        return g5Result(DiagId::G5HttpRedirect, "Not HTTP(S)", DiagStatus::Skipped);
    auto cr = curlHttp(u, 15000, true);
    if (!cr.ok) return g5Result(DiagId::G5HttpRedirect, cr.error, DiagStatus::Fail);
    auto r = g5Result(DiagId::G5HttpRedirect,
        cr.statusCode >= 300 && cr.statusCode < 400
            ? QStringLiteral("Redirect %1 → %2").arg(cr.statusCode).arg(cr.redirectLocation)
            : QStringLiteral("No redirect (HTTP %1)").arg(cr.statusCode),
        cr.statusCode >= 200 && cr.statusCode < 300 ? DiagStatus::Pass : DiagStatus::Warning);
    r.rawOutput = cr.lines.join('\n'); r.details = r.rawOutput;
    r.durationMs = cr.totalMs;
    return r;
}

DiagnosticResult httpCompression(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5HttpCompression, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "http" && u.scheme() != "https")
        return g5Result(DiagId::G5HttpCompression, "Not HTTP(S)", DiagStatus::Skipped);
    auto cr = curlHttp(u, 15000, true);
    if (!cr.ok) return g5Result(DiagId::G5HttpCompression, cr.error, DiagStatus::Fail);
    bool compressed = false; QString enc;
    for (const auto& line : cr.lines) {
        if (line.startsWith("< ") && line.contains("content-encoding", Qt::CaseInsensitive)) {
            auto colon = line.indexOf(':');
            if (colon > 2) enc = line.mid(colon + 1).trimmed();
            compressed = true;
        }
    }
    auto r = g5Result(DiagId::G5HttpCompression,
        compressed ? QStringLiteral("Compressed: %1").arg(enc) : "Uncompressed", DiagStatus::Info);
    r.rawOutput = cr.lines.join('\n'); r.details = r.rawOutput;
    r.durationMs = cr.totalMs;
    return r;
}

DiagnosticResult httpTiming(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5HttpTiming, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "http" && u.scheme() != "https")
        return g5Result(DiagId::G5HttpTiming, "Not HTTP(S)", DiagStatus::Skipped);
    auto cr = curlHttp(u, 30000, true);
    if (!cr.ok) return g5Result(DiagId::G5HttpTiming, cr.error, DiagStatus::Fail);
    auto r = g5Result(DiagId::G5HttpTiming,
        QStringLiteral("DNS=%1ms Connect=%2ms SSL=%3ms FirstByte=%4ms Total=%5ms")
            .arg(cr.dnsMs, 0, 'f', 1).arg(cr.connectMs, 0, 'f', 1)
            .arg(cr.appConnectMs, 0, 'f', 1).arg(cr.firstByteMs, 0, 'f', 1)
            .arg(cr.totalMs, 0, 'f', 1),
        cr.totalMs < 1000 ? DiagStatus::Pass :
        cr.totalMs < 3000 ? DiagStatus::Warning : DiagStatus::Fail);
    r.rawOutput = cr.lines.join('\n'); r.details = r.rawOutput;
    r.durationMs = cr.totalMs;
    // Expose per-phase curl timing as structured properties
    r.properties.append({QStringLiteral("DNS Lookup"),    QStringLiteral("%1 ms").arg(cr.dnsMs, 0, 'f', 1)});
    r.properties.append({QStringLiteral("TCP Connect"),   QStringLiteral("%1 ms").arg(cr.connectMs, 0, 'f', 1)});
    r.properties.append({QStringLiteral("SSL Handshake"), QStringLiteral("%1 ms").arg(cr.appConnectMs, 0, 'f', 1)});
    r.properties.append({QStringLiteral("Time to First Byte"), QStringLiteral("%1 ms").arg(cr.firstByteMs, 0, 'f', 1)});
    r.properties.append({QStringLiteral("Total Time"),    QStringLiteral("%1 ms").arg(cr.totalMs, 0, 'f', 1)});
    r.properties.append({QStringLiteral("HTTP Status"),   QString::number(cr.statusCode)});
    if (!cr.redirectLocation.isEmpty())
        r.properties.append({QStringLiteral("Redirect"),  cr.redirectLocation});
    return r;
}
#endif // NO_CURL

DiagnosticResult ftpDiagnostics(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5FtpDiagnostics, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "ftp" && u.scheme() != "ftps")
        return g5Result(DiagId::G5FtpDiagnostics, "Not FTP", DiagStatus::Skipped);
    int port = portForUrl(u);
    QTcpSocket sock;
    sock.connectToHost(u.host(), port);
    if (!sock.waitForConnected(5000))
        return g5Result(DiagId::G5FtpDiagnostics, "Connection failed", DiagStatus::Fail);
    sock.waitForReadyRead(3000);
    QByteArray banner = sock.readAll();
    sock.write("QUIT\r\n");
    sock.disconnectFromHost();
    return g5Result(DiagId::G5FtpDiagnostics,
        banner.isEmpty() ? "No banner" : QString::fromUtf8(banner).trimmed().left(200),
        banner.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass);
}

DiagnosticResult sshDiagnostics(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5SshDiagnostics, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    if (u.scheme() != "ssh" && u.scheme() != "sftp")
        return g5Result(DiagId::G5SshDiagnostics, "Not SSH", DiagStatus::Skipped);
    int port = portForUrl(u);
    QTcpSocket sock;
    sock.connectToHost(u.host(), port);
    if (!sock.waitForConnected(5000))
        return g5Result(DiagId::G5SshDiagnostics, "Connection failed", DiagStatus::Fail);
    sock.waitForReadyRead(3000);
    QByteArray banner = sock.readAll();
    sock.disconnectFromHost();
    QString bstr = QString::fromUtf8(banner).trimmed().left(200);
    QString version;
    if (bstr.startsWith("SSH-")) version = bstr.section(' ', 0, 0);
    return g5Result(DiagId::G5SshDiagnostics,
        version.isEmpty() ? "No SSH banner" : version,
        version.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass);
}

DiagnosticResult emailDiagnostics(const QString& target) {
    if (target.isEmpty()) return g5Result(DiagId::G5EmailDiagnostics, "No target", DiagStatus::Skipped);
    QUrl u = validate(target);
    QString scheme = u.scheme();
    if (scheme != "smtp" && scheme != "imap" && scheme != "pop3"
        && scheme != "smtps" && scheme != "imaps" && scheme != "pop3s")
        return g5Result(DiagId::G5EmailDiagnostics,
                         "Not email protocol (smtp/smtps/imap/imaps/pop3/pop3s)", DiagStatus::Skipped);
    int port = portForUrl(u);
    QTcpSocket sock;
    sock.connectToHost(u.host(), port);
    if (!sock.waitForConnected(5000))
        return g5Result(DiagId::G5EmailDiagnostics, "Connection failed", DiagStatus::Fail);
    sock.waitForReadyRead(3000);
    QByteArray banner = sock.readAll();
    sock.disconnectFromHost();
    return g5Result(DiagId::G5EmailDiagnostics,
        banner.isEmpty() ? "No banner" : QString::fromUtf8(banner).trimmed().left(200),
        banner.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass);
}

} // namespace G5WebsiteUrl
