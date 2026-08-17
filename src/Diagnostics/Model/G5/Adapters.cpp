// =============================================================================
// G5/Adapters.cpp — G5 Protocol diagnostics (20 real probes, no libcurl)
//
// Ported from the archived G5*.cpp behavioral code into the new adapter
// structure.  HTTP family uses a self-contained raw HTTP/1.1 client over
// blocking QSslSocket (5WHY: QNetworkAccessManager+QEventLoop in worker
// threads races Qt's lazy proxy/locale init → SIGSEGV; blocking sockets
// need no event loop).  Protocol family (FTP/SSH/Email/Telnet/MySQL/
// PostgreSQL/Redis/MongoDB/LDAP/MQTT) uses blocking QTcpSocket with the
// archived handshake-verification logic (PING→PONG, CONNECT→CONNACK,
// MySQL handshake version, PG StartupMessage, LDAP bind, Mongo isMaster).
// Platforms per NEW-1: all 20 = All.
// =============================================================================

#include "Common/Services/PlatformAdapter.h"
#include "Common/Services/DnsResolver.h"
#include "Common/Model/DiagnosticMeta.h"
#include "Common/Model/DiagNames.h"

#if defined(PLATFORM_IOS)
// iOS NSURLSession HTTP 族（.mm 实现，全局作用域声明）
DiagnosticResult iosHttpDiagnostic(DiagId id, const QString& target);
#endif
#if defined(PLATFORM_ANDROID)
#include "Diagnostics/Model/G5/Platform/Android/NetworkDiagnostics.h"
#endif

#include <QUrl>
#include <QTcpSocket>
#include <QSslSocket>
#include <QSslCertificate>
#include <QElapsedTimer>
#include <QDateTime>
#include <QHostInfo>
#include <QSet>

#include <functional>
#include <cstring>

using namespace PlatformFlag;

