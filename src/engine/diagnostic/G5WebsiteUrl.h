// =============================================================================
// G5WebsiteUrl.h — Website/URL diagnostic tests (13 tests)
// =============================================================================
#pragma once

#include <QString>
#include <QMap>
#include "models/DiagnosticResult.h"

namespace G5WebsiteUrl {

// ── Protocol default port map (shared with AppState for UI) ────────────────
int defaultPortForScheme(const QString& scheme);
QStringList knownSchemes();

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

} // namespace G5WebsiteUrl
