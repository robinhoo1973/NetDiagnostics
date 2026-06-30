// =============================================================================
// IosHttpClient.cpp — Stub: delegates to G5WebsiteUrl
//   TODO(Phase 4): NSURLSession implementation — removes libcurl dependency
// =============================================================================
#include "controllers/ios/IosHttpClient.h"
#include "engine/diagnostic/G5WebsiteUrl.h"

bool IosHttpClient::isAvailable() const { return true; }
DiagnosticResult IosHttpClient::urlParsing(const QString& t)       { return G5WebsiteUrl::urlParsing(t); }
DiagnosticResult IosHttpClient::tcpConnect(const QString& t)       { return G5WebsiteUrl::tcpConnect(t); }
DiagnosticResult IosHttpClient::serviceBanner(const QString& t)    { return G5WebsiteUrl::serviceBanner(t); }
DiagnosticResult IosHttpClient::curlVerbose(const QString& t)      { return G5WebsiteUrl::curlVerbose(t); }
DiagnosticResult IosHttpClient::httpHeaders(const QString& t)      { return G5WebsiteUrl::httpHeaders(t); }
DiagnosticResult IosHttpClient::securityHeaders(const QString& t)  { return G5WebsiteUrl::securityHeaders(t); }
DiagnosticResult IosHttpClient::sslCertificate(const QString& t)   { return G5WebsiteUrl::sslCertificate(t); }
DiagnosticResult IosHttpClient::httpRedirect(const QString& t)     { return G5WebsiteUrl::httpRedirect(t); }
DiagnosticResult IosHttpClient::httpCompression(const QString& t)  { return G5WebsiteUrl::httpCompression(t); }
DiagnosticResult IosHttpClient::httpTiming(const QString& t)       { return G5WebsiteUrl::httpTiming(t); }
DiagnosticResult IosHttpClient::ftpDiagnostics(const QString& t)   { return G5WebsiteUrl::ftpDiagnostics(t); }
DiagnosticResult IosHttpClient::sshDiagnostics(const QString& t)   { return G5WebsiteUrl::sshDiagnostics(t); }
DiagnosticResult IosHttpClient::emailDiagnostics(const QString& t) { return G5WebsiteUrl::emailDiagnostics(t); }