namespace g5 {

// ── Result helpers (errorOutput auto-population — archive contract) ────────
static DiagnosticResult makeResult(DiagId id, DiagStatus status,
                                   const QString& summary,
                                   const QVector<ResultProperty>& props,
                                   const QString& details) {
    DiagnosticResult r;
    r.id = id; r.displayName = diagDisplayName(id); r.group = diagGroup(id);
    r.status = status; r.summary = summary; r.properties = props;
    r.details = details; r.rawOutput = details;
    r.timestamp = QDateTime::currentDateTime();
    if ((status == DiagStatus::Fail || status == DiagStatus::Warning
         || status == DiagStatus::Error) && r.errorOutput.isEmpty())
        r.errorOutput = summary;
    return r;
}

static DiagnosticResult skippedProbe(DiagId id, const QString& reason) {
    return makeResult(id, DiagStatus::Skipped, reason, {}, {});
}

// ── URL normalization + default ports (G5WebsiteUrl.h contract) ────────────
static QUrl normalizeUrl(const QString& target) {
    QString t = target.trimmed();
    if (t.isEmpty()) return {};
    if (!t.contains(QLatin1String("://"))) {
        // bare host / host:port → https
        t = QStringLiteral("https://") + t;
    }
    QUrl u(t);
    return u.isValid() && !u.host().isEmpty() ? u : QUrl();
}

static int defaultPort(const QString& schemeIn) {
    const QString s = schemeIn.toLower();
    if (s == QLatin1String("http")) return 80;
    if (s == QLatin1String("https")) return 443;
    if (s == QLatin1String("ftp")) return 21;
    if (s == QLatin1String("ftps")) return 990;
    if (s == QLatin1String("sftp") || s == QLatin1String("ssh")) return 22;
    if (s == QLatin1String("telnet")) return 23;
    if (s == QLatin1String("rdp")) return 3389;
    if (s == QLatin1String("smtp")) return 25;
    if (s == QLatin1String("smtps")) return 465;
    if (s == QLatin1String("imap")) return 143;
    if (s == QLatin1String("imaps")) return 993;
    if (s == QLatin1String("pop3")) return 110;
    if (s == QLatin1String("pop3s")) return 995;
    if (s == QLatin1String("mysql")) return 3306;
    if (s == QLatin1String("postgresql")) return 5432;
    if (s == QLatin1String("redis")) return 6379;
    if (s == QLatin1String("mongodb")) return 27017;
    if (s == QLatin1String("mssql")) return 1433;
    if (s == QLatin1String("ldap")) return 389;
    if (s == QLatin1String("ldaps")) return 636;
    if (s == QLatin1String("mqtt")) return 1883;
    if (s == QLatin1String("mqtts")) return 8883;
    return 80;
}

static int portForUrl(const QUrl& u) {
    return u.port() > 0 ? u.port() : defaultPort(u.scheme());
}

// ── Shared blocking TCP probe ──────────────────────────────────────────────
struct ProbeOutcome {
    bool connected = false;
    QByteArray banner;
    qint64 latencyMs = 0;
    QString error;
};

static ProbeOutcome tcpProbe(const QUrl& u, const QByteArray& sendData,
                             int connectTimeoutMs = 5000, int readTimeoutMs = 3000) {
    ProbeOutcome p;
    const int port = portForUrl(u);
    const QString scheme = u.scheme().toLower();
    // 5WHY (review 2026-08-17): 旧启发式 "以 s 结尾即隐式 TLS" 把 sftp（SSH
    // 家族，端口 22）误当 FTPS——对 SSH 服务器做 TLS 握手必然超时失败，
    // 健康的 SFTP 目标永远报 handshake 错误。改为显式白名单；sftp 走 SSH
    // banner 路径（probeSsh）。
    static const QSet<QString> implicitTlsSchemes = {
        QStringLiteral("ftps"), QStringLiteral("smtps"), QStringLiteral("imaps"),
        QStringLiteral("pop3s"), QStringLiteral("mqtts"), QStringLiteral("ldaps")
    };
    const bool useTls = implicitTlsSchemes.contains(scheme);
    QElapsedTimer t; t.start();

    if (useTls) {
        QSslSocket sock;
        sock.setPeerVerifyMode(QSslSocket::VerifyNone);
        sock.connectToHostEncrypted(u.host(), (quint16)port);
        if (!sock.waitForEncrypted(connectTimeoutMs)) {
            p.error = sock.errorString();
            p.latencyMs = t.elapsed();
            return p;
        }
        if (!sendData.isEmpty()) { sock.write(sendData); sock.waitForBytesWritten(2000); }
        const qint64 deadline = t.elapsed() + readTimeoutMs;
        QByteArray data;
        while (t.elapsed() < deadline) {
            if (!sock.waitForReadyRead(qMin<qint64>(300, deadline - t.elapsed()))) break;
            data += sock.readAll();
        }
        sock.disconnectFromHost();
        p.connected = true;
        p.banner = data;
        p.latencyMs = t.elapsed();
        return p;
    }

    QTcpSocket sock;
    sock.connectToHost(u.host(), (quint16)port);
    if (!sock.waitForConnected(connectTimeoutMs)) {
        p.error = sock.errorString();
        p.latencyMs = t.elapsed();
        return p;
    }
    if (!sendData.isEmpty()) { sock.write(sendData); sock.waitForBytesWritten(2000); }
    const qint64 deadline = t.elapsed() + readTimeoutMs;
    QByteArray data;
    while (t.elapsed() < deadline) {
        if (!sock.waitForReadyRead(qMin<qint64>(300, deadline - t.elapsed()))) break;
        data += sock.readAll();
    }
    sock.disconnectFromHost();
    p.connected = true;
    p.banner = data;
    p.latencyMs = t.elapsed();
    return p;
}

static DiagnosticResult probeResultScaffold(DiagId id, const QUrl& u,
                                            const ProbeOutcome& p) {
    DiagnosticResult r = makeResult(id, p.connected ? DiagStatus::Pass : DiagStatus::Fail,
        p.connected ? QString() : QStringLiteral("Connection failed"), {}, {});
    r.durationMs = p.latencyMs;
    r.data[QStringLiteral("host")] = u.host();
    r.data[QStringLiteral("port")] = portForUrl(u);
    r.data[QStringLiteral("connected")] = p.connected;
    r.data[QStringLiteral("latencyMs")] = p.latencyMs;
    r.data[QStringLiteral("banner")] = QString();   // 失败结果键齐备（消费方绝不读到 undefined）
    if (p.connected && !p.banner.isEmpty()) {
        r.details = QString::fromUtf8(p.banner);
        r.rawOutput = r.details;
    }
    if (!p.connected) {
        r.errorOutput = p.error.isEmpty()
            ? QStringLiteral("Connection to %1:%2 failed").arg(u.host()).arg(portForUrl(u))
            : QStringLiteral("Connection to %1:%2 failed: %3").arg(u.host()).arg(portForUrl(u)).arg(p.error);
    }
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// Raw HTTP/1.1 client over blocking QSslSocket (no event loop, no QNAM)
// ═════════════════════════════════════════════════════════════════════════
struct HttpResult {
    bool ok = false;
    int statusCode = 0;
    QByteArray statusLine;
    QList<QPair<QByteArray, QByteArray>> headers;   // lowercase name → value
    QByteArray body;
    QString error;
    QString redirectLocation;
    qint64 dnsMs = 0, connectMs = 0, tlsMs = 0, firstByteMs = 0, totalMs = 0;
    QStringList verboseLines;   // "> request" / "< response" style
};

static bool parseResponseHead(const QByteArray& head, HttpResult& r) {
    const QList<QByteArray> lines = head.split('\r');
    if (lines.isEmpty()) return false;
    r.statusLine = lines.first();
    const QList<QByteArray> parts = r.statusLine.split(' ');
    if (parts.size() < 2) return false;
    r.statusCode = parts[1].toInt();
    for (int i = 1; i < lines.size(); ++i) {
        const QByteArray& line = lines[i];
        if (line.isEmpty()) continue;
        const int colon = line.indexOf(':');
        if (colon <= 0) continue;
        QByteArray name = line.left(colon).toLower();
        QByteArray value = line.mid(colon + 1).trimmed();
        r.headers.append({name, value});
        if (name == "location")
            r.redirectLocation = QString::fromUtf8(value);
    }
    return true;
}

static QByteArray headerValue(const HttpResult& r, const char* name) {
    for (const auto& kv : r.headers)
        if (kv.first == QByteArray(name)) return kv.second;
    return {};
}

// One request (no redirects). dnsMs measured with QHostInfo for host names.
static HttpResult httpOnce(const QUrl& u, const QByteArray& method,
                           const QByteArray& extraHeaders, int timeoutMs) {
    HttpResult r;
    QElapsedTimer total; total.start();
    QElapsedTimer phase; phase.start();

    // Phase: DNS (only for hostnames) — DnsResolver 3s 超时单例（H4：裸 QHostInfo
    // 在坏网络下可阻塞数十秒，远超 watchdog 预算）
    if (u.host().contains(QLatin1Char('.')) && !u.host().startsWith(QLatin1String("192."))
        && !u.host().startsWith(QLatin1String("10."))
        && !u.host().startsWith(QLatin1String("198.18."))
        && !u.host().startsWith(QLatin1String("172."))
        && !u.host().startsWith(QLatin1String("127."))) {
        const QString ip = DnsResolver::instance().resolve(u.host(), 3000);
        r.dnsMs = phase.restart();
        if (ip.isEmpty()) {
            r.error = QStringLiteral("DNS resolution failed (3s timeout)");
            r.totalMs = total.elapsed();
            return r;
        }
    } else {
        r.dnsMs = 0;
    }

    const int port = portForUrl(u);
    const bool https = u.scheme().toLower() == QLatin1String("https");

    if (https) {
        QSslSocket sock;
        sock.setPeerVerifyMode(QSslSocket::VerifyNone);
        // M6：TCP 连接与 TLS 握手分段计时——先 connectToHost，再 startClientEncryption
        sock.connectToHost(u.host(), (quint16)port);
        if (!sock.waitForConnected(timeoutMs)) {
            r.error = sock.errorString();
            r.totalMs = total.elapsed();
            return r;
        }
        r.connectMs = phase.restart();
        sock.startClientEncryption();
        if (!sock.waitForEncrypted(timeoutMs)) {
            r.error = sock.errorString();
            r.totalMs = total.elapsed();
            return r;
        }
        r.tlsMs = phase.elapsed();

        QByteArray req;
        req += method + " " + (u.path(QUrl::FullyEncoded).isEmpty() ? QByteArray("/") : u.path(QUrl::FullyEncoded).toUtf8());
        if (u.hasQuery()) { req += '?'; req += u.query(QUrl::FullyEncoded).toUtf8(); }
        req += " HTTP/1.1\r\n";
        req += "Host: " + u.host().toUtf8() + "\r\n";
        req += "User-Agent: NetDiagnostics/1.0\r\n";
        req += "Connection: close\r\n";
        if (!extraHeaders.isEmpty()) req += extraHeaders;
        req += "\r\n";
        sock.write(req);
        r.verboseLines.append(QStringLiteral("> %1 %2 HTTP/1.1").arg(QString::fromUtf8(method), u.path()));
        QByteArray all;
        while (total.elapsed() < timeoutMs) {
            if (!sock.waitForReadyRead(qMin<qint64>(300, timeoutMs - total.elapsed()))) break;
            all += sock.readAll();
            if (all.contains("\r\n\r\n")) { r.firstByteMs = total.elapsed(); break; }
        }
        // drain remaining body
        while (total.elapsed() < timeoutMs) {
            if (!sock.waitForReadyRead(qMin<qint64>(300, timeoutMs - total.elapsed()))) break;
            all += sock.readAll();
        }
        sock.disconnectFromHost();
        r.totalMs = total.elapsed();
        const int hdrEnd = all.indexOf("\r\n\r\n");
        if (hdrEnd < 0) {
            r.error = QStringLiteral("No HTTP response");
            return r;
        }
        if (!parseResponseHead(all.left(hdrEnd), r)) {
            r.error = QStringLiteral("Malformed HTTP response");
            return r;
        }
        r.body = all.mid(hdrEnd + 4);
        r.verboseLines.append(QStringLiteral("< %1").arg(QString::fromLatin1(r.statusLine)));
        for (const auto& kv : r.headers)
            r.verboseLines.append(QStringLiteral("< %1: %2").arg(QString::fromLatin1(kv.first), QString::fromLatin1(kv.second)));
        r.ok = true;
        return r;
    }

    // plain HTTP
    QTcpSocket sock;
    sock.connectToHost(u.host(), (quint16)port);
    if (!sock.waitForConnected(timeoutMs)) {
        r.error = sock.errorString();
        r.totalMs = total.elapsed();
        return r;
    }
    r.connectMs = phase.elapsed();
    QByteArray req;
    req += method + " " + (u.path(QUrl::FullyEncoded).isEmpty() ? QByteArray("/") : u.path(QUrl::FullyEncoded).toUtf8());
    if (u.hasQuery()) { req += '?'; req += u.query(QUrl::FullyEncoded).toUtf8(); }
    req += " HTTP/1.1\r\n";
    req += "Host: " + u.host().toUtf8() + "\r\n";
    req += "User-Agent: NetDiagnostics/1.0\r\n";
    req += "Connection: close\r\n";
    if (!extraHeaders.isEmpty()) req += extraHeaders;
    req += "\r\n";
    sock.write(req);
    r.verboseLines.append(QStringLiteral("> %1 %2 HTTP/1.1").arg(QString::fromUtf8(method), u.path()));
    QByteArray all;
    while (total.elapsed() < timeoutMs) {
        if (!sock.waitForReadyRead(qMin<qint64>(300, timeoutMs - total.elapsed()))) break;
        all += sock.readAll();
        if (all.contains("\r\n\r\n")) { r.firstByteMs = total.elapsed(); break; }
    }
    while (total.elapsed() < timeoutMs) {
        if (!sock.waitForReadyRead(qMin<qint64>(300, timeoutMs - total.elapsed()))) break;
        all += sock.readAll();
    }
    sock.disconnectFromHost();
    r.totalMs = total.elapsed();
    const int hdrEnd = all.indexOf("\r\n\r\n");
    if (hdrEnd < 0) {
        r.error = QStringLiteral("No HTTP response");
        return r;
    }
    if (!parseResponseHead(all.left(hdrEnd), r)) {
        r.error = QStringLiteral("Malformed HTTP response");
        return r;
    }
    r.body = all.mid(hdrEnd + 4);
    r.verboseLines.append(QStringLiteral("< %1").arg(QString::fromLatin1(r.statusLine)));
    for (const auto& kv : r.headers)
        r.verboseLines.append(QStringLiteral("< %1: %2").arg(QString::fromLatin1(kv.first), QString::fromLatin1(kv.second)));
    r.ok = true;
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G5UrlParsing
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeUrlParsing(DiagId id, const QString& target, RunContext&) {
    if (target.isEmpty()) return skippedProbe(id, QStringLiteral("No target"));
    const QUrl u = normalizeUrl(target);
    if (u.isEmpty()) return makeResult(id, DiagStatus::Fail, QStringLiteral("Invalid URL"), {}, {});
    const QString details = QStringLiteral("Scheme: %1\nHost: %2\nPort: %3\nPath: %4\nQuery: %5")
        .arg(u.scheme(), u.host()).arg(portForUrl(u)).arg(u.path(), u.query());
    DiagnosticResult r = makeResult(id, DiagStatus::Pass,
        QStringLiteral("Scheme=%1 Host=%2 Port=%3").arg(u.scheme(), u.host()).arg(portForUrl(u)),
        {}, details);
    r.data[QStringLiteral("scheme")] = u.scheme();
    r.data[QStringLiteral("host")] = u.host();
    r.data[QStringLiteral("port")] = portForUrl(u);
    r.data[QStringLiteral("path")] = u.path();
    r.data[QStringLiteral("query")] = u.query();
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G5TcpConnect
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeTcpConnect(DiagId id, const QString& target, RunContext&) {
    if (target.isEmpty()) return skippedProbe(id, QStringLiteral("No target"));
    const QUrl u = normalizeUrl(target);
    if (u.isEmpty()) return makeResult(id, DiagStatus::Fail, QStringLiteral("Invalid target"), {}, {});
    QElapsedTimer t; t.start();
    QTcpSocket sock;
    const int port = portForUrl(u);
    sock.connectToHost(u.host(), (quint16)port);
    const bool ok = sock.waitForConnected(5000);
    const qint64 ms = t.elapsed();
    sock.disconnectFromHost();
    DiagnosticResult r = makeResult(id, ok ? DiagStatus::Pass : DiagStatus::Fail,
        ok ? QStringLiteral("Connected in %1ms").arg(ms)
           : QStringLiteral("Failed: %1").arg(sock.errorString()), {}, {});
    r.data[QStringLiteral("host")] = u.host();
    r.data[QStringLiteral("port")] = port;
    r.data[QStringLiteral("connected")] = ok;
    r.data[QStringLiteral("latencyMs")] = ms;
    if (!ok) r.errorOutput = QStringLiteral("TCP connect to %1:%2 failed: %3")
        .arg(u.host()).arg(port).arg(sock.errorString());
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G5ServiceBanner
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeServiceBanner(DiagId id, const QString& target, RunContext&) {
    if (target.isEmpty()) return skippedProbe(id, QStringLiteral("No target"));
    const QUrl u = normalizeUrl(target);
    if (u.isEmpty()) return makeResult(id, DiagStatus::Fail, QStringLiteral("Invalid target"), {}, {});
    const ProbeOutcome p = tcpProbe(u, {}, 5000, 2000);
    DiagnosticResult r = probeResultScaffold(id, u, p);
    if (!p.connected) return r;
    const QString banner = QString::fromUtf8(p.banner).left(500);
    r.summary = p.banner.isEmpty() ? QStringLiteral("No banner received")
                                   : QStringLiteral("Banner received");
    r.status = p.banner.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass;
    r.data[QStringLiteral("banner")] = banner;
    r.data[QStringLiteral("bannerLength")] = p.banner.size();
    if (r.status != DiagStatus::Pass && r.errorOutput.isEmpty()) r.errorOutput = r.summary;
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G5CurlVerbose — full request/response dump + timing waterfall
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeCurlVerbose(DiagId id, const QString& target, RunContext&) {
    if (target.isEmpty()) return skippedProbe(id, QStringLiteral("No target"));
    const QUrl u = normalizeUrl(target);
    if (u.isEmpty()) return makeResult(id, DiagStatus::Fail, QStringLiteral("Invalid target"), {}, {});
    const HttpResult hr = httpOnce(u, QByteArrayLiteral("GET"), QByteArray(), 15000);
    if (!hr.ok) return makeResult(id, DiagStatus::Fail,
        hr.error.isEmpty() ? QStringLiteral("HTTP request failed") : hr.error, {}, {});

    QStringList out;
    for (const auto& l : hr.verboseLines) out.append(l);
    out.append(QString());
    out.append(QStringLiteral("* Timing breakdown:"));
    out.append(QStringLiteral("  DNS:        %1 ms").arg(hr.dnsMs));
    out.append(QStringLiteral("  Connect:    %1 ms").arg(hr.connectMs));
    out.append(QStringLiteral("  TLS:        %1 ms").arg(hr.tlsMs));
    out.append(QStringLiteral("  First byte: %1 ms").arg(hr.firstByteMs));
    out.append(QStringLiteral("  Total:      %1 ms").arg(hr.totalMs));
    if (!hr.body.isEmpty()) {
        out.append(QString());
        out.append(QStringLiteral("* Body: %1 bytes").arg(hr.body.size()));
        const QByteArray preview = hr.body.left(500);
        for (const auto& line : QString::fromUtf8(preview).split(QLatin1Char('\n')))
            if (!line.trimmed().isEmpty()) out.append(QStringLiteral("  %1").arg(line.left(120)));
        if (hr.body.size() > 500)
            out.append(QStringLiteral("  ... (%1 more bytes)").arg(hr.body.size() - 500));
    }

    DiagnosticResult r = makeResult(id, DiagStatus::Pass,
        QStringLiteral("HTTP %1 — %2 ms").arg(hr.statusCode).arg(hr.totalMs), {}, out.join(QLatin1Char('\n')));
    r.data[QStringLiteral("statusCode")] = hr.statusCode;
    r.data[QStringLiteral("totalMs")] = hr.totalMs;
    r.data[QStringLiteral("dnsMs")] = hr.dnsMs;
    r.data[QStringLiteral("connectMs")] = hr.connectMs;
    r.data[QStringLiteral("sslMs")] = hr.tlsMs;   // diag-g5 §2.4：契约键名 sslMs
    r.data[QStringLiteral("firstByteMs")] = hr.firstByteMs;
    QVariantList waterfall;   // 5 段（diag-g5 §2.4：DNS/Connect/SSL/FirstByte/Total）
    waterfall.append(QVariantMap{{QStringLiteral("phase"), QStringLiteral("DNS")}, {QStringLiteral("ms"), (double)hr.dnsMs}});
    waterfall.append(QVariantMap{{QStringLiteral("phase"), QStringLiteral("Connect")}, {QStringLiteral("ms"), (double)hr.connectMs}});
    waterfall.append(QVariantMap{{QStringLiteral("phase"), QStringLiteral("SSL")}, {QStringLiteral("ms"), (double)hr.tlsMs}});
    waterfall.append(QVariantMap{{QStringLiteral("phase"), QStringLiteral("FirstByte")}, {QStringLiteral("ms"), (double)hr.firstByteMs}});
    waterfall.append(QVariantMap{{QStringLiteral("phase"), QStringLiteral("Total")}, {QStringLiteral("ms"), (double)hr.totalMs}});
    r.data[QStringLiteral("waterfall")] = waterfall;
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G5HttpHeaders
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeHttpHeaders(DiagId id, const QString& target, RunContext&) {
    if (target.isEmpty()) return skippedProbe(id, QStringLiteral("No target"));
    const QUrl u = normalizeUrl(target);
    if (u.isEmpty()) return makeResult(id, DiagStatus::Fail, QStringLiteral("Invalid target"), {}, {});
    const HttpResult hr = httpOnce(u, QByteArrayLiteral("GET"), QByteArray(), 12000);
    if (!hr.ok) return makeResult(id, DiagStatus::Fail,
        hr.error.isEmpty() ? QStringLiteral("HTTP request failed") : hr.error, {}, {});
    QStringList out;
    out.append(QStringLiteral("HTTP/1.1 response headers for %1:").arg(u.toString()));
    out.append(QStringLiteral("  %1").arg(QString::fromLatin1(hr.statusLine)));
    for (const auto& kv : hr.headers)
        out.append(QStringLiteral("  %1: %2").arg(QString::fromLatin1(kv.first), QString::fromLatin1(kv.second)));
    DiagnosticResult r = makeResult(id, DiagStatus::Pass,
        QStringLiteral("HTTP %1 — %2 headers").arg(hr.statusCode).arg(hr.headers.size()),
        {}, out.join(QLatin1Char('\n')));
    r.data[QStringLiteral("statusCode")] = hr.statusCode;
    r.data[QStringLiteral("headerCount")] = hr.headers.size();
    QVariantList headers;
    for (const auto& kv : hr.headers) {
        QVariantMap m;
        m[QStringLiteral("name")] = QString::fromLatin1(kv.first);
        m[QStringLiteral("value")] = QString::fromLatin1(kv.second);
        headers.append(m);
    }
    r.data[QStringLiteral("headers")] = headers;
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G5SecurityHeaders
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeSecurityHeaders(DiagId id, const QString& target, RunContext&) {
    if (target.isEmpty()) return skippedProbe(id, QStringLiteral("No target"));
    const QUrl u = normalizeUrl(target);
    if (u.isEmpty()) return makeResult(id, DiagStatus::Fail, QStringLiteral("Invalid target"), {}, {});
    const HttpResult hr = httpOnce(u, QByteArrayLiteral("GET"), QByteArray(), 15000);
    if (!hr.ok) return makeResult(id, DiagStatus::Fail,
        hr.error.isEmpty() ? QStringLiteral("HTTP request failed") : hr.error, {}, {});

    static const char* kRequired[] = {
        "strict-transport-security", "content-security-policy", "x-frame-options",
        "x-content-type-options", "x-xss-protection", "referrer-policy", "permissions-policy",
    };
    QStringList found, missing;
    for (const char* req : kRequired) {
        bool present = false;
        for (const auto& kv : hr.headers)
            if (kv.first == QByteArray(req)) { present = true; break; }
        if (present) found.append(QLatin1String(req));
        else missing.append(QLatin1String(req));
    }
    QStringList out;
    out.append(QStringLiteral("Security Header Analysis:"));
    out.append(QStringLiteral("  %1  %2").arg(QStringLiteral("Header"), -30).arg(QStringLiteral("Status")));
    out.append(QStringLiteral("  %1  %2").arg(QString(30, QLatin1Char('-')), QString(10, QLatin1Char('-'))));
    for (const char* req : kRequired) {
        const bool present = found.contains(QLatin1String(req));
        out.append(QStringLiteral("  %1  %2").arg(QLatin1String(req), -30)
            .arg(present ? QStringLiteral("✓ Present") : QStringLiteral("✗ Missing")));
    }
    out.append(QString());
    out.append(QStringLiteral("  Result: %1 of 7 security headers present").arg(found.size()));

    const int score = found.size();
    const DiagStatus status = missing.isEmpty() ? DiagStatus::Pass
        : missing.size() <= 4 ? DiagStatus::Warning : DiagStatus::Fail;
    DiagnosticResult r = makeResult(id, status,
        missing.isEmpty() ? QStringLiteral("All 7 present")
                          : QStringLiteral("%1 missing").arg(missing.size()),
        {}, out.join(QLatin1Char('\n')));
    r.data[QStringLiteral("presentHeaders")] = found;
    r.data[QStringLiteral("missingHeaders")] = missing;
    r.data[QStringLiteral("score")] = score;
    r.data[QStringLiteral("totalRequired")] = 7;
    r.data[QStringLiteral("statusCode")] = hr.statusCode;
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G5SslCertificate
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeSslCertificate(DiagId id, const QString& target, RunContext&) {
    if (target.isEmpty()) return skippedProbe(id, QStringLiteral("No target"));
    const QUrl u = normalizeUrl(target);
    if (u.isEmpty()) return makeResult(id, DiagStatus::Fail, QStringLiteral("Invalid target"), {}, {});
    QSslSocket sock;
    sock.setPeerVerifyMode(QSslSocket::VerifyNone);
    QElapsedTimer t; t.start();
    sock.connectToHostEncrypted(u.host(), (quint16)portForUrl(u));
    if (!sock.waitForEncrypted(10000)) {
        return makeResult(id, DiagStatus::Fail,
            QStringLiteral("TLS handshake failed: %1").arg(sock.errorString()), {}, {});
    }
    const auto chain = sock.peerCertificateChain();
    sock.disconnectFromHost();
    if (chain.isEmpty())
        return makeResult(id, DiagStatus::Warning, QStringLiteral("No certificate presented"), {}, {});

    QStringList out;
    int certIdx = 0;
    for (const auto& cert : chain) {
        const QString cn = cert.subjectInfo(QSslCertificate::CommonName).value(0);
        const QString issuer = cert.issuerInfo(QSslCertificate::CommonName).value(0);
        const auto sans = cert.subjectAlternativeNames().values();
        const QDateTime notBefore = cert.effectiveDate();
        const QDateTime notAfter = cert.expiryDate();
        const qint64 daysLeft = QDateTime::currentDateTime().daysTo(notAfter);
        out.append(QStringLiteral("Certificate #%1:").arg(++certIdx));
        out.append(QStringLiteral("  CN:        %1").arg(cn));
        out.append(QStringLiteral("  Issuer:    %1").arg(issuer));
        out.append(QStringLiteral("  SANs:      %1").arg(sans.join(QStringLiteral(", "))));
        out.append(QStringLiteral("  Valid:     %1 → %2").arg(notBefore.toString(Qt::ISODate), notAfter.toString(Qt::ISODate)));
        out.append(QStringLiteral("  Days left: %1").arg(daysLeft));
        out.append(QStringLiteral("  Serial:    %1").arg(QString::fromLatin1(cert.serialNumber().toHex())));
        out.append(QString());
        if (certIdx == 1) {
            const QSslCertificate& leaf = cert;
            const qint64 dl = QDateTime::currentDateTime().daysTo(leaf.expiryDate());
            const bool expired = dl < 0;
            const bool soonExpiring = dl >= 0 && dl <= 30;
            DiagnosticResult r = makeResult(id, expired ? DiagStatus::Fail
                : soonExpiring ? DiagStatus::Warning : DiagStatus::Pass,
                expired ? QStringLiteral("Certificate EXPIRED %1 days ago").arg(-dl)
                : soonExpiring ? QStringLiteral("Expires in %1 days").arg(dl)
                : QStringLiteral("Valid for %1 days").arg(dl), {}, out.join(QLatin1Char('\n')));
            // diag-g5 §2.7 契约键：daysLeft/issuer/validFrom/validTo/subject
            r.data[QStringLiteral("daysLeft")] = dl;
            r.data[QStringLiteral("issuer")] = issuer;
            r.data[QStringLiteral("subject")] = cn;
            r.data[QStringLiteral("validFrom")] = notBefore;
            r.data[QStringLiteral("validTo")] = notAfter;
            r.data[QStringLiteral("sans")] = sans;
            r.data[QStringLiteral("chainLength")] = chain.size();
            r.data[QStringLiteral("handshakeMs")] = t.elapsed();
            // 归档 NetworkProbe::sslCertInfo 的 SHA-256 指纹
            r.data[QStringLiteral("thumbprint")] = QString::fromLatin1(
                cert.digest(QCryptographicHash::Sha256).toHex());
            return r;
        }
    }
    return makeResult(id, DiagStatus::Warning, QStringLiteral("No leaf certificate"), {}, out.join(QLatin1Char('\n')));
}

// ═════════════════════════════════════════════════════════════════════════
// G5HttpRedirect
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeHttpRedirect(DiagId id, const QString& target, RunContext& ctx) {
    if (target.isEmpty()) return skippedProbe(id, QStringLiteral("No target"));
    QUrl u = normalizeUrl(target);
    if (u.isEmpty()) return makeResult(id, DiagStatus::Fail, QStringLiteral("Invalid target"), {}, {});
    QStringList out;
    out.append(QStringLiteral("Redirect chain for %1:").arg(u.toString()));
    int redirectCount = 0;
    bool finalOk = false;
    HttpResult last;
    QVariantList hops;
    for (int hop = 0; hop <= 5; ++hop) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        // L2：每跳 8s（6×8=48s < 60s watchdog 预算）
        const HttpResult hr = httpOnce(u, QByteArrayLiteral("GET"), QByteArray(), 8000);
        last = hr;
        if (!hr.ok) break;
        QVariantMap hm;
        hm[QStringLiteral("url")] = u.toString();
        hm[QStringLiteral("statusCode")] = hr.statusCode;
        hm[QStringLiteral("location")] = hr.redirectLocation;
        hops.append(hm);
        out.append(QStringLiteral("  → %1 [HTTP %2]").arg(u.toString()).arg(hr.statusCode));
        if (hr.statusCode >= 300 && hr.statusCode < 400 && !hr.redirectLocation.isEmpty()) {
            out.append(QStringLiteral("    Location: %1").arg(hr.redirectLocation));
            u = u.resolved(QUrl(hr.redirectLocation));
            ++redirectCount;
            if (u.scheme().isEmpty() || u.host().isEmpty()) break;
            continue;
        }
        finalOk = (hr.statusCode >= 200 && hr.statusCode < 400);
        break;
    }
    if (!last.ok) {
        return makeResult(id, DiagStatus::Fail,
            last.error.isEmpty() ? QStringLiteral("HTTP request failed") : last.error,
            {}, out.join(QLatin1Char('\n')));
    }
    if (!finalOk) {
        out.append(QStringLiteral("Final status %1 is an error").arg(last.statusCode));
        DiagnosticResult r = makeResult(id, DiagStatus::Warning,
            QStringLiteral("%1 redirect(s), final HTTP %2").arg(redirectCount).arg(last.statusCode),
            {}, out.join(QLatin1Char('\n')));
        r.data[QStringLiteral("redirectCount")] = redirectCount;
        r.data[QStringLiteral("redirects")] = hops;   // diag-g5 §2.8 契约键
        r.data[QStringLiteral("finalStatus")] = last.statusCode;
        return r;
    }
    DiagnosticResult r = makeResult(id, DiagStatus::Pass,
        redirectCount > 0 ? QStringLiteral("%1 redirect(s), final HTTP %2").arg(redirectCount).arg(last.statusCode)
                          : QStringLiteral("No redirect — HTTP %1").arg(last.statusCode),
        {}, out.join(QLatin1Char('\n')));
    r.data[QStringLiteral("redirectCount")] = redirectCount;
    r.data[QStringLiteral("redirects")] = hops;   // diag-g5 §2.8 契约键
    r.data[QStringLiteral("finalStatus")] = last.statusCode;
    r.data[QStringLiteral("finalUrl")] = u.toString();
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G5HttpCompression
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeHttpCompression(DiagId id, const QString& target, RunContext&) {
    if (target.isEmpty()) return skippedProbe(id, QStringLiteral("No target"));
    const QUrl u = normalizeUrl(target);
    if (u.isEmpty()) return makeResult(id, DiagStatus::Fail, QStringLiteral("Invalid target"), {}, {});
    const HttpResult hr = httpOnce(u, QByteArrayLiteral("GET"),
        QByteArrayLiteral("Accept-Encoding: gzip, deflate, br\r\n"), 15000);
    if (!hr.ok) return makeResult(id, DiagStatus::Fail,
        hr.error.isEmpty() ? QStringLiteral("HTTP request failed") : hr.error, {}, {});
    const QByteArray encoding = headerValue(hr, "content-encoding");
    // M7：按规格补 originalSize/compressedSize/ratio——identity 对照请求测实体比
    const HttpResult hrIdentity = httpOnce(u, QByteArrayLiteral("GET"),
        QByteArrayLiteral("Accept-Encoding: identity\r\n"), 15000);
    const int originalSize = hrIdentity.ok ? hrIdentity.body.size() : 0;
    const int compressedSize = hr.body.size();
    const double ratio = (originalSize > 0)
        ? 100.0 * (1.0 - double(compressedSize) / double(originalSize)) : 0.0;
    QStringList out;
    out.append(QStringLiteral("Compression negotiation (Accept-Encoding: gzip, deflate, br):"));
    out.append(QStringLiteral("  HTTP %1").arg(hr.statusCode));
    out.append(QStringLiteral("  Content-Encoding: %1").arg(encoding.isEmpty() ? QStringLiteral("(none)") : QString::fromLatin1(encoding)));
    out.append(QStringLiteral("  Body bytes received: %1").arg(hr.body.size()));
    out.append(QStringLiteral("  Identity body bytes: %1").arg(originalSize));
    out.append(QStringLiteral("  Size ratio: %1%").arg(ratio, 0, 'f', 1));
    const bool supported = !encoding.isEmpty() && encoding != QByteArrayLiteral("identity");
    DiagnosticResult r = makeResult(id, supported ? DiagStatus::Pass : DiagStatus::Info,
        supported ? QStringLiteral("%1 enabled").arg(QString::fromLatin1(encoding))
                  : QStringLiteral("No compression negotiated"),
        {}, out.join(QLatin1Char('\n')));
    r.data[QStringLiteral("contentEncoding")] = QString::fromLatin1(encoding);
    r.data[QStringLiteral("supported")] = supported;
    r.data[QStringLiteral("bodyBytes")] = hr.body.size();
    r.data[QStringLiteral("totalMs")] = hr.totalMs;
    r.data[QStringLiteral("originalSize")] = originalSize;
    r.data[QStringLiteral("compressedSize")] = compressedSize;
    r.data[QStringLiteral("ratio")] = ratio;
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G5HttpTiming
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeHttpTiming(DiagId id, const QString& target, RunContext&) {
    if (target.isEmpty()) return skippedProbe(id, QStringLiteral("No target"));
    const QUrl u = normalizeUrl(target);
    if (u.isEmpty()) return makeResult(id, DiagStatus::Fail, QStringLiteral("Invalid target"), {}, {});
    const HttpResult hr = httpOnce(u, QByteArrayLiteral("GET"), QByteArray(), 15000);
    if (!hr.ok) return makeResult(id, DiagStatus::Fail,
        hr.error.isEmpty() ? QStringLiteral("HTTP request failed") : hr.error, {}, {});
    QStringList out;
    out.append(QStringLiteral("HTTP timing breakdown (%1):").arg(u.toString()));
    out.append(QStringLiteral("  DNS lookup:   %1 ms").arg(hr.dnsMs));
    out.append(QStringLiteral("  TCP connect:  %1 ms").arg(hr.connectMs));
    out.append(QStringLiteral("  TLS handshake:%1 ms").arg(hr.tlsMs));
    out.append(QStringLiteral("  First byte:   %1 ms").arg(hr.firstByteMs));
    out.append(QStringLiteral("  Total:        %1 ms").arg(hr.totalMs));
    DiagnosticResult r = makeResult(id, DiagStatus::Pass,
        QStringLiteral("TTFB %1 ms, total %2 ms").arg(hr.firstByteMs).arg(hr.totalMs),
        {}, out.join(QLatin1Char('\n')));
    r.data[QStringLiteral("dnsMs")] = hr.dnsMs;
    r.data[QStringLiteral("connectMs")] = hr.connectMs;
    r.data[QStringLiteral("sslMs")] = hr.tlsMs;   // diag-g5 §2.10：契约键名 sslMs
    r.data[QStringLiteral("firstByteMs")] = hr.firstByteMs;
    r.data[QStringLiteral("totalMs")] = hr.totalMs;
    r.data[QStringLiteral("statusCode")] = hr.statusCode;
    QVariantList waterfall;   // 5 段（diag-g5 §2.10）
    waterfall.append(QVariantMap{{QStringLiteral("phase"), QStringLiteral("DNS")}, {QStringLiteral("ms"), (double)hr.dnsMs}});
    waterfall.append(QVariantMap{{QStringLiteral("phase"), QStringLiteral("Connect")}, {QStringLiteral("ms"), (double)hr.connectMs}});
    waterfall.append(QVariantMap{{QStringLiteral("phase"), QStringLiteral("SSL")}, {QStringLiteral("ms"), (double)hr.tlsMs}});
    waterfall.append(QVariantMap{{QStringLiteral("phase"), QStringLiteral("FirstByte")}, {QStringLiteral("ms"), (double)hr.firstByteMs}});
    waterfall.append(QVariantMap{{QStringLiteral("phase"), QStringLiteral("Total")}, {QStringLiteral("ms"), (double)hr.totalMs}});
    r.data[QStringLiteral("waterfall")] = waterfall;
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// Protocol family — banner / handshake verification
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeFtp(DiagId id, const QString& target, RunContext&) {
    if (target.isEmpty()) return skippedProbe(id, QStringLiteral("No target"));
    const QUrl u = normalizeUrl(target);
    if (u.isEmpty()) return makeResult(id, DiagStatus::Fail, QStringLiteral("Invalid target"), {}, {});
    if (u.scheme().toLower() != QLatin1String("ftp") && u.scheme().toLower() != QLatin1String("ftps"))
        return skippedProbe(id, QStringLiteral("Not FTP"));
    const ProbeOutcome p = tcpProbe(u, {});
    DiagnosticResult r = probeResultScaffold(id, u, p);
    if (!p.connected) return r;
    const QString banner = QString::fromUtf8(p.banner).trimmed().left(200);
    r.data[QStringLiteral("banner")] = banner;
    r.summary = banner.isEmpty() ? QStringLiteral("No banner") : banner;
    r.status = banner.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass;
    if (r.status != DiagStatus::Pass && r.errorOutput.isEmpty()) r.errorOutput = r.summary;
    return r;
}

static DiagnosticResult probeSsh(DiagId id, const QString& target, RunContext&) {
    if (target.isEmpty()) return skippedProbe(id, QStringLiteral("No target"));
    const QUrl u = normalizeUrl(target);
    if (u.isEmpty()) return makeResult(id, DiagStatus::Fail, QStringLiteral("Invalid target"), {}, {});
    if (u.scheme().toLower() != QLatin1String("ssh") && u.scheme().toLower() != QLatin1String("sftp"))
        return skippedProbe(id, QStringLiteral("Not SSH"));
    const ProbeOutcome p = tcpProbe(u, {});
    DiagnosticResult r = probeResultScaffold(id, u, p);
    if (!p.connected) return r;
    const QString banner = QString::fromUtf8(p.banner).trimmed().left(200);
    const QString version = banner.startsWith(QLatin1String("SSH-"))
        ? banner.section(QLatin1Char(' '), 0, 0) : QString();
    r.data[QStringLiteral("sshVersion")] = version;
    r.data[QStringLiteral("banner")] = banner;
    r.summary = version.isEmpty() ? QStringLiteral("No SSH banner") : version;
    r.status = version.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass;
    if (r.status != DiagStatus::Pass && r.errorOutput.isEmpty()) r.errorOutput = r.summary;
    return r;
}

static DiagnosticResult probeEmail(DiagId id, const QString& target, RunContext&) {
    if (target.isEmpty()) return skippedProbe(id, QStringLiteral("No target"));
    const QUrl u = normalizeUrl(target);
    if (u.isEmpty()) return makeResult(id, DiagStatus::Fail, QStringLiteral("Invalid target"), {}, {});
    const QString scheme = u.scheme().toLower();
    if (scheme != QLatin1String("smtp") && scheme != QLatin1String("imap") && scheme != QLatin1String("pop3")
        && scheme != QLatin1String("smtps") && scheme != QLatin1String("imaps") && scheme != QLatin1String("pop3s"))
        return skippedProbe(id, QStringLiteral("Not email protocol (smtp/smtps/imap/imaps/pop3/pop3s)"));
    const ProbeOutcome p = tcpProbe(u, {});
    DiagnosticResult r = probeResultScaffold(id, u, p);
    r.data[QStringLiteral("protocol")] = scheme;
    if (!p.connected) return r;
    const QString banner = QString::fromUtf8(p.banner).trimmed().left(200);
    r.data[QStringLiteral("banner")] = banner;
    r.summary = banner.isEmpty() ? QStringLiteral("No banner") : banner;
    r.status = banner.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass;
    if (r.status != DiagStatus::Pass && r.errorOutput.isEmpty()) r.errorOutput = r.summary;
    return r;
}

static DiagnosticResult probeTelnet(DiagId id, const QString& target, RunContext&) {
    if (target.isEmpty()) return skippedProbe(id, QStringLiteral("No target"));
    const QUrl u = normalizeUrl(target);
    if (u.isEmpty()) return makeResult(id, DiagStatus::Fail, QStringLiteral("Invalid target"), {}, {});
    if (u.scheme().toLower() != QLatin1String("telnet"))
        return skippedProbe(id, QStringLiteral("Not Telnet"));
    const ProbeOutcome p = tcpProbe(u, {}, 5000, 2000);
    DiagnosticResult r = probeResultScaffold(id, u, p);
    if (!p.connected) return r;
    const QString banner = QString::fromUtf8(p.banner).trimmed().left(200);
    r.data[QStringLiteral("banner")] = banner;
    r.summary = banner.isEmpty() ? QStringLiteral("Connected (no banner)") : banner;
    r.status = banner.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass;
    if (r.status != DiagStatus::Pass && r.errorOutput.isEmpty()) r.errorOutput = r.summary;
    return r;
}

static DiagnosticResult probeMysql(DiagId id, const QString& target, RunContext&) {
    if (target.isEmpty()) return skippedProbe(id, QStringLiteral("No target"));
    const QUrl u = normalizeUrl(target);
    if (u.isEmpty()) return makeResult(id, DiagStatus::Fail, QStringLiteral("Invalid target"), {}, {});
    if (u.scheme().toLower() != QLatin1String("mysql"))
        return skippedProbe(id, QStringLiteral("Not MySQL"));
    const ProbeOutcome p = tcpProbe(u, {}, 5000, 2000);
    DiagnosticResult r = probeResultScaffold(id, u, p);
    if (!p.connected) return r;
    const QByteArray& data = p.banner;
    if (data.size() < 5) {
        r.summary = QStringLiteral("No handshake packet");
        r.status = DiagStatus::Warning;
        r.data[QStringLiteral("version")] = QString();
        r.data[QStringLiteral("protocolVersion")] = 0;
        if (r.errorOutput.isEmpty()) r.errorOutput = r.summary;
        return r;
    }
    const int verStart = 5;
    const int verEnd = data.indexOf('\0', verStart);
    const QString version = (verEnd > verStart)
        ? QString::fromUtf8(data.mid(verStart, verEnd - verStart)) : QString();
    r.summary = version.isEmpty() ? QStringLiteral("MySQL (version unknown)")
                                  : QStringLiteral("MySQL %1").arg(version);
    r.status = version.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass;
    r.rawOutput = r.details = QString::fromUtf8(data.toHex(' '));
    r.data[QStringLiteral("version")] = version;
    r.data[QStringLiteral("protocolVersion")] = (int)(quint8)data.at(4);
    if (r.status != DiagStatus::Pass && r.errorOutput.isEmpty()) r.errorOutput = r.summary;
    return r;
}

static DiagnosticResult probePostgres(DiagId id, const QString& target, RunContext&) {
    if (target.isEmpty()) return skippedProbe(id, QStringLiteral("No target"));
    const QUrl u = normalizeUrl(target);
    if (u.isEmpty()) return makeResult(id, DiagStatus::Fail, QStringLiteral("Invalid target"), {}, {});
    if (u.scheme().toLower() != QLatin1String("postgresql"))
        return skippedProbe(id, QStringLiteral("Not PostgreSQL"));
    // StartupMessage (protocol 3.0, user "diagnostic")
    QByteArray startup;
    startup.append(char(0x00)); startup.append(char(0x00));
    startup.append(char(0x03)); startup.append(char(0x00));
    startup.append("user"); startup.append('\0');
    startup.append("diagnostic"); startup.append('\0');
    startup.append('\0');
    QByteArray packet;
    const quint32 len = startup.size() + 4;
    packet.append(char((len >> 24) & 0xFF));
    packet.append(char((len >> 16) & 0xFF));
    packet.append(char((len >> 8) & 0xFF));
    packet.append(char(len & 0xFF));
    packet.append(startup);
    const ProbeOutcome p = tcpProbe(u, packet, 5000, 3000);
    DiagnosticResult r = probeResultScaffold(id, u, p);
    // 失败结果键齐备（归档契约：消费者绝不读到 undefined key）
    r.data[QStringLiteral("responseType")] = QString();
    r.data[QStringLiteral("authOk")] = false;
    if (!p.connected) return r;
    const QByteArray& resp = p.banner;
    if (resp.isEmpty()) {
        r.summary = QStringLiteral("No response");
        r.status = DiagStatus::Warning;
        r.data[QStringLiteral("responseType")] = QString();
        r.data[QStringLiteral("authOk")] = false;
        if (r.errorOutput.isEmpty()) r.errorOutput = r.summary;
        return r;
    }
    const char type = resp.at(0);
    QString info;
    switch (type) {
        case 'R': info = QStringLiteral("Authentication request"); break;
        case 'E': info = QStringLiteral("Error response"); break;
        case 'N': info = QStringLiteral("Notice"); break;
        case 'S': info = QStringLiteral("Parameter status"); break;
        default:  info = QStringLiteral("Response type '%1'").arg(type); break;
    }
    r.summary = QStringLiteral("PostgreSQL: %1").arg(info);
    r.status = (type == 'R') ? DiagStatus::Pass : DiagStatus::Warning;
    r.rawOutput = r.details = QString::fromUtf8(resp.toHex(' '));
    r.data[QStringLiteral("responseType")] = QString(QChar(type));
    r.data[QStringLiteral("responseInfo")] = info;
    r.data[QStringLiteral("authOk")] = (type == 'R');
    if (r.status != DiagStatus::Pass && r.errorOutput.isEmpty()) r.errorOutput = r.summary;
    return r;
}

static DiagnosticResult probeRedis(DiagId id, const QString& target, RunContext&) {
    if (target.isEmpty()) return skippedProbe(id, QStringLiteral("No target"));
    const QUrl u = normalizeUrl(target);
    if (u.isEmpty()) return makeResult(id, DiagStatus::Fail, QStringLiteral("Invalid target"), {}, {});
    if (u.scheme().toLower() != QLatin1String("redis"))
        return skippedProbe(id, QStringLiteral("Not Redis"));
    const ProbeOutcome p = tcpProbe(u, QByteArrayLiteral("PING\r\n"), 5000, 2000);
    DiagnosticResult r = probeResultScaffold(id, u, p);
    if (!p.connected) return r;
    const QString resp = QString::fromUtf8(p.banner).trimmed();
    const bool pong = resp.contains(QLatin1String("PONG"));
    r.data[QStringLiteral("pong")] = pong;
    r.data[QStringLiteral("response")] = resp;
    r.data[QStringLiteral("banner")] = resp.left(200);
    r.summary = pong ? QStringLiteral("Redis: PONG")
        : resp.isEmpty() ? QStringLiteral("No response") : resp.left(200);
    r.status = pong ? DiagStatus::Pass : DiagStatus::Warning;
    if (r.status != DiagStatus::Pass && r.errorOutput.isEmpty()) r.errorOutput = r.summary;
    return r;
}

static DiagnosticResult probeMongodb(DiagId id, const QString& target, RunContext&) {
    if (target.isEmpty()) return skippedProbe(id, QStringLiteral("No target"));
    const QUrl u = normalizeUrl(target);
    if (u.isEmpty()) return makeResult(id, DiagStatus::Fail, QStringLiteral("Invalid target"), {}, {});
    if (u.scheme().toLower() != QLatin1String("mongodb"))
        return skippedProbe(id, QStringLiteral("Not MongoDB"));
    // OP_QUERY isMaster on admin.$cmd (little-endian header)
    QByteArray bson;
    bson.append('\x13', 1);       // BSON total size = 19
    bson.append('\0', 3);
    bson.append('\x10');          // int32 type
    bson.append("isMaster");
    bson.append('\0');
    bson.append('\x01'); bson.append('\0', 3);
    bson.append('\0');
    QByteArray msg;
    auto appendLE32 = [&msg](quint32 v) {
        msg.append(char(v & 0xFF));
        msg.append(char((v >> 8) & 0xFF));
        msg.append(char((v >> 16) & 0xFF));
        msg.append(char((v >> 24) & 0xFF));
    };
    appendLE32(16 + bson.size());  // messageLength
    appendLE32(1);                 // requestID
    appendLE32(0);                 // responseTo
    appendLE32(2004);              // OP_QUERY
    msg.append('\x3f', 1);         // SlaveOk
    msg.append('\0', 3);
    msg.append("admin.$cmd");
    msg.append('\0');
    appendLE32(0);                 // numberToSkip
    appendLE32(1);                 // numberToReturn
    msg.append(bson);
    const ProbeOutcome p = tcpProbe(u, msg, 5000, 3000);
    DiagnosticResult r = probeResultScaffold(id, u, p);
    // 失败结果键齐备（归档契约）
    r.data[QStringLiteral("responded")] = false;
    r.data[QStringLiteral("version")] = QString();
    if (!p.connected) return r;
    const QByteArray& resp = p.banner;
    const bool responded = resp.size() >= 16;
    // OP_REPLY: int32 flags at offset 0; BSON docs follow 20-byte header.
    QString version;
    if (resp.size() > 36) {
        // First BSON doc: total size (LE32) then elements. Look for "version" field.
        const int docSize = (int)((quint8)resp[20]) | ((quint8)resp[21] << 8)
                          | ((quint8)resp[22] << 16) | ((quint8)resp[23] << 24);
        if (docSize > 0 && 20 + docSize <= resp.size()) {
            const QByteArray doc = resp.mid(20, docSize);
            const int vPos = doc.indexOf("version");
            if (vPos > 0 && vPos + 9 <= doc.size())
                version = QString::fromUtf8(doc.mid(vPos + 8, doc.size() - vPos - 9).split('\0').first());
        }
    }
    r.summary = responded ? (version.isEmpty() ? QStringLiteral("MongoDB responded (version unknown)") : QStringLiteral("MongoDB %1").arg(version))
                          : QStringLiteral("No response");
    r.status = responded ? DiagStatus::Pass : DiagStatus::Warning;
    r.rawOutput = r.details = QString::fromUtf8(resp.left(400).toHex(' '));
    r.data[QStringLiteral("responded")] = responded;
    r.data[QStringLiteral("version")] = version;
    if (r.status != DiagStatus::Pass && r.errorOutput.isEmpty()) r.errorOutput = r.summary;
    return r;
}

static DiagnosticResult probeLdap(DiagId id, const QString& target, RunContext&) {
    if (target.isEmpty()) return skippedProbe(id, QStringLiteral("No target"));
    const QUrl u = normalizeUrl(target);
    if (u.isEmpty()) return makeResult(id, DiagStatus::Fail, QStringLiteral("Invalid target"), {}, {});
    const QString scheme = u.scheme().toLower();
    if (scheme != QLatin1String("ldap") && scheme != QLatin1String("ldaps"))
        return skippedProbe(id, QStringLiteral("Not LDAP(S)"));
    // Minimal anonymous BindRequest
    QByteArray ldapMsg;
    ldapMsg.append('\x30'); ldapMsg.append('\x0c');
    ldapMsg.append('\x02'); ldapMsg.append('\x01'); ldapMsg.append('\x01');
    ldapMsg.append('\x60'); ldapMsg.append('\x07');
    ldapMsg.append('\x02'); ldapMsg.append('\x01'); ldapMsg.append('\x03');
    ldapMsg.append('\x04'); ldapMsg.append('\x00');
    ldapMsg.append('\x80'); ldapMsg.append('\x00');
    const ProbeOutcome p = tcpProbe(u, ldapMsg, 5000, 3000);
    DiagnosticResult r = probeResultScaffold(id, u, p);
    // 失败结果键齐备（归档契约）
    r.data[QStringLiteral("hasBindResp")] = false;
    r.data[QStringLiteral("bindOk")] = false;
    r.data[QStringLiteral("resultCode")] = -1;
    if (!p.connected) return r;
    const QByteArray& resp = p.banner;
    if (resp.isEmpty()) {
        r.summary = QStringLiteral("No response");
        r.status = DiagStatus::Warning;
        r.data[QStringLiteral("hasBindResp")] = false;
        r.data[QStringLiteral("bindOk")] = false;
        r.data[QStringLiteral("resultCode")] = -1;
        if (r.errorOutput.isEmpty()) r.errorOutput = r.summary;
        return r;
    }
    // BindResponse: 0x30 [len] ... 0x61 [len] 0x0a 0x01 [resultCode] ...
    // resultCode 在 0x61 标签块内（不是报文最后一个字节）。
    const int bResp = resp.indexOf('\x61');
    const bool hasBindResp = bResp >= 0 && bResp + 3 < resp.size();
    int resultCode = -1;
    if (hasBindResp) {
        const int lenByte = (int)(quint8)resp.at(bResp + 1);
        if (bResp + 2 + 2 < resp.size() && (quint8)resp.at(bResp + 2) == 0x0a
            && (quint8)resp.at(bResp + 3) == 0x01)
            resultCode = (int)(quint8)resp.at(bResp + 4);
        else
            resultCode = (int)lenByte;   // 简并情形：长度字节本身（错误包）
    }
    const bool bindOk = hasBindResp && resultCode == 0;
    r.summary = bindOk ? QStringLiteral("LDAP bind OK (resultCode 0)")
        : hasBindResp ? QStringLiteral("LDAP bind result code %1").arg(resultCode)
        : QStringLiteral("No LDAP bind response");
    r.status = bindOk ? DiagStatus::Pass : DiagStatus::Warning;
    r.rawOutput = r.details = QString::fromUtf8(resp.toHex(' '));
    r.data[QStringLiteral("hasBindResp")] = hasBindResp;
    r.data[QStringLiteral("bindOk")] = bindOk;
    r.data[QStringLiteral("resultCode")] = resultCode;
    if (r.status != DiagStatus::Pass && r.errorOutput.isEmpty()) r.errorOutput = r.summary;
    return r;
}

static DiagnosticResult probeMqtt(DiagId id, const QString& target, RunContext&) {
    if (target.isEmpty()) return skippedProbe(id, QStringLiteral("No target"));
    const QUrl u = normalizeUrl(target);
    if (u.isEmpty()) return makeResult(id, DiagStatus::Fail, QStringLiteral("Invalid target"), {}, {});
    const QString scheme = u.scheme().toLower();
    if (scheme != QLatin1String("mqtt") && scheme != QLatin1String("mqtts"))
        return skippedProbe(id, QStringLiteral("Not MQTT(S)"));
    // MQTT 3.1.1 CONNECT: clean session, keep-alive 60s, zero-length client id
    QByteArray connect;
    connect.append('\x10');
    // M1（5WHY）：remaining length = 12（Variable Header 10 + Payload 2），
    // 原 0x10=16 会让 broker 多等 4 字节 → CONNACK 永不返回。
    connect.append('\x0c');
    connect.append('\x00'); connect.append('\x04');
    connect.append("MQTT");
    connect.append('\x04');
    connect.append('\x02');
    connect.append('\x00'); connect.append('\x3c');
    connect.append('\x00'); connect.append('\x00');
    const ProbeOutcome p = tcpProbe(u, connect, 5000, 3000);
    DiagnosticResult r = probeResultScaffold(id, u, p);
    // 失败结果键齐备（归档契约）
    r.data[QStringLiteral("isConnack")] = false;
    r.data[QStringLiteral("resultCode")] = -1;
    r.data[QStringLiteral("accepted")] = false;
    if (!p.connected) return r;
    const QByteArray& resp = p.banner;
    if (resp.size() < 2) {
        r.summary = QStringLiteral("No CONNACK");
        r.status = DiagStatus::Warning;
        r.data[QStringLiteral("isConnack")] = false;
        r.data[QStringLiteral("resultCode")] = -1;
        r.data[QStringLiteral("accepted")] = false;
        if (r.errorOutput.isEmpty()) r.errorOutput = r.summary;
        return r;
    }
    const bool isConnack = (quint8)resp.at(0) == 0x20;
    const quint8 retCode = resp.size() >= 4 ? (quint8)resp.at(3) : 255;
    static const char* kDesc[] = {
        "Accepted", "Protocol version refused", "Identifier rejected",
        "Server unavailable", "Bad credentials", "Not authorized",
    };
    const QString desc = (retCode <= 5) ? QLatin1String(kDesc[retCode])
                                        : QStringLiteral("Unknown code %1").arg(retCode);
    r.summary = isConnack ? QStringLiteral("MQTT CONNACK: %1").arg(desc)
                          : QStringLiteral("No CONNACK received");
    r.status = (isConnack && retCode == 0) ? DiagStatus::Pass : DiagStatus::Warning;
    r.rawOutput = r.details = QString::fromUtf8(resp.toHex(' '));
    r.data[QStringLiteral("isConnack")] = isConnack;
    r.data[QStringLiteral("resultCode")] = (int)retCode;
    r.data[QStringLiteral("returnDescription")] = desc;
    r.data[QStringLiteral("accepted")] = (isConnack && retCode == 0);
    if (r.status != DiagStatus::Pass && r.errorOutput.isEmpty()) r.errorOutput = r.summary;
    return r;
}

} // namespace g5

// ── Registration（NEW-1/NEW-2/DIAG-4：scheme 过滤唯一入口 = select()）────
// diag-g5 §2.21 映射表落库：不匹配的检测不在调度/统计/Config 出现（隐藏，
// 不计 skipped）。DB/目录/消息 6 项仅 Desktop 注册（移动端隐藏，有效 Desktop）。
void registerG5Adapters() {
    using namespace PlatformFlag;
    using F = std::function<DiagnosticResult(DiagId, const QString&, RunContext&)>;
    const auto tri = [](F fn, SchemeFilter s) {
        return QVector<PlatformAdapter>{
            {PF_Desktop, "Desktop", s, fn},
            {PF_IOS,     "iOS",     s, fn},
            {PF_Android, "Android", s, fn},
        };
    };
    const auto desktopOnly = [](F fn, SchemeFilter s) {
        return QVector<PlatformAdapter>{
            {PF_Desktop, "Desktop", s, fn},   // H2：DB/目录/消息 6 项有效平台 = Desktop
        };
    };
    const SchemeFilter wildcard;                       // include 空 = 任意 scheme
    const SchemeFilter httpSchemes{{QStringLiteral("http"), QStringLiteral("https")}, false};
    const SchemeFilter nonHttp{{QStringLiteral("http"), QStringLiteral("https")}, true};  // ServiceBanner 排除语义
#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID)
    // 移动端 HTTP 族平台工厂（iOS NSURLSession / Android HttpURLConnection JNI）
    const auto platformHttp = [](DiagId i, const QString& t, RunContext&) -> DiagnosticResult {
#if defined(PLATFORM_IOS)
        return iosHttpDiagnostic(i, t);
#else
        return androidHttpDiag(i, t);
#endif
    };
    const auto triHttp = [&platformHttp](F, SchemeFilter s) {
        return QVector<PlatformAdapter>{
            {PF_Desktop, "Desktop", s, platformHttp},
            {PF_IOS,     "iOS",     s, platformHttp},
            {PF_Android, "Android", s, platformHttp},
        };
    };
#else
    const auto triHttp = tri;
#endif
    AdapterRegistry::registerAdapters(DiagId::G5UrlParsing,       tri(g5::probeUrlParsing, wildcard));
    AdapterRegistry::registerAdapters(DiagId::G5TcpConnect,       tri(g5::probeTcpConnect, wildcard));
    AdapterRegistry::registerAdapters(DiagId::G5ServiceBanner,    tri(g5::probeServiceBanner, nonHttp));
    AdapterRegistry::registerAdapters(DiagId::G5CurlVerbose,      triHttp(g5::probeCurlVerbose, httpSchemes));
    AdapterRegistry::registerAdapters(DiagId::G5HttpHeaders,      triHttp(g5::probeHttpHeaders, httpSchemes));
    AdapterRegistry::registerAdapters(DiagId::G5SecurityHeaders,  triHttp(g5::probeSecurityHeaders, httpSchemes));
    AdapterRegistry::registerAdapters(DiagId::G5SslCertificate,   tri(g5::probeSslCertificate, httpSchemes));
    AdapterRegistry::registerAdapters(DiagId::G5HttpRedirect,     triHttp(g5::probeHttpRedirect, httpSchemes));
    AdapterRegistry::registerAdapters(DiagId::G5HttpCompression,  triHttp(g5::probeHttpCompression, httpSchemes));
    AdapterRegistry::registerAdapters(DiagId::G5HttpTiming,       triHttp(g5::probeHttpTiming, httpSchemes));
    AdapterRegistry::registerAdapters(DiagId::G5FtpDiagnostics,   tri(g5::probeFtp,
        SchemeFilter{{QStringLiteral("ftp"), QStringLiteral("ftps")}, false}));
    AdapterRegistry::registerAdapters(DiagId::G5SshDiagnostics,   tri(g5::probeSsh,
        SchemeFilter{{QStringLiteral("ssh"), QStringLiteral("sftp")}, false}));
    AdapterRegistry::registerAdapters(DiagId::G5EmailDiagnostics, tri(g5::probeEmail,
        SchemeFilter{{QStringLiteral("smtp"), QStringLiteral("smtps"),
                      QStringLiteral("imap"), QStringLiteral("imaps"),
                      QStringLiteral("pop3"), QStringLiteral("pop3s")}, false}));
    AdapterRegistry::registerAdapters(DiagId::G5Telnet,           tri(g5::probeTelnet,
        SchemeFilter{{QStringLiteral("telnet")}, false}));
    AdapterRegistry::registerAdapters(DiagId::G5Mysql,            desktopOnly(g5::probeMysql,
        SchemeFilter{{QStringLiteral("mysql")}, false}));
    AdapterRegistry::registerAdapters(DiagId::G5Postgres,         desktopOnly(g5::probePostgres,
        SchemeFilter{{QStringLiteral("postgresql")}, false}));
    AdapterRegistry::registerAdapters(DiagId::G5Redis,            desktopOnly(g5::probeRedis,
        SchemeFilter{{QStringLiteral("redis")}, false}));
    AdapterRegistry::registerAdapters(DiagId::G5Mongodb,          desktopOnly(g5::probeMongodb,
        SchemeFilter{{QStringLiteral("mongodb")}, false}));
    AdapterRegistry::registerAdapters(DiagId::G5Ldap,             desktopOnly(g5::probeLdap,
        SchemeFilter{{QStringLiteral("ldap")}, false}));
    AdapterRegistry::registerAdapters(DiagId::G5Mqtt,             desktopOnly(g5::probeMqtt,
        SchemeFilter{{QStringLiteral("mqtt")}, false}));
}
