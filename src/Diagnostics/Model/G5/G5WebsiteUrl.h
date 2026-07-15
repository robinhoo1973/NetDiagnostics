// =============================================================================
// G5WebsiteUrl.h — Website/URL diagnostic tests (13 tests)
// =============================================================================
#pragma once

#include <QString>
#include <QUrl>
#include <QMap>
#include "Common/Model/DiagnosticResult.h"

namespace G5WebsiteUrl {

// ── Protocol default port map (shared with AppState for UI) ────────────────
// 5WHY: s_defaultPorts was a namespace-scope static const QMap in a header.
// Every TU that included this header constructed its own copy of the QMap
// (with QString keys from const char*) during static initialization before
// main().  On iOS, dyld's static-init ordering could run this before Qt's
// internal allocator / QString tables were ready → SIGSEGV before main().
// Meyer's Singleton (function-local static) defers construction to first
// call, which is always after QCoreApplication has been constructed.
inline const QMap<QString, int>& defaultPorts() {
    static const QMap<QString, int> ports = {
        {"http",80},{"https",443},{"ftp",21},{"ftps",990},{"sftp",22},{"ssh",22},
        {"telnet",23},{"rdp",3389},{"smtp",25},{"smtps",465},{"imap",143},{"imaps",993},
        {"pop3",110},{"pop3s",995},{"mysql",3306},{"postgresql",5432},{"redis",6379},
        {"mongodb",27017},{"mssql",1433},{"ldap",389},{"ldaps",636},{"mqtt",1883},{"mqtts",8883}
    };
    return ports;
}
inline int defaultPortForScheme(const QString& scheme) {
    return defaultPorts().value(scheme.toLower(), 80);
}
inline QStringList knownSchemes() {
    return defaultPorts().keys();
}

DiagnosticResult urlParsing(const QString& target);
DiagnosticResult tcpConnect(const QString& target);
DiagnosticResult serviceBanner(const QString& target);
DiagnosticResult curlVerbose(const QString& target);
DiagnosticResult httpHeaders(const QString& target);
DiagnosticResult securityHeaders(const QString& target);
DiagnosticResult sslCertificate(const QString& target);
DiagnosticResult httpRedirect(const QString& target);
DiagnosticResult httpCompression(const QString& target);
DiagnosticResult httpTiming(const QString& target);
DiagnosticResult ftpDiagnostics(const QString& target);
DiagnosticResult sshDiagnostics(const QString& target);
DiagnosticResult emailDiagnostics(const QString& target);

// ── Per-scheme TCP protocol diagnostics (G5ProtocolDiagnostics.cpp) ────
// All use QTcpSocket only — no libcurl, works on all platforms.
DiagnosticResult telnetDiagnostics(const QString& target);
DiagnosticResult mysqlDiagnostics(const QString& target);
DiagnosticResult postgresDiagnostics(const QString& target);
DiagnosticResult redisDiagnostics(const QString& target);
DiagnosticResult mongodbDiagnostics(const QString& target);
DiagnosticResult ldapDiagnostics(const QString& target);
DiagnosticResult mqttDiagnostics(const QString& target);

// URL helpers (used by per-scheme diagnostics)
inline QUrl validate(const QString& target) {
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
inline int portForUrl(const QUrl& u) {
    return u.port() > 0 ? u.port() : defaultPorts().value(u.scheme(), 80);
}

} // namespace G5WebsiteUrl

// ── G5 scheme-to-DiagId matching (single source of truth) ──────────────
// Used by AppState::runDiagnostics (scheduling) and ResultsModel (display).
inline bool g5DiagMatchesScheme(DiagId id, const QString& schemeLower) {
    bool isGeneric = (id == DiagId::G5UrlParsing || id == DiagId::G5TcpConnect);
    if (isGeneric) return true;

    bool isHttp = (schemeLower == "http" || schemeLower == "https");

    // G5ServiceBanner: raw TCP banner grab — useful for FTP/SSH/SMTP/etc.
    // but NOT for HTTP/HTTPS (those have dedicated tests: CurlVerbose,
    // HttpHeaders, SecurityHeaders, HttpRedirect, HttpCompression, HttpTiming).
    // Running ServiceBanner against HTTP would just dump raw HTTP headers
    // redundantly with the proper HTTP tests above.
    if (id == DiagId::G5ServiceBanner) return !isHttp;
    bool isFtp  = (schemeLower == "ftp" || schemeLower == "ftps");
    bool isSsh  = (schemeLower == "ssh" || schemeLower == "sftp");

    if (isHttp) {
        return (id == DiagId::G5CurlVerbose || id == DiagId::G5HttpHeaders
             || id == DiagId::G5SecurityHeaders || id == DiagId::G5SslCertificate
             || id == DiagId::G5HttpRedirect || id == DiagId::G5HttpCompression
             || id == DiagId::G5HttpTiming);
    }
    if (isFtp)  return id == DiagId::G5FtpDiagnostics;
    if (isSsh)  return id == DiagId::G5SshDiagnostics;

    if (schemeLower == "mysql")      return id == DiagId::G5Mysql;
    if (schemeLower == "postgresql") return id == DiagId::G5Postgres;
    if (schemeLower == "redis")      return id == DiagId::G5Redis;
    if (schemeLower == "mongodb")    return id == DiagId::G5Mongodb;
    if (schemeLower == "ldap")       return id == DiagId::G5Ldap;
    if (schemeLower == "mqtt")       return id == DiagId::G5Mqtt;
    if (schemeLower == "telnet")     return id == DiagId::G5Telnet;
    if (schemeLower == "smtp" || schemeLower == "smtps"
     || schemeLower == "imap" || schemeLower == "imaps"
     || schemeLower == "pop3" || schemeLower == "pop3s")
        return id == DiagId::G5EmailDiagnostics;

    return false;
}
