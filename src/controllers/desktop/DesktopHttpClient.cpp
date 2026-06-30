// =============================================================================
// DesktopHttpClient.cpp — G5 HTTP diagnostics (libcurl)
// =============================================================================
#include "controllers/desktop/DesktopHttpClient.h"
#ifndef NO_CURL
#include "engine/diagnostic/G5WebsiteUrl.h"
#endif

bool DesktopHttpClient::isAvailable() const {
#ifndef NO_CURL
    return true;
#else
    return false;
#endif
}

#ifdef NO_CURL
#define G5_METHOD(method, diagId) \
    DiagnosticResult DesktopHttpClient::method(const QString&) { \
        return DiagnosticResult::skipped(DiagId::diagId, QStringLiteral("G5 not available (no curl)")); \
    }
#else
#define G5_METHOD(method, diagId) \
    DiagnosticResult DesktopHttpClient::method(const QString& t) { return G5WebsiteUrl::method(t); }
#endif

G5_METHOD(urlParsing,       G5UrlParsing)
G5_METHOD(tcpConnect,        G5TcpConnect)
G5_METHOD(serviceBanner,     G5ServiceBanner)
G5_METHOD(curlVerbose,       G5CurlVerbose)
G5_METHOD(httpHeaders,       G5HttpHeaders)
G5_METHOD(securityHeaders,   G5SecurityHeaders)
G5_METHOD(sslCertificate,    G5SslCertificate)
G5_METHOD(httpRedirect,      G5HttpRedirect)
G5_METHOD(httpCompression,   G5HttpCompression)
G5_METHOD(httpTiming,        G5HttpTiming)
G5_METHOD(ftpDiagnostics,    G5FtpDiagnostics)
G5_METHOD(sshDiagnostics,    G5SshDiagnostics)
G5_METHOD(emailDiagnostics,  G5EmailDiagnostics)

#undef G5_METHOD
