// =============================================================================
// DesktopHttpClient.cpp — delegates to existing G5WebsiteUrl (libcurl)
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

DiagnosticResult DesktopHttpClient::urlParsing(const QString& target) {
#ifndef NO_CURL
    return G5WebsiteUrl::urlParsing(target);
#else
    return DiagnosticResult::skipped(DiagId::G5UrlParsing, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult DesktopHttpClient::tcpConnect(const QString& target) {
#ifndef NO_CURL
    return G5WebsiteUrl::tcpConnect(target);
#else
    return DiagnosticResult::skipped(DiagId::G5TcpConnect, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult DesktopHttpClient::serviceBanner(const QString& target) {
#ifndef NO_CURL
    return G5WebsiteUrl::serviceBanner(target);
#else
    return DiagnosticResult::skipped(DiagId::G5ServiceBanner, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult DesktopHttpClient::curlVerbose(const QString& target) {
#ifndef NO_CURL
    return G5WebsiteUrl::curlVerbose(target);
#else
    return DiagnosticResult::skipped(DiagId::G5CurlVerbose, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult DesktopHttpClient::httpHeaders(const QString& target) {
#ifndef NO_CURL
    return G5WebsiteUrl::httpHeaders(target);
#else
    return DiagnosticResult::skipped(DiagId::G5HttpHeaders, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult DesktopHttpClient::securityHeaders(const QString& target) {
#ifndef NO_CURL
    return G5WebsiteUrl::securityHeaders(target);
#else
    return DiagnosticResult::skipped(DiagId::G5SecurityHeaders, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult DesktopHttpClient::sslCertificate(const QString& target) {
#ifndef NO_CURL
    return G5WebsiteUrl::sslCertificate(target);
#else
    return DiagnosticResult::skipped(DiagId::G5SslCertificate, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult DesktopHttpClient::httpRedirect(const QString& target) {
#ifndef NO_CURL
    return G5WebsiteUrl::httpRedirect(target);
#else
    return DiagnosticResult::skipped(DiagId::G5HttpRedirect, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult DesktopHttpClient::httpCompression(const QString& target) {
#ifndef NO_CURL
    return G5WebsiteUrl::httpCompression(target);
#else
    return DiagnosticResult::skipped(DiagId::G5HttpCompression, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult DesktopHttpClient::httpTiming(const QString& target) {
#ifndef NO_CURL
    return G5WebsiteUrl::httpTiming(target);
#else
    return DiagnosticResult::skipped(DiagId::G5HttpTiming, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult DesktopHttpClient::ftpDiagnostics(const QString& target) {
#ifndef NO_CURL
    return G5WebsiteUrl::ftpDiagnostics(target);
#else
    return DiagnosticResult::skipped(DiagId::G5FtpDiagnostics, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult DesktopHttpClient::sshDiagnostics(const QString& target) {
#ifndef NO_CURL
    return G5WebsiteUrl::sshDiagnostics(target);
#else
    return DiagnosticResult::skipped(DiagId::G5SshDiagnostics, QStringLiteral("G5 not available (no curl)"));
#endif
}
DiagnosticResult DesktopHttpClient::emailDiagnostics(const QString& target) {
#ifndef NO_CURL
    return G5WebsiteUrl::emailDiagnostics(target);
#else
    return DiagnosticResult::skipped(DiagId::G5EmailDiagnostics, QStringLiteral("G5 not available (no curl)"));
#endif
}
