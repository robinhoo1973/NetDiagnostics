// =============================================================================
// DesktopHttpClient.cpp — G5 HTTP diagnostics (libcurl)
// =============================================================================
#include "controllers/desktop/DesktopHttpClient.h"
#include "controllers/detail/G5Macros.h"

bool DesktopHttpClient::isAvailable() const {
#ifdef NO_CURL
    return false;
#else
    return true;
#endif
}

G5_METHOD(DesktopHttpClient, urlParsing,       G5UrlParsing)
G5_METHOD(DesktopHttpClient, tcpConnect,        G5TcpConnect)
G5_METHOD(DesktopHttpClient, serviceBanner,     G5ServiceBanner)
G5_METHOD(DesktopHttpClient, curlVerbose,       G5CurlVerbose)
G5_METHOD(DesktopHttpClient, httpHeaders,       G5HttpHeaders)
G5_METHOD(DesktopHttpClient, securityHeaders,   G5SecurityHeaders)
G5_METHOD(DesktopHttpClient, sslCertificate,    G5SslCertificate)
G5_METHOD(DesktopHttpClient, httpRedirect,      G5HttpRedirect)
G5_METHOD(DesktopHttpClient, httpCompression,   G5HttpCompression)
G5_METHOD(DesktopHttpClient, httpTiming,        G5HttpTiming)
G5_METHOD(DesktopHttpClient, ftpDiagnostics,    G5FtpDiagnostics)
G5_METHOD(DesktopHttpClient, sshDiagnostics,    G5SshDiagnostics)
G5_METHOD(DesktopHttpClient, emailDiagnostics,  G5EmailDiagnostics)

#undef G5_METHOD
