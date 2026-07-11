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
static const QMap<QString, int> s_defaultPorts = {
    {"http",80},{"https",443},{"ftp",21},{"ftps",990},{"sftp",22},{"ssh",22},
    {"telnet",23},{"rdp",3389},{"smtp",25},{"smtps",465},{"imap",143},{"imaps",993},
    {"pop3",110},{"pop3s",995},{"mysql",3306},{"postgresql",5432},{"redis",6379},
    {"mongodb",27017},{"mssql",1433},{"ldap",389},{"ldaps",636},{"mqtt",1883},{"mqtts",8883}
};
inline int defaultPortForScheme(const QString& scheme) {
    return s_defaultPorts.value(scheme.toLower(), 80);
}
inline QStringList knownSchemes() {
    return s_defaultPorts.keys();
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
    return u.port() > 0 ? u.port() : s_defaultPorts.value(u.scheme(), 80);
}

} // namespace G5WebsiteUrl
