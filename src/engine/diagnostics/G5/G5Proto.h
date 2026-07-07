// =============================================================================
// G5ProtocolDiagnostics.cpp — per-scheme TCP protocol diagnostics.
//
// All diagnostics use QTcpSocket (available on ALL platforms including
// iOS/Android).  No libcurl dependency.  Works in NO_CURL builds.
// =============================================================================
#include "engine/diagnostics/G5/G5WebsiteUrl.h"   // validate, portForUrl + declarations
#include "models/ResultProperty.h"
#include <QTcpSocket>
#include <QUrl>
#include <QElapsedTimer>

namespace G5WebsiteUrl {

namespace {

DiagId inferDiagFromScheme(const QUrl& u) {
    const QString s = u.scheme().toLower();
    if (s == "telnet")        return DiagId::G5Telnet;
    if (s == "mysql")         return DiagId::G5Mysql;
    if (s == "postgresql")    return DiagId::G5Postgres;
    if (s == "redis")         return DiagId::G5Redis;
    if (s == "mongodb")       return DiagId::G5Mongodb;
    if (s == "ldap" || s == "ldaps") return DiagId::G5Ldap;
    if (s == "mqtt" || s == "mqtts") return DiagId::G5Mqtt;
    return DiagId::G5ServiceBanner; // fallback
}
