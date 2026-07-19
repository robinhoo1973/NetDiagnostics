#include "Diagnostics/Model/GHelpers.h"
#include "Diagnostics/Model/G3/G3InternetDns.h"
#include <QSslSocket>
#include <algorithm>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <cstring>

namespace G1G2G3Native {

// SpeedTest class defined in G3InternetDns.h (shared between geoIPLoc and internetConnectivity)
// Continent fallback: map country codes to nearby regions with server coverage
static const QMap<QString, QStringList> continentFallback = {
    {"Asia",         {"CN","KR","SG","IN","JP","AE"}},
    {"Europe",       {"DE","GB","FR","NL","IT","ES","SE","RU"}},
    {"NorthAmerica", {"US","CA"}},
    {"SouthAmerica", {"BR","AR"}},
    {"Africa",       {"ZA","FR","GB"}},  // ZA for Southern, FR/GB for West (Lagos→Paris < Lagos→Johannesburg)
    {"Oceania",      {"AU","SG"}},
};
static QString continentForCountry(const QString& cc) {
    static const QMap<QString, QString> ccContinent = {
        {"CN","Asia"},{"KR","Asia"},{"SG","Asia"},{"IN","Asia"},{"JP","Asia"},{"MN","Asia"},
        {"AE","Asia"},{"TR","Asia"},{"SA","Asia"},{"QA","Asia"},
        {"HK","Asia"},{"TW","Asia"},{"TH","Asia"},{"VN","Asia"},
        {"MY","Asia"},{"ID","Asia"},{"PH","Asia"},{"PK","Asia"},{"BD","Asia"},{"LK","Asia"},
        {"RU","Europe"},{"DE","Europe"},{"GB","Europe"},{"FR","Europe"},{"NL","Europe"},{"IT","Europe"},
        {"ES","Europe"},{"SE","Europe"},{"PL","Europe"},{"UA","Europe"},{"CH","Europe"},{"AT","Europe"},
        {"BE","Europe"},{"NO","Europe"},{"FI","Europe"},{"DK","Europe"},{"IE","Europe"},{"PT","Europe"},
        {"US","NorthAmerica"},{"CA","NorthAmerica"},{"MX","NorthAmerica"},
        {"BR","SouthAmerica"},{"AR","SouthAmerica"},{"CO","SouthAmerica"},{"CL","SouthAmerica"},{"PE","SouthAmerica"},
        {"ZA","Africa"},{"NG","Africa"},{"KE","Africa"},{"EG","Africa"},
        {"AU","Oceania"},{"NZ","Oceania"},
    };
    return ccContinent.value(cc, "XX");
}
QVector<SpeedTest::Server> SpeedTest::serversForCountry(const QString& hint) const {
    auto it = m.constFind(hint);
    if (it != m.cend()) return it.value();
    QString p = hint.left(2).toUpper();
    it = m.constFind(p);
    if (it != m.cend()) return it.value();
    // Continent fallback: find servers from nearby countries in the same continent
    QString continent = continentForCountry(p);
    if (continent != "XX" && continentFallback.contains(continent)) {
        QVector<Server> nearby;
        const auto& ccList = continentFallback[continent];
        for (const auto& cc : ccList) {
            auto cit = m.constFind(cc);
            if (cit != m.cend()) nearby.append(cit.value());
        }
        if (!nearby.isEmpty()) return nearby;
    }
    // 5WHY: When country is unknown (XX), allServers() returned 48+ servers
    // globally. Shuffling and latency-checking 12 of them at 5s each took up
    // to 60s, and many region-specific servers (e.g. CN-only) were unreachable
    // from other continents. Now returns a curated shortlist of well-known
    // geographically-diverse public speed-test servers that are highly likely
    // to be reachable from anywhere. Falls back to allServers() only if the
    // curated set is empty (should never happen with the current server DB).
    if (hint == QStringLiteral("XX") || p == QStringLiteral("XX")) {
        // 5WHY: triple-nested loop (countries x servers x fallbackHosts) was
        // O(n*m*k) ~600 comparisons. Build a QSet from the fallback hostnames
        // for O(1) lookup: single pass over all servers, O(n*m) total.
        // 5WHY: When country is Unknown (XX), all 8 global fallback servers
        // were outside China → GFW blocked them → all unreachable →
        // "no reachable servers".  Now includes China Mobile servers which
        // are always GFW-accessible, plus diverse global servers.
        static const QSet<QString> kGlobalFallbackSet = {
            // China (GFW-safe, always reachable)
            "speedtest1.gd.chinamobile.com",
            "speedtest.bj.chinamobile.com",
            // Asia-Pacific
            "speedtest.singtel.com",     // Singapore
            "seoul.speedtest.gslnetworks.com", // Seoul
            // Europe
            "speedtest.tele2.net",      // Stockholm -- most reliable
            "speedtest.belwue.net",     // Stuttgart -- very reliable
            // Americas
            "speedtest.xfinity.com",     // New York (NA)
            "speedtest.vivo.com.br",     // Sao Paulo (SA)
            // Africa
            "speedtest.mtn.co.za",       // Johannesburg
        };
        QVector<Server> curated;
        for (auto it = m.cbegin(); it != m.cend(); ++it) {
            for (const auto& s : it.value()) {
                if (kGlobalFallbackSet.contains(s.host))
                    curated.append(s);
            }
        }
        if (!curated.isEmpty()) return curated;
    }
    return allServers();
}
QVector<SpeedTest::Server> SpeedTest::allServers() const {
    // 5WHY: appending without reserve() causes ~5-6 reallocations for 50+
    // servers across ~15 countries. Count first, then reserve + append.
    // Two-pass is worthwhile here: QMap iteration is O(n) with cheap
    // arithmetic (size()), while QVector reallocation copies all QString
    // members per Server object -- much more expensive.
    int total = 0;
    for (auto it = m.cbegin(); it != m.cend(); ++it) total += it.value().size();
    QVector<Server> a; a.reserve(total);
    for (auto it = m.cbegin(); it != m.cend(); ++it) a.append(it.value());
    return a;
}
// 5WHY: All HTTP GeoIP providers (ipip.net, ip-api.com, ipapi.co, ipinfo.io)
// have been removed — they were blocked inside GFW.  Replaced with the
// DNS(UDP+DoH) → HTTPS GeoIP → prefix chain below.

// ── DNS-based public IP discovery (UDP 53) ──
// 5WHY: DNS UDP is rarely blocked. Query OpenDNS for myip.opendns.com A.
static QString discoverPublicIpViaDns(int timeoutMs = 2000) {
    unsigned char query[64] = {};
    query[0] = 0x12; query[1] = 0x34;
    query[2] = 0x01; query[3] = 0x00;
    query[4] = 0x00; query[5] = 0x01;
    int pos = 12;
    query[pos++] = 4; memcpy(query + pos, "myip", 4); pos += 4;
    query[pos++] = 7; memcpy(query + pos, "opendns", 7); pos += 7;
    query[pos++] = 3; memcpy(query + pos, "com", 3); pos += 3;
    query[pos++] = 0;
    query[pos++] = 0x00; query[pos++] = 0x01;
    query[pos++] = 0x00; query[pos++] = 0x01;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return {};
    struct sockaddr_in resolver;
    resolver.sin_family = AF_INET;
    resolver.sin_port = htons(53);
    inet_pton(AF_INET, "208.67.222.222", &resolver.sin_addr);

    struct timeval tv = {timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    sendto(sock, (const char*)query, pos, 0,
           (struct sockaddr*)&resolver, sizeof(resolver));

    unsigned char response[512];
    socklen_t addrLen = sizeof(resolver);
    int len = recvfrom(sock, (char*)response, sizeof(response), 0,
                       (struct sockaddr*)&resolver, &addrLen);
    closeSocket(sock);
    if (len < 12) return {};
    int anCount = (response[6] << 8) | response[7];
    if (anCount < 1) return {};
    int qdCount = (response[4] << 8) | response[5];

    int offset = 12;
    for (int i = 0; i < qdCount && offset < len; i++) {
        while (offset < len && response[offset] != 0) {
            if ((response[offset] & 0xC0) == 0xC0) { offset += 2; break; }
            offset += response[offset] + 1;
        }
        if (offset < len && response[offset] == 0) offset++;
        offset += 4;
    }
    for (int i = 0; i < anCount && offset + 10 <= len; i++) {
        if ((response[offset] & 0xC0) == 0xC0) offset += 2;
        else { while (offset < len && response[offset] != 0) offset += response[offset] + 1; offset++; }
        if (offset + 10 > len) break;
        int rtype  = (response[offset] << 8) | response[offset + 1];
        int rdlen  = (response[offset + 8] << 8) | response[offset + 9];
        int rdStart = offset + 10;
        if (rtype == 1 && rdlen == 4 && rdStart + 4 <= len) {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, response + rdStart, ip, sizeof(ip));
            return QString::fromLatin1(ip);
        }
        offset = rdStart + rdlen;
    }
    return {};
}

// ── DoH-based public IP discovery (HTTPS DNS, fallback when UDP blocked) ──
// 5WHY: Raw UDP DNS on port 53 can be blocked/redirected in corporate or
// restrictive networks. DNS-over-HTTPS uses standard HTTPS (TCP 443) which
// is almost never blocked. AliDNS (dns.alidns.com) has CDN nodes inside
// China, making it the most reliable DoH endpoint for GFW users.
static QString discoverPublicIpViaDoh(int timeoutMs = 3000) {
    QSslSocket ssl;
    ssl.connectToHostEncrypted(QStringLiteral("dns.alidns.com"), 443);
    if (!ssl.waitForEncrypted(timeoutMs)) return {};

    QByteArray req = QByteArrayLiteral(
        "GET /dns-query?name=myip.opendns.com&type=A HTTP/1.1\r\n"
        "Host: dns.alidns.com\r\n"
        "Accept: application/dns-json\r\n"
        "User-Agent: NetDiagnostics/1.0\r\n"
        "Connection: close\r\n\r\n");
    ssl.write(req);
    if (!ssl.waitForBytesWritten(timeoutMs)) return {};

    QByteArray resp;
    QElapsedTimer t; t.start();
    while (t.elapsed() < timeoutMs) {
        if (ssl.waitForReadyRead(500)) {
            resp.append(ssl.readAll());
            if (resp.contains("\r\n\r\n")) break;
        }
    }
    ssl.disconnectFromHost();

    int hdrEnd = resp.indexOf("\r\n\r\n");
    if (hdrEnd < 0) return {};
    QByteArray body = resp.mid(hdrEnd + 4);
    // Parse DoH JSON: {"Answer":[{"data":"1.2.3.4"}]}
    int dataPos = body.indexOf("\"data\":\"");
    if (dataPos < 0) return {};
    dataPos += 8;
    int dataEnd = body.indexOf('\"', dataPos);
    if (dataEnd < 0) return {};
    QString ip = QString::fromUtf8(body.mid(dataPos, dataEnd - dataPos));
    if (ip.count('.') == 3) {
        bool ok = true;
        for (const auto& part : ip.split('.')) {
            int v = part.toInt(&ok);
            if (!ok || v < 0 || v > 255) { ok = false; break; }
        }
        if (ok) return ip;
    }
    return {};
}

// ── HTTPS GeoIP lookup helper — generic GET → parse JSON country field ──
// 5WHY: DoH only revealed public IP, still needing IP→country mapping.
// Now we query real HTTPS GeoIP APIs with the DNS-discovered IP directly
// in the URL path.  4 providers: 2 CN + 2 global, tried sequentially with
// 2s timeout each so total worst-case adds ~8s to the DNS 2s = 10s total.
static QString geoIpLookupHttps(const QString& host, int port, const QString& path,
                                 const QString& jsonKey, int timeoutMs = 2000) {
    QSslSocket ssl;
    ssl.connectToHostEncrypted(host, port);
    if (!ssl.waitForEncrypted(timeoutMs)) return {};

    QByteArray req = QStringLiteral("GET %1 HTTP/1.1\r\nHost: %2\r\n"
        "User-Agent: NetDiagnostics/1.0\r\nConnection: close\r\n\r\n")
        .arg(path, host).toUtf8();
    ssl.write(req);
    if (!ssl.waitForBytesWritten(timeoutMs)) return {};

    QByteArray resp;
    QElapsedTimer t; t.start();
    while (t.elapsed() < timeoutMs) {
        if (ssl.waitForReadyRead(500)) {
            resp.append(ssl.readAll());
            if (resp.contains("\r\n\r\n")) break;
        }
    }
    ssl.disconnectFromHost();

    int hdrEnd = resp.indexOf("\r\n\r\n");
    if (hdrEnd < 0) return {};
    QByteArray body = resp.mid(hdrEnd + 4);

    // ipapi.co returns plain-text country code (no JSON)
    if (jsonKey.isEmpty()) {
        QString cc = QString::fromUtf8(body).trimmed().toUpper();
        return (cc.length() == 2) ? cc : QString();
    }
    // JSON: search for "key":"value"
    QByteArray needle = QByteArrayLiteral("\"") + jsonKey.toUtf8() + QByteArrayLiteral("\":\"");
    int pos = body.indexOf(needle);
    if (pos < 0) {
        // ipip.net freeapi returns JSON array ["CN",...] — search for first quoted string
        if (jsonKey == QStringLiteral("_array_")) {
            pos = body.indexOf('\"');
            if (pos >= 0) {
                pos++;
                int end = body.indexOf('\"', pos);
                if (end > pos) {
                    QString cc = QString::fromUtf8(body.mid(pos, end - pos)).trimmed().toUpper();
                    return (cc.length() == 2) ? cc : QString();
                }
            }
        }
        return {};
    }
    pos += needle.size();
    int end = body.indexOf('\"', pos);
    if (end < 0) return {};
    QString cc = QString::fromUtf8(body.mid(pos, end - pos)).trimmed().toUpper();
    return (cc.length() == 2) ? cc : QString();
}

// ── Discover country via HTTPS GeoIP (4 providers: 2 CN + 2 global) ──
// 5WHY: DNS gives us the public IP.  Now we feed that IP into 4 HTTPS
// GeoIP APIs.  CN providers (ip.sb, ipip.net) have CDN nodes inside China
// so are GFW-safe.  Global providers (ipapi.co, country.is) work outside.
// Tried sequentially with 2s timeout each — first match wins.
static QString discoverCountryViaHttps(const QString& ip) {
    struct GeoProvider { QString host; int port; QString pathFmt; QString jsonKey; };
    const GeoProvider providers[] = {
        // ── Domestic (CN CDN, GFW-safe) ──
        {QStringLiteral("api.ip.sb"), 443,
         QStringLiteral("/geoip/%1").arg(ip), QStringLiteral("country_code")},
        {QStringLiteral("freeapi.ipip.net"), 443,
         QStringLiteral("/%1").arg(ip), QStringLiteral("_array_")},
        // ── Global ──
        {QStringLiteral("ipapi.co"), 443,
         QStringLiteral("/%1/country/").arg(ip), QString()},  // plain text
        {QStringLiteral("api.country.is"), 443,
         QStringLiteral("/%1").arg(ip), QStringLiteral("country")},
    };
    for (const auto& p : providers) {
        QString cc = geoIpLookupHttps(p.host, p.port, p.pathFmt, p.jsonKey, 2000);
        if (!cc.isEmpty()) return cc;
    }
    return {};
}

// ── Static IP prefix → country (absolute last resort) ──
// 5WHY: When ALL network GeoIP fails, use the public IP's first octet
// to guess the country.  Covers ~80% of GFW scenarios where user has a
// Chinese IP but GeoIP endpoints are all blocked.
static QString countryFromIpPrefix(const QString& ip) {
    if (ip.isEmpty() || ip == QStringLiteral("127.0.0.1")) return {};
    int first = ip.section('.', 0, 0).toInt();
    // 5WHY: first==1 excluded — 1.0.0.0/8 includes Cloudflare (1.1.1.1)
    // and APNIC research nets, too broad for reliable CN detection.
    if (first == 14 || first == 27 ||
        first == 36 || first == 39 || first == 42 || first == 49 ||
        (first >= 58 && first <= 61) ||
        (first >= 101 && first <= 126) ||
        first == 139 || first == 171 || first == 175 || first == 180 ||
        first == 182 || first == 183 ||
        (first >= 202 && first <= 203) ||
        (first >= 210 && first <= 211) ||
        (first >= 218 && first <= 223))
        return QStringLiteral("CN");
    return {};
}

QString SpeedTest::detectCountry(int timeoutMs) {
    // 5WHY: Every speed test run made 2-3 GeoIP HTTP calls (~6s each to fail).
    // Cache SUCCESSFUL results for the process lifetime (country doesn't change).
    // Do NOT cache "XX" failures -- network may recover, so retry on each run.
    // 5WHY (2nd): static QString is not thread-safe — concurrent reads on the
    // ref-counted d-pointer can tear. Guard with a mutex.
    static QString sCachedCountry;
    static QMutex sCountryMutex;
    {
        QMutexLocker lock(&sCountryMutex);
        if (!sCachedCountry.isEmpty() && sCachedCountry != QStringLiteral("XX"))
            return sCachedCountry;
    }
    // 5WHY: Full chain: DNS(IP) → HTTPS GeoIP(4 providers) → prefix
    // fallback.  DNS (UDP 53) reveals public IP; HTTPS GeoIP maps IP→
    // country using 2 CN + 2 global providers; prefix mapping is the
    // absolute last resort when ALL network services are blocked.

    // Phase 1a: Raw DNS query (UDP 53 — rarely blocked)
    int dnsTimeout = (timeoutMs > 0) ? qMin(timeoutMs, 2000) : 2000;
    QString ip = discoverPublicIpViaDns(dnsTimeout);
    // Phase 1b: DoH fallback (HTTPS DNS — TCP 443, almost never blocked)
    if (ip.isEmpty())
        ip = discoverPublicIpViaDoh(timeoutMs > 0 ? timeoutMs : 3000);

    // Phase 2: HTTPS GeoIP with the discovered IP (2 CN + 2 global)
    if (!ip.isEmpty()) {
        QString cc = discoverCountryViaHttps(ip);
        if (!cc.isEmpty()) {
            QMutexLocker l(&sCountryMutex);
            sCachedCountry = cc;
            return sCachedCountry;
        }
        // Phase 3: Static IP prefix as last resort
        cc = countryFromIpPrefix(ip);
        if (!cc.isEmpty()) {
            QMutexLocker l(&sCountryMutex);
            sCachedCountry = cc;
            return sCachedCountry;
        }
    }

    Logger::instance().event(QStringLiteral("GeoIP: DNS/HTTPS/prefix all failed, country=XX"));
    return QStringLiteral("XX");
}

} // namespace G1G2G3Native
