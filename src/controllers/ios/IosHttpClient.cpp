// =============================================================================
// IosHttpClient.cpp — G5 HTTP diagnostics
//   TODO(Phase 4): NSURLSession implementation — removes libcurl dependency
// =============================================================================
#include "controllers/ios/IosHttpClient.h"
#include "controllers/detail/G5Macros.h"

bool IosHttpClient::isAvailable() const {
#ifdef NO_CURL
    return false;
#else
    return true;
#endif
}

G5_METHOD(IosHttpClient, urlParsing,       G5UrlParsing)
G5_METHOD(IosHttpClient, tcpConnect,        G5TcpConnect)
G5_METHOD(IosHttpClient, serviceBanner,     G5ServiceBanner)
G5_METHOD(IosHttpClient, curlVerbose,       G5CurlVerbose)
G5_METHOD(IosHttpClient, httpHeaders,       G5HttpHeaders)
G5_METHOD(IosHttpClient, securityHeaders,   G5SecurityHeaders)
G5_METHOD(IosHttpClient, sslCertificate,    G5SslCertificate)
G5_METHOD(IosHttpClient, httpRedirect,      G5HttpRedirect)
G5_METHOD(IosHttpClient, httpCompression,   G5HttpCompression)
G5_METHOD(IosHttpClient, httpTiming,        G5HttpTiming)
G5_METHOD(IosHttpClient, ftpDiagnostics,    G5FtpDiagnostics)
G5_METHOD(IosHttpClient, sshDiagnostics,    G5SshDiagnostics)
G5_METHOD(IosHttpClient, emailDiagnostics,  G5EmailDiagnostics)

#undef G5_METHOD
