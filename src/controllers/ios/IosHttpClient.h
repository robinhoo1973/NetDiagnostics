// =============================================================================
// IosHttpClient.h — iOS G5 HTTP via NSURLSession (stub, delegates to G5)
//   TODO(Phase 4): Replace with NSURLSession + TaskMetrics implementation
// =============================================================================
#pragma once
#include "controllers/IHttpClient.h"

class IosHttpClient : public IHttpClient {
public:
    IosHttpClient() = default;

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
    QString controllerName() const override { return QStringLiteral("IosHttp"); }
};
