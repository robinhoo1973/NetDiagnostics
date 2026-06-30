// =============================================================================
// DesktopHttpClient.h — G5 HTTP via existing libcurl-based G5WebsiteUrl
// =============================================================================
#pragma once
#include "controllers/IHttpClient.h"

class DesktopHttpClient : public IHttpClient {
public:
    DesktopHttpClient() = default;

    DiagnosticResult urlParsing(const QString& target) override;
    DiagnosticResult tcpConnect(const QString& target) override;
    DiagnosticResult serviceBanner(const QString& target) override;
    DiagnosticResult curlVerbose(const QString& target) override;
    DiagnosticResult httpHeaders(const QString& target) override;
    DiagnosticResult securityHeaders(const QString& target) override;
    DiagnosticResult sslCertificate(const QString& target) override;
    DiagnosticResult httpRedirect(const QString& target) override;
    DiagnosticResult httpCompression(const QString& target) override;
    DiagnosticResult httpTiming(const QString& target) override;
    DiagnosticResult ftpDiagnostics(const QString& target) override;
    DiagnosticResult sshDiagnostics(const QString& target) override;
    DiagnosticResult emailDiagnostics(const QString& target) override;

    bool isAvailable() const override;
    QString controllerName() const override { return QStringLiteral("DesktopHttp"); }
};
