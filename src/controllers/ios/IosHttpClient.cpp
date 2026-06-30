// =============================================================================
// IosHttpClient.cpp — Stub: delegates to G5WebsiteUrl
//   TODO(Phase 4): NSURLSession implementation — removes libcurl dependency
// =============================================================================
#include "controllers/ios/IosHttpClient.h"
#include "engine/diagnostic/G5WebsiteUrl.h"

bool IosHttpClient::isAvailable() const {
#ifndef NO_CURL
    return true;
#else
    return false;
#endif
}
DiagnosticResult IosHttpClient::urlParsing(const QString& t) {
#ifndef NO_CURL
    return G5WebsiteUrl::urlParsing(t);
#else
    return DiagnosticResult::skipped(DiagId::G5UrlParsing, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult IosHttpClient::tcpConnect(const QString& t) {
#ifndef NO_CURL
    return G5WebsiteUrl::tcpConnect(t);
#else
    return DiagnosticResult::skipped(DiagId::G5TcpConnect, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult IosHttpClient::serviceBanner(const QString& t) {
#ifndef NO_CURL
    return G5WebsiteUrl::serviceBanner(t);
#else
    return DiagnosticResult::skipped(DiagId::G5ServiceBanner, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult IosHttpClient::curlVerbose(const QString& t) {
#ifndef NO_CURL
    return G5WebsiteUrl::curlVerbose(t);
#else
    return DiagnosticResult::skipped(DiagId::G5CurlVerbose, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult IosHttpClient::httpHeaders(const QString& t) {
#ifndef NO_CURL
    return G5WebsiteUrl::httpHeaders(t);
#else
    return DiagnosticResult::skipped(DiagId::G5HttpHeaders, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult IosHttpClient::securityHeaders(const QString& t) {
#ifndef NO_CURL
    return G5WebsiteUrl::securityHeaders(t);
#else
    return DiagnosticResult::skipped(DiagId::G5SecurityHeaders, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult IosHttpClient::sslCertificate(const QString& t) {
#ifndef NO_CURL
    return G5WebsiteUrl::sslCertificate(t);
#else
    return DiagnosticResult::skipped(DiagId::G5SslCertificate, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult IosHttpClient::httpRedirect(const QString& t) {
#ifndef NO_CURL
    return G5WebsiteUrl::httpRedirect(t);
#else
    return DiagnosticResult::skipped(DiagId::G5HttpRedirect, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult IosHttpClient::httpCompression(const QString& t) {
#ifndef NO_CURL
    return G5WebsiteUrl::httpCompression(t);
#else
    return DiagnosticResult::skipped(DiagId::G5HttpCompression, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult IosHttpClient::httpTiming(const QString& t) {
#ifndef NO_CURL
    return G5WebsiteUrl::httpTiming(t);
#else
    return DiagnosticResult::skipped(DiagId::G5HttpTiming, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult IosHttpClient::ftpDiagnostics(const QString& t) {
#ifndef NO_CURL
    return G5WebsiteUrl::ftpDiagnostics(t);
#else
    return DiagnosticResult::skipped(DiagId::G5FtpDiagnostics, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult IosHttpClient::sshDiagnostics(const QString& t) {
#ifndef NO_CURL
    return G5WebsiteUrl::sshDiagnostics(t);
#else
    return DiagnosticResult::skipped(DiagId::G5SshDiagnostics, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult IosHttpClient::emailDiagnostics(const QString& t) {
#ifndef NO_CURL
    return G5WebsiteUrl::emailDiagnostics(t);
#else
    return DiagnosticResult::skipped(DiagId::G5EmailDiagnostics, QStringLiteral("G5 not available (no curl)"));
#endif
}
