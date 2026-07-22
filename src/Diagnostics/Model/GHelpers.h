// GHelpers.h — shared helpers for G1/G2/G3 per-function .cpp files.
#pragma once
#include "Diagnostics/Model/GBase.h"
#include "Diagnostics/View/DiagnosticFormatter.h"
#include "Common/Utils/Logger.h"
#include <cstring>

namespace SystemDiagnostics {

// ── MAC address formatting ──────────────────────────────────────────
static QString macToStr(const unsigned char* mac) {
    return QStringLiteral("%1:%2:%3:%4:%5:%6")
        .arg(mac[0], 2, 16, QLatin1Char('0'))
        .arg(mac[1], 2, 16, QLatin1Char('0'))
        .arg(mac[2], 2, 16, QLatin1Char('0'))
        .arg(mac[3], 2, 16, QLatin1Char('0'))
        .arg(mac[4], 2, 16, QLatin1Char('0'))
        .arg(mac[5], 2, 16, QLatin1Char('0'));
}

// ── IPv4 formatting ─────────────────────────────────────────────────
static QString ip4ToStr(struct in_addr a) {
    char buf[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &a, buf, sizeof(buf));
    return QString::fromLatin1(buf);
}
static QString ipToStr(uint32_t ip) {
    struct in_addr a; a.s_addr = ip;
    return ip4ToStr(a);
}

// ── Cellular helpers ────────────────────────────────────────────────
static bool hasNonEmptyValue(const QVariantMap& values, const char* key) {
    auto it = values.constFind(QLatin1String(key));
    return it != values.cend() && !it->toString().trimmed().isEmpty();
}

static bool hasCellularIdentity(const QVariantMap& cell) {
    return hasNonEmptyValue(cell, "carrierName")
        || hasNonEmptyValue(cell, "radioAccess")
        || (hasNonEmptyValue(cell, "mcc") && hasNonEmptyValue(cell, "mnc"));
}

static QString cellularSummary(const QVariantMap& cell) {
    QString carrier = cell.value(QStringLiteral("carrierName")).toString().trimmed();
    QString radio = cell.value(QStringLiteral("radioAccess")).toString().trimmed();
    if (!carrier.isEmpty() && !radio.isEmpty())
        return QStringLiteral("Carrier: %1 (%2)").arg(carrier, radio);
    if (!carrier.isEmpty()) return QStringLiteral("Carrier: %1").arg(carrier);
    if (!radio.isEmpty()) return QStringLiteral("Radio: %1").arg(radio);
    return QStringLiteral("Cellular service detected");
}

// ── TCP state names ─────────────────────────────────────────────────
#if defined(_WIN32)
static const char* tcpStateName(int st) {
    switch(st){case 1:return"CLOSED";case 2:return"LISTEN";case 3:return"SYN_SENT";
    case 4:return"SYN_RCVD";case 5:return"ESTABLISHED";case 6:return"FIN_WAIT1";
    case 7:return"FIN_WAIT2";case 8:return"CLOSE_WAIT";case 9:return"CLOSING";
    case 10:return"LAST_ACK";case 11:return"TIME_WAIT";case 12:return"DELETE_TCB";
    default:return"UNKNOWN";}
}
#else
static const char* tcpStateName(int st) {
    switch(st){case 1:return"ESTABLISHED";case 2:return"SYN_SENT";case 3:return"SYN_RECV";
    case 4:return"FIN_WAIT1";case 5:return"FIN_WAIT2";case 6:return"TIME_WAIT";
    case 7:return"CLOSE";case 8:return"CLOSE_WAIT";case 9:return"LAST_ACK";
    case 10:return"LISTEN";case 11:return"CLOSING";default:return"UNKNOWN";}
}
#endif

// ── Shared URL parser — eliminates 5x duplicated parse logic ─────
struct ParsedUrl { QString host; int port = 80; QString path; };
inline ParsedUrl parseHttpUrl(const QString& urlStr) {
    ParsedUrl p;
    QString u = urlStr;
    if (!u.startsWith(QStringLiteral("http://"))) return p;
    u = u.mid(7);
    auto slash = u.indexOf('/');
    QString hp = (slash > 0) ? u.left(slash) : u;
    p.path = (slash > 0) ? u.mid(slash) : QStringLiteral("/");
    auto colon = hp.lastIndexOf(':');
    if (colon > 0) { p.host = hp.left(colon); p.port = hp.mid(colon + 1).toInt(); }
    else { p.host = hp; }
    return p;
}

// ── Hodges-Lehmann robust location estimator ─────────────────────
// Median of all N(N+1)/2 pairwise averages.  96% Gaussian efficiency,
// 29% breakdown point.  Best all-around robust estimator for N=3-100.
inline double hodgesLehmann(const QVector<double>& v) {
    int n = v.size();
    if (n == 1) return v[0];
    int npairs = n * (n + 1) / 2;
    QVector<double> pairs; pairs.reserve(npairs);
    for (int i = 0; i < n; i++)
        for (int j = i; j < n; j++)
            pairs.append((v[i] + v[j]) / 2.0);
    std::sort(pairs.begin(), pairs.end());
    return (npairs % 2 == 1) ? pairs[npairs / 2]
           : (pairs[npairs / 2 - 1] + pairs[npairs / 2]) / 2.0;
}

// ── Generic median ───────────────────────────────────────────────────
inline double median(QVector<double> v) {
    int n = v.size();
    if (n == 0) return 0.0;
    if (n == 1) return v[0];
    std::sort(v.begin(), v.end());
    return (n % 2 == 1) ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2.0;
}

// Forward declarations (defined in GCommon.cpp, non-static — shared across TUs)
int      tcpPingMs(const QString& host, int port);
struct SpeedResult { double mbps; int bytes; int durationMs; bool ok; QString error; };
SpeedResult httpDownload(const QString& urlStr, int targetBytes, int timeoutMs);
SpeedResult httpUpload(const QString& urlStr, int targetBytes, int timeoutMs);

// Raw HTTP GET — TCP connect + GET request → raw HTTP response (headers + body).
// Used by G3GeoIPLoc for GeoIP country detection (Phase 3).
// When connectHost is non-empty, connects to that address (e.g. IP) but still
// sends `host` in the HTTP Host header — bypasses DNS for the TCP connection.
QByteArray httpGet(const QString& host, int port, const QString& path,
                   int timeoutMs = 3000, int maxBytes = 4096,
                   const QString& connectHost = QString());

// HTTPS GET — uses QNetworkAccessManager for TLS.  Returns response body.
// Synchronous (local QEventLoop).  Used by G3GeoIPLoc for GeoIP providers.
QByteArray httpsGet(const QString& url, int timeoutMs = 5000);

// ── DoH DNS records ────────────────────────────────────────────────
struct DohDnsRecord {
    QString name;     // owner name (e.g. "www.google.com.")
    int     type = 0;  // 1=A, 5=CNAME, 2=NS, 28=AAAA
    int     ttl  = 0;
    QString data;     // IP for A/AAAA, target for CNAME/NS
};

struct DohDnsFullResult {
    QStringList     aRecords;    // A-record IPs (backward compat)
    QStringList     cnameChain;  // CNAME targets in order
    QList<DohDnsRecord> allRecords; // all parsed records
    bool            hasCname = false;
    int             minTtl = -1;   // -1 = no TTL data sentinel; 0 = real TTL=0 (pollution signal)
};

// DoH (DNS-over-HTTPS) query — queries 4 resolvers, returns majority consensus.
// Returns IPs agreed upon by majority (≥3 of 4), or all unique IPs if split.
QStringList dohQuery(const QString& domain,
                     const QString& type = QStringLiteral("A"), int timeoutMs = 4000);

// DoH query with full record parsing — returns A records, CNAME chain, TTL.
// Same 4-resolver majority logic as dohQuery(), but preserves record metadata.
// 5WHY: DoH timeout was 4000ms, but typical response is 50-500ms.
// 2000ms provides 4× headroom for congested networks while halving
// the worst-case wait for unreachable resolvers.
DohDnsFullResult dohQueryFull(const QString& domain,
                           const QString& type = QStringLiteral("A"), int timeoutMs = 2000);

// HTTP TTFB probe — TCP connect + HTTP GET → time to first byte (ms).
// Returns -1.0 on failure. Shared by GeoProbe and geoIPLoc.
double   httpTtfb(const QString& host, int port, const QString& path,
                  int connectTimeoutMs = 5000, int readTimeoutSec = 5);
inline double httpTtfb(const ParsedUrl& pu, int connectTimeoutMs = 5000, int readTimeoutSec = 5) {
    return httpTtfb(pu.host, pu.port, pu.path, connectTimeoutMs, readTimeoutSec);
}

// ── ISO 3166-1 country code display helpers ──────────────────────
// Converts 2-letter ISO code (e.g. "CN") → 3-letter (e.g. "CHN") for table view,
// or → full name (e.g. "China") for non-table display.
QString countryCode3(const QString& code2);
QString countryFullName(const QString& code2);

} // namespace SystemDiagnostics
