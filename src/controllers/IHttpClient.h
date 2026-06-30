// =============================================================================
// IHttpClient.h — Interface for G5 HTTP/Website diagnostics
// =============================================================================
#pragma once
#include "models/DiagnosticResult.h"
#include "models/DiagId.h"
#include <QUrl>
#include <QDateTime>
#include <QStringList>

struct HttpTimingResult {
    int statusCode = 0;
    qint64 dnsMs = 0, connectMs = 0, tlsMs = 0;
    qint64 firstByteMs = 0, totalMs = 0;
    qint64 bodyBytes = 0;
    QString error;
};

struct SslCertResult {
    QString subject, issuer;
    QDateTime validFrom, validTo;
    qint64 daysLeft = 0;
    QString thumbprint;
    QStringList subjectAltNames;
    bool valid = false;
};

class IHttpClient {
public:
    virtual ~IHttpClient() = default;

    // G5 diagnostic tests
    virtual DiagnosticResult urlParsing(const QString& target) = 0;
    virtual DiagnosticResult tcpConnect(const QString& target) = 0;
    virtual DiagnosticResult serviceBanner(const QString& target) = 0;
    virtual DiagnosticResult curlVerbose(const QString& target) = 0;
    virtual DiagnosticResult httpHeaders(const QString& target) = 0;
    virtual DiagnosticResult securityHeaders(const QString& target) = 0;
    virtual DiagnosticResult sslCertificate(const QString& target) = 0;
    virtual DiagnosticResult httpRedirect(const QString& target) = 0;
    virtual DiagnosticResult httpCompression(const QString& target) = 0;
    virtual DiagnosticResult httpTiming(const QString& target) = 0;
    virtual DiagnosticResult ftpDiagnostics(const QString& target) = 0;
    virtual DiagnosticResult sshDiagnostics(const QString& target) = 0;
    virtual DiagnosticResult emailDiagnostics(const QString& target) = 0;

    virtual bool isAvailable() const = 0;
    virtual QString controllerName() const = 0;
};
