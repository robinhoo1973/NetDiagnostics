#include "Diagnostics/Model/GHelpers.h"
#include <QSslSocket>
#include <algorithm>
#include <random>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <cstring>

namespace G1G2G3Native {

class SpeedTest {
public:
    struct Server { QString host; int port; QString name, sponsor, country, url; };
    SpeedTest();
    QVector<Server> serversForCountry(const QString& hint) const;
    QVector<Server> allServers() const;
    static QString detectCountry(int = 3000);
private: void build(); QMap<QString, QVector<Server>> m;
};
inline SpeedTest::SpeedTest() { build(); }
#define S(c, h, p, n, sp) s.host=h; s.port=p; s.name=n; s.sponsor=sp; s.country=c; s.url=QStringLiteral("http://%1:%2").arg(h).arg(p); m[c].append(s);
inline void SpeedTest::build() { Server s;
    S("CN","speedtest1.gd.chinamobile.com",8080,"Guangzhou","China Mobile");
    S("CN","speedtest2.gz.chinamobile.com",8080,"Guangzhou 2","China Mobile");
    S("CN","speedtest.bj.chinamobile.com",8080,"Beijing","China Mobile");
    S("CN","speedtest2.fj.chinamobile.com",8080,"Fujian","China Mobile");
    S("CN","speedtest.sc.chinamobile.com",8080,"Sichuan","China Mobile");
    S("CN","speedtest.hb.chinamobile.com",8080,"Hubei","China Mobile");
    S("CN","speedtest.zj.chinamobile.com",8080,"Zhejiang","China Mobile");
    S("CN","speedtest.jl.chinamobile.com",8080,"Jilin","China Mobile");
    S("CN","speedtest1.online.sh.cn",8080,"Shanghai","China Telecom");
    S("CN","speedtest2.online.sh.cn",8080,"Shanghai 2","China Telecom");
    S("CN","speedtest1.gx.chinatel.com.cn",8080,"Guangxi","China Telecom");
    S("CN","speedtest1.ah.chinatel.com.cn",8080,"Anhui","China Telecom");
    S("CN","speedtest1.js.chinatel.com.cn",8080,"Jiangsu","China Telecom");
    S("CN","speedtest1.zj.chinatel.com.cn",8080,"Zhejiang","China Telecom");
    S("CN","speedtest1.cq.chinatel.com.cn",8080,"Chongqing","China Telecom");
    S("CN","speedtest1.hb.cnc.cn",8080,"Hubei","China Unicom");
    S("CN","speedtest1.bj.cnc.cn",8080,"Beijing","China Unicom");
    S("CN","speedtest1.sh.cnc.cn",8080,"Shanghai","China Unicom");
    S("CN","speedtest1.gd.cnc.cn",8080,"Guangdong","China Unicom");
    S("CN","speedtest-js.volcengine.com",8080,"Jiangsu","Volcengine");
    S("CN","speedtest-hb.volcengine.com",8080,"Hubei","Volcengine");
    S("CN","speedtest-zj.volcengine.com",8080,"Zhejiang","Volcengine");
    S("CN","speedtest-bj.oss-cn-beijing.aliyuncs.com",80,"Beijing","Alibaba Cloud");
    S("CN","speedtest-sh.oss-cn-shanghai.aliyuncs.com",80,"Shanghai","Alibaba Cloud");
    S("CN","speedtest-gz.oss-cn-guangzhou.aliyuncs.com",80,"Guangzhou","Alibaba Cloud");
    S("CN","speedtest-bj-ct.oss-cn-beijing.aliyuncs.com",80,"Beijing CT","Alibaba Cloud");
    // === Global speed test servers — verified against Ookla API 2024-2025 ===
    // All use port 8080 with HTTP-based protocol (/download?size=N, /upload)
    //
    // -- East Asia --
    S("KR","seoul.speedtest.gslnetworks.com",8080,"Seoul","GSL Networks");
    S("MN","speedtest.gemnet.mn",8080,"Ulaanbaatar","Gemnet");
    S("MN","speedtest1.kewiko.mn",8080,"Ulaanbaatar 2","Kewiko");

    // -- Southeast Asia / Oceania --
    S("SG","speedtest.singtel.com",8080,"Singapore","Singtel");
    S("SG","speedtest.myrepublic.net",8080,"Singapore 2","MyRepublic");
    S("AU","speedtest.telstra.net",8080,"Sydney","Telstra");
    S("AU","speedtest.vocus.com.au",8080,"Melbourne","Vocus");

    // -- South Asia --
    S("IN","speedtest.actcorp.in",8080,"Bangalore","ACT Fibernet");
    S("IN","speedtestpnq.airstel.com",8080,"Pune","Airtel");

    // -- Middle East --
    S("AE","speedtest.du.ae",8080,"Dubai","du");
    S("TR","hiztesti.isimkayit.com",8080,"Kocaeli","IsimKayit.com");

    // -- Russia / CIS --
    S("RU","speedtest-ude.edinos.ru",8080,"Ulan-Ude","EDINOS");
    S("RU","speedtest.bteleport.ru",8080,"Irkutsk","Baikal Teleport");
    S("RU","speedtest-irkutsk.fttb.beeline.ru",8080,"Irkutsk 2","Beeline");

    // -- Europe --
    S("SE","speedtest.tele2.net",8080,"Stockholm","Tele2 Sweden");
    S("DE","speedtest.belwue.net",8080,"Stuttgart","BelWue");
    S("DE","speedtest.ftp.otenet.gr",8080,"Frankfurt","OTE");
    S("GB","speedtest1.sky.com",8080,"London","Sky Broadband");
    S("NL","speedtest.ams1.nl.leaseweb.net",8080,"Amsterdam","Leaseweb");
    S("FR","speedtest.orange.fr",8080,"Paris","Orange France");
    S("IT","speedtest.optimaitalia.com",8080,"Milan","Optima Italia");
    S("ES","speedtest.movistar.es",8080,"Madrid","Movistar");

    // -- North America --
    S("US","speedtest.xfinity.com",8080,"New York","Comcast Xfinity");
    S("US","speedtest.att.com",8080,"Dallas","AT&T");
    S("US","speedtest.netturbo.com",8080,"Chicago","NetTurbo");
    S("CA","speedtest.bell.ca",8080,"Toronto","Bell Canada");

    // -- South America --
    S("BR","speedtest.vivo.com.br",8080,"Sao Paulo","Vivo Brazil");
    S("AR","speedtest.movistar.com.ar",8080,"Buenos Aires","Movistar");

    // -- Africa --
    S("ZA","speedtest.mtn.co.za",8080,"Johannesburg","MTN South Africa");
    S("ZA","speedtest.vodacom.co.za",8080,"Cape Town","Vodacom");
}
#undef S
// Continent fallback: map country codes to nearby regions with server coverage
static const QMap<QString, QStringList> continentFallback = {
    {"AS", {"CN","KR","SG","IN","JP","AE"}},  // Asia
    {"EU", {"DE","GB","FR","NL","IT","ES","SE","RU"}},  // Europe
    {"NA", {"US","CA"}},                       // North America
    {"SA", {"BR","AR"}},                       // South America
    {"AF", {"ZA"}},                            // Africa
    {"OC", {"AU","SG"}},                       // Oceania (SG as fallback)
};
static QString continentForCountry(const QString& cc) {
    static const QMap<QString, QString> ccContinent = {
        {"CN","AS"},{"KR","AS"},{"SG","AS"},{"IN","AS"},{"JP","AS"},{"MN","AS"},
        {"AE","AS"},{"TR","AS"},{"HK","AS"},{"TW","AS"},{"TH","AS"},{"VN","AS"},
        {"MY","AS"},{"ID","AS"},{"PH","AS"},{"PK","AS"},{"BD","AS"},{"LK","AS"},
        {"RU","EU"},{"DE","EU"},{"GB","EU"},{"FR","EU"},{"NL","EU"},{"IT","EU"},
        {"ES","EU"},{"SE","EU"},{"PL","EU"},{"UA","EU"},{"CH","EU"},{"AT","EU"},
        {"BE","EU"},{"NO","EU"},{"FI","EU"},{"DK","EU"},{"IE","EU"},{"PT","EU"},
        {"US","NA"},{"CA","NA"},{"MX","NA"},
        {"BR","SA"},{"AR","SA"},{"CO","SA"},{"CL","SA"},{"PE","SA"},
        {"ZA","AF"},{"NG","AF"},{"KE","AF"},{"EG","AF"},
        {"AU","OC"},{"NZ","OC"},
    };
    return ccContinent.value(cc, "XX");
}
inline QVector<SpeedTest::Server> SpeedTest::serversForCountry(const QString& hint) const {
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
inline QVector<SpeedTest::Server> SpeedTest::allServers() const {
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

inline QString SpeedTest::detectCountry(int timeoutMs) {
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
DiagnosticResult speedTest(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer totalTimer; totalTimer.start();
    QStringList out;

    out.append(QString());
    out.append(QStringLiteral("Internet Connectivity"));
    out.append(QStringLiteral("Protocol: Speedtest.net (Ookla-compatible)"));
    out.append(QString());

    // === Phase 0: Quick connectivity check (TCP to well-known hosts) ===
    out.append(QStringLiteral("--- Connectivity Check -------------------------------------------------"));
    out.append(QString());
    out.append(QStringLiteral("  %1  %2  %3  %4  %5")
        .arg(QStringLiteral("Host").leftJustified(16, ' '))
        .arg(QStringLiteral("Address").leftJustified(15, ' '))
        .arg(QStringLiteral("Port").rightJustified(5, ' '))
        .arg(QStringLiteral("Status").leftJustified(6, ' '))
        .arg(QStringLiteral("Latency").rightJustified(7, ' ')));
    out.append(QStringLiteral("  %1  %2  %3  %4  %5")
        .arg(QString(16, QChar('-')))
        .arg(QString(15, QChar('-')))
        .arg(QString(5, QChar('-')))
        .arg(QString(6, QChar('-')))
        .arg(QString(7, QChar('-'))));

    // Connectivity check: region-diverse hosts (China primary, global fallback)
    struct { const char* host; int port; const char* name; } checkSites[] = {
        {"223.5.5.5", 53, "Alibaba DNS"},
        {"119.29.29.29", 53, "DNSPod DNS"},
        {"baidu.com", 443, "Baidu"},
        {"208.67.222.222", 53, "OpenDNS"},
        {"114.114.114.114", 53, "114DNS"},
    };
    int connOk = 0;
    // 5WHY: Track China-specific connectivity - if Chinese sites respond
    // but global ones don't, we're inside GFW and should prioritize CN servers
    // regardless of GeoIP result.
    int connChina = 0;  // Alibaba DNS, DNSPod DNS, Baidu, 114DNS
    int connGlobal = 0; // OpenDNS
    for (auto& cs : checkSites) {
        int p = tcpPingMs(cs.host, cs.port);
        QString status, latency;
        if (p >= 0) { status = QStringLiteral("[OK]"); latency = QStringLiteral("%1 ms").arg(p); connOk++;
            // Classify by region: all non-OpenDNS sites are China-based
            if (std::strcmp(cs.name, "OpenDNS") == 0) connGlobal++;
            else connChina++;
        }
        else        { status = QStringLiteral("[FAIL]"); latency = QStringLiteral("-"); }
        out.append(QStringLiteral("  %1  %2  %3  %4  %5")
            .arg(QString::fromUtf8(cs.name).leftJustified(16, ' '))
            .arg(QString::fromUtf8(cs.host).leftJustified(15, ' '))
            .arg(cs.port, 5)
            .arg(status.leftJustified(6, ' '))
            .arg(latency.rightJustified(7, ' ')));
    }
    bool hasConnectivity = (connOk > 0);
    out.append(QString());
    out.append(QStringLiteral("------------------------------------------------------------------"));
    out.append(QStringLiteral("  Result: %1").arg(hasConnectivity
        ? QStringLiteral("CONNECTED (%1/%2)").arg(connOk).arg((int)(sizeof(checkSites)/sizeof(checkSites[0])))
        : QStringLiteral("DISCONNECTED")));
    out.append(QString());

    // === Phase 1: Detect country ===
    SpeedTest st;
    QString country = SpeedTest::detectCountry(3000);
    out.append(QStringLiteral("Detected country: %1").arg(country == "XX" ? "Unknown" : country));

    // Timeout guard: if we've already spent >25s, skip speed measurement
    if (totalTimer.elapsed() > 25000) {
        out.append(QString());
        out.append(QStringLiteral("  (Speed test skipped: connectivity check took too long)"));
        r.rawOutput = out.join('\n'); r.details = r.rawOutput;
        r.status = hasConnectivity ? DiagStatus::Warning : DiagStatus::Fail;
        r.summary = hasConnectivity ? QStringLiteral("Connected -- speed test timed out") : QStringLiteral("No internet");
        r.durationMs = totalTimer.elapsed(); return r;
    }

    // === Phase 2: Build candidate pool (always includes ALL CN servers) ===
    // 5WHY: TCP ping screening is useless — all CN servers show ~1-5ms
    // TCP RTT with zero discrimination.  TCP connectivity != download
    // capability (common on CN servers).  Now uses actual micro-downloads
    // (100KB) as the sole screening signal.  Also: always merge ALL CN
    // servers into the pool regardless of GeoIP — handles VPN users whose
    // GeoIP says "US" but are physically in China.
    // 5WHY: Country Unknown with China connectivity = likely GFW user.
    if (country == QStringLiteral("XX") && connChina >= 3) {
        out.append(QStringLiteral("  (GeoIP failed, but %1/%2 China sites reachable)").arg(connChina).arg(connChina + connGlobal));
        out.append(QStringLiteral("  Using connectivity-guided server selection)"));
    }

    // Build candidate pool: always CN + region + global diversity backup.
    // 5WHY: when country=CN, candidates were 100% CN → all unreachable if
    // user is outside China (VPN CN IP but physically overseas).  Now always
    // includes a small global diversity set as insurance, regardless of GeoIP.
    QVector<SpeedTest::Server> candidates;
    {
        // Tier 1: ALL CN servers (26), always — handles VPN misdetection
        {
            QVector<SpeedTest::Server> cnServers = st.serversForCountry(QStringLiteral("CN"));
            candidates.append(cnServers);
        }
        // Tier 2: Region servers (skip CN duplicates, added for non-XX countries)
        if (country != QStringLiteral("XX")) {
            QVector<SpeedTest::Server> region = st.serversForCountry(country);
            for (const auto& s : region) {
                if (s.country != QStringLiteral("CN"))
                    candidates.append(s);
            }
        }
        // Tier 3: Global diversity backup (4 servers, always included as insurance)
        {
            static const QStringList kGlobalBackup = {
                QStringLiteral("speedtest.tele2.net"),       // Stockholm
                QStringLiteral("speedtest.singtel.com"),     // Singapore
                QStringLiteral("speedtest.belwue.net"),      // Stuttgart
                QStringLiteral("seoul.speedtest.gslnetworks.com"), // Seoul
            };
            QVector<SpeedTest::Server> allSrv = st.allServers();
            for (const auto& s : allSrv) {
                if (kGlobalBackup.contains(s.host)) {
                    if (s.country != QStringLiteral("CN") ||
                        !std::any_of(candidates.begin(), candidates.end(),
                            [&](const SpeedTest::Server& c) { return c.host == s.host; }))
                        candidates.append(s);
                }
            }
        }
        // Shuffle for geographic diversity
        {
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(candidates.begin(), candidates.end(), g);
        }
        // Cap at 15 (was 12 — need room for global backup diversity)
        if (candidates.size() > 15)
            candidates.resize(15);
    }
    out.append(QStringLiteral("Candidate pool: %1 servers (%2 CN, country=%3)")
        .arg(candidates.size())
        .arg(candidates.isEmpty() ? 0 : std::count_if(candidates.begin(), candidates.end(), [](const SpeedTest::Server& s) { return s.country == QStringLiteral("CN"); }))
        .arg(country));

    // === Phase 3: TCP fast screening (with 50x avg for sub-ms servers) ===
    // 5WHY: TCP ping is cheap (~1ms each) but can't differentiate servers
    // with ≤1ms RTT.  For those, run 50 connects and average to get
    // ~0.02ms effective resolution.  Feeds top-8 into the expensive
    // 100KB micro-download phase.
    out.append(QStringLiteral("--- TCP Screening (50x avg for ≤1ms) ---------------------------------"));
    out.append(QString());
    out.append(QStringLiteral("  %1  %2  %3  %4")
        .arg(QStringLiteral("#").rightJustified(3, ' '))
        .arg(QStringLiteral("Sponsor").leftJustified(22, ' '))
        .arg(QStringLiteral("Server").leftJustified(17, ' '))
        .arg(QStringLiteral("TCP(ms)").rightJustified(8, ' ')));
    out.append(QStringLiteral("  %1  %2  %3  %4")
        .arg(QString(3, QChar('-')))
        .arg(QString(22, QChar('-')))
        .arg(QString(17, QChar('-')))
        .arg(QString(8, QChar('-'))));

    struct TcpResult { SpeedTest::Server* srv; double latencyMs; };
    QVector<TcpResult> tcpRanked;

    for (auto& s : candidates) {
        double lat = tcpPingAvg(s.host, s.port);
        // 5WHY: lat>0 filtered sub-ms connects returning 0 (truncation).
        // tcpPingAvg already returns -1 for true failures; >= 0 is success.
        if (lat >= 0.0)
            tcpRanked.append({&s, lat});
    }

    if (tcpRanked.isEmpty()) {
        out.append(QStringLiteral("  (no servers reachable via TCP)"));
        out.append(QString());
        r.rawOutput = out.join('\n'); r.details = r.rawOutput;
        r.status = hasConnectivity ? DiagStatus::Warning : DiagStatus::Fail;
        r.summary = hasConnectivity ? QStringLiteral("Connected -- no servers reachable via TCP")
                                    : QStringLiteral("No internet connectivity");
        r.durationMs = totalTimer.elapsed(); return r;
    }

    // Sort by TCP latency ascending
    std::sort(tcpRanked.begin(), tcpRanked.end(),
              [](const TcpResult& a, const TcpResult& b) { return a.latencyMs < b.latencyMs; });

    // Output sorted TCP results
    for (int i = 0; i < tcpRanked.size(); i++) {
        auto& tr = tcpRanked[i];
        out.append(QStringLiteral("  %1  %2  %3  %4")
            .arg(i + 1, 3)
            .arg(tr.srv->sponsor.leftJustified(22, ' '))
            .arg(tr.srv->name.leftJustified(17, ' '))
            .arg(QStringLiteral("%1").arg(tr.latencyMs, 0, 'f', 2).rightJustified(8, ' ')));
    }

    // Take top 8 for micro-download (expensive phase)
    int topN = qMin(8, (int)tcpRanked.size());
    out.append(QString());
    out.append(QStringLiteral("  Top %1 by TCP latency → micro-download screening").arg(topN));

    // === Phase 4: Micro-download ranking (100KB on top 8 by TCP) ===
    // 5WHY: 100KB micro-download simultaneously verifies reachability AND
    // measures throughput — the only reliable screening signal.  TCP ping
    // was removed: all servers showed ~1ms with no discrimination, and
    // TCP connectivity had zero correlation with download capability.
    out.append(QStringLiteral("--- Server Selection (100KB micro-download) --------------------------"));
    out.append(QString());
    out.append(QStringLiteral("  %1  %2  %3  %4")
        .arg(QStringLiteral("#").rightJustified(3, ' '))
        .arg(QStringLiteral("Sponsor").leftJustified(22, ' '))
        .arg(QStringLiteral("Server").leftJustified(17, ' '))
        .arg(QStringLiteral("Throughput").rightJustified(10, ' ')));
    out.append(QStringLiteral("  %1  %2  %3  %4")
        .arg(QString(3, QChar('-')))
        .arg(QString(22, QChar('-')))
        .arg(QString(17, QChar('-')))
        .arg(QString(10, QChar('-'))));

    struct CandidateResult { SpeedTest::Server* srv; double mbps; double latencyMs; };
    QVector<CandidateResult> results;

    for (int i = 0; i < topN; i++) {
        auto& s = *tcpRanked[i].srv;
        if (totalTimer.elapsed() > 55000) break; // 55s wall-clock guard
        QString probeUrl = QStringLiteral("%1/download?size=%2").arg(s.url).arg(100000);
        auto res = httpDownload(probeUrl, 100000, 6000);
        if (res.ok && res.mbps > 0.01) {
            results.append({&s, res.mbps, tcpRanked[i].latencyMs});
            out.append(QStringLiteral("  %1  %2  %3  %4")
                .arg(results.size(), 3)
                .arg(s.sponsor.leftJustified(22, ' '))
                .arg(s.name.leftJustified(17, ' '))
                .arg(QStringLiteral("%1 Mbit/s").arg(res.mbps, 0, 'f', 2).rightJustified(10, ' ')));
            if (results.size() >= 8) break; // enough candidates
        }
    }

    if (results.isEmpty()) {
        out.append(QStringLiteral("  (no servers passed micro-download screening)"));
        out.append(QString());
        r.rawOutput = out.join('\n'); r.details = r.rawOutput;
        r.status = hasConnectivity ? DiagStatus::Warning : DiagStatus::Fail;
        r.summary = hasConnectivity ? QStringLiteral("Connected -- no servers passed 100KB download")
                                    : QStringLiteral("No internet connectivity");
        r.durationMs = totalTimer.elapsed(); return r;
    }

    // Rank by throughput (descending)
    std::sort(results.begin(), results.end(),
              [](const CandidateResult& a, const CandidateResult& b) { return a.mbps > b.mbps; });

    // === Phase 5: Pre-validation (250KB on top 2) ===
    // 5WHY: 100KB proves basic reachability but some servers accept small
    // downloads and fail on larger ones.  250KB pre-validation on the top-2
    // candidates catches this.
    SpeedTest::Server* best = nullptr;
    double bestMbps = 0;
    double bestLatency = 0;
    for (int i = 0; i < qMin(2, (int)results.size()); i++) {
        auto& cr = results[i];
        QString valUrl = QStringLiteral("%1/download?size=%2").arg(cr.srv->url).arg(250000);
        auto valRes = httpDownload(valUrl, 250000, 8000);
        if (valRes.ok && valRes.mbps > 0.01) {
            best = cr.srv;
            bestMbps = cr.mbps;
            bestLatency = cr.latencyMs;
            break;
        }
        out.append(QStringLiteral("  (server %1 failed 250KB validation)").arg(cr.srv->sponsor));
    }

    if (!best) {
        out.append(QString());
        r.rawOutput = out.join('\n'); r.details = r.rawOutput;
        r.status = hasConnectivity ? DiagStatus::Warning : DiagStatus::Fail;
        r.summary = QStringLiteral("Connected -- all top servers failed pre-validation");
        r.durationMs = totalTimer.elapsed(); return r;
    }

    out.append(QString());
    out.append(QStringLiteral("------------------------------------------------------------------"));
    out.append(QStringLiteral("  Selected: %1 (%2, %3) -- %4 Mbit/s (100KB)")
        .arg(best->sponsor)
        .arg(best->name)
        .arg(best->country)
        .arg(bestMbps, 0, 'f', 2));
    out.append(QString());

    // === Phase 6: Download test (with server fallback) ===
    out.append(QString());
    out.append(QStringLiteral("--- Download Test ------------------------------------------------"));
    out.append(QString());
    out.append(QStringLiteral("  Server: %1").arg(best->host));
    out.append(QString());
    out.append(QStringLiteral("  %1  %2  %3")
        .arg(QStringLiteral("Size").rightJustified(10, ' '))
        .arg(QStringLiteral("Throughput").rightJustified(16, ' '))
        .arg(QStringLiteral("Time").rightJustified(8, ' ')));
    out.append(QStringLiteral("  %1  %2  %3")
        .arg(QString(10, QChar('-')))
        .arg(QString(16, QChar('-')))
        .arg(QString(8, QChar('-'))));

    // Progressive download sizes (KB): start small, ramp up
    int dlSizes[] = {250, 500, 750, 1000, 1500, 2000, 2500, 3000, 4000, 5000, 7500, 10000, 15000, 20000, 25000};
    QVector<double> dlResults;
    int dlTotalBytes = 0, dlTotalMs = 0;
    int dlServerIdx = 0; // current preferred server index into results[]

    for (int sizeKb : dlSizes) {
        // 5WHY: dlTotalMs only accumulates successful download time — failed
        // HTTP timeouts (4s × 8 servers × 15 tiers = 480s worst case) are NOT
        // counted, so the dlTotalMs > 12000 guard was useless when all servers
        // are unreachable (Country Unknown + GFW). Added totalTimer.elapsed()
        // hard wall-clock guard (45s) so the download phase always completes
        // within the 180s diagnostic watchdog budget.
        if (dlTotalMs > 12000) break;        // cumulative download time cap
        if (totalTimer.elapsed() > 45000) break; // wall-clock hard cap

        // Try preferred server first, fall back through ranked list independently
        // per size tier 闂?a server that handles 250KB may choke on 25MB.
        bool ok = false;
        // 5WHY: cap fallback at 3 servers per tier — reduces worst-case
        // timeout from 8 × 4s = 32s to 3 × 4s = 12s per tier when all
        // servers are unreachable (Country Unknown + GFW).
        int maxFallback = qMin(3, (int)results.size());
        for (int si = 0; si < maxFallback; si++) {
            // 5WHY: bail early if we have exceeded total time budget.
            if (totalTimer.elapsed() > 45000) break;
            // Try each server once before marking failure
            int idx = (dlServerIdx + si) % results.size();
            SpeedTest::Server* srv = results[idx].srv;
            QString dlUrl = QStringLiteral("%1/download?size=%2").arg(srv->url).arg(sizeKb * 1000);
            auto res = httpDownload(dlUrl, sizeKb * 1000, 8000);
            if (res.ok && res.mbps > 0.01) {
                dlResults.append(res.mbps);
                dlTotalBytes += res.bytes;
                dlTotalMs += res.durationMs;
                if (idx != dlServerIdx) {
                    out.append(QStringLiteral("  (switched to %1)").arg(srv->sponsor));
                    best = srv; bestLatency = results[idx].latencyMs;
                    dlServerIdx = idx; // make this the new preferred server
                }
                out.append(QStringLiteral("  %1  %2  %3")
                    .arg(QStringLiteral("%1 KB").arg(sizeKb).rightJustified(10, ' '))
                    .arg(QStringLiteral("%1 Mbit/s").arg(res.mbps, 0, 'f', 2).rightJustified(16, ' '))
                    .arg(QStringLiteral("%1 ms").arg(res.durationMs).rightJustified(8, ' ')));
                ok = true;
                break;
            }
        }
        if (!ok) {
            out.append(QStringLiteral("  %1  %2  %3")
                .arg(QStringLiteral("%1 KB").arg(sizeKb).rightJustified(10, ' '))
                .arg(QStringLiteral("(timeout)").rightJustified(16, ' '))
                .arg(QStringLiteral("-").rightJustified(8, ' ')));
            // Don't abort 闂?try next size tier even if this one failed
        }
    }

    auto avgTopN = [](QVector<double>& v, int n = 5) -> double {
        if (v.isEmpty()) return 0;
        std::sort(v.begin(), v.end());
        int count = qMin(n, v.size());
        double sum = 0;
        for (auto i = v.size() - count; i < v.size(); i++) sum += v[i];
        return sum / count;
    };
    double dlSpeed = avgTopN(dlResults);

    out.append(QString());
    out.append(QStringLiteral("------------------------------------------------------------------"));
    out.append(QStringLiteral("  Download: %1 Mbit/s%2")
        .arg(dlSpeed, 0, 'f', 2)
        .arg(dlResults.size() >= 5 ? QStringLiteral("  (avg of top %1)").arg(qMin(5, (int)dlResults.size())) : QString()));

    // === Phase 7: Upload test ===
    out.append(QString());
    out.append(QStringLiteral("--- Upload Test --------------------------------------------------"));
    out.append(QString());
    out.append(QStringLiteral("  %1  %2  %3")
        .arg(QStringLiteral("Size").rightJustified(10, ' '))
        .arg(QStringLiteral("Throughput").rightJustified(16, ' '))
        .arg(QStringLiteral("Time").rightJustified(8, ' ')));
    out.append(QStringLiteral("  %1  %2  %3")
        .arg(QString(10, QChar('-')))
        .arg(QString(16, QChar('-')))
        .arg(QString(8, QChar('-'))));

    // Upload test: POST random data, increasing sizes
    int ulSizes[] = {100, 250, 500, 750, 1000, 1500, 2000, 2500, 3000, 4000};
    QVector<double> ulResults;
    int ulTotalMs = 0;

    // 5WHY: Upload test had NO cumulative time cap — 10 size tiers ×
    // up to 30s each = 300s worst case. The 180s watchdog fires AFTER the
    // function returns, not during the loop. Added 15s cumulative cap so
    // the upload phase completes within a reasonable total time budget.
    // 5WHY: Pre-generate upload data once (max 4000KB), reuse left() slices
    // per tier.  Was generating ~40MB total across 10 tiers; now ~4MB once.
    int maxUlSize = ulSizes[sizeof(ulSizes)/sizeof(ulSizes[0]) - 1] * 1000; // 4000KB
    QByteArray uploadData(maxUlSize, '\0');
    {
        thread_local uint64_t lcgState = static_cast<uint64_t>(
            QDateTime::currentMSecsSinceEpoch()) ^
            (reinterpret_cast<uintptr_t>(&lcgState) << 16);
        for (int i = 0; i < maxUlSize; i++) {
            lcgState = lcgState * 6364136223846793005ULL + 1442695040888963407ULL;
            uploadData[i] = static_cast<char>(33 + static_cast<int>(lcgState % 94));
        }
    }
    for (int sizeKb : ulSizes) {
        if (ulTotalMs > 15000) break;
        if (totalTimer.elapsed() > 70000) break; // wall-clock hard cap (70s total, upload stays within 180s watchdog)
        int dataSize = sizeKb * 1000;

        // HTTP POST with measured upload
        // 5WHY: Raw socket→connect→select replaced with tcpConnect()
        // helper (NetUtil.h). Same 10s timeout, eliminates 15 lines of
        // duplicated boilerplate.
        int sock = tcpConnect(best->host, best->port, 10000);
        if (sock < 0) continue;

        // POST request headers (upload data pre-generated above)
        // 5WHY: Host header missing port — same bug fixed in httpGet/httpDownload.
        // Speed-test servers on port 8080 behind reverse proxies may route
        // incorrectly without explicit port per RFC 7230 §5.4.
        QString uploadHost = (best->port != 80) ? QStringLiteral("%1:%2").arg(best->host).arg(best->port) : best->host;
        QByteArray postHeaders = QStringLiteral("POST /upload HTTP/1.0\r\nHost: %1\r\nContent-Type: application/octet-stream\r\nContent-Length: %2\r\nConnection: close\r\n\r\n")
            .arg(uploadHost).arg(dataSize).toUtf8();

        QElapsedTimer ulTimer; ulTimer.start();
        // Send POST headers (EAGAIN-safe: select() for writability on stall).
        // 5WHY: The 180s diagnostic task watchdog fires AFTER the function
        // returns, which doesn't help an infinite retry loop here.  30s is
        // generous for a few KB of headers on any realistic connection.
        int hdrSent = 0;
        QElapsedTimer hdrSendGuard; hdrSendGuard.start();
        while (hdrSent < postHeaders.size()) {
            if (hdrSendGuard.elapsed() > 30000) break;
            auto n = ::send(sock, postHeaders.constData() + hdrSent, postHeaders.size() - hdrSent, 0);
            if (n < 0) {
#if defined(_WIN32)
                if (WSAGetLastError() == WSAEWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); timeval hdrTv={1,0}; select(sock+1,nullptr,&wf,nullptr,&hdrTv); continue; }
#else
                if (errno == EAGAIN || errno == EWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); timeval hdrTv={1,0}; select(sock+1,nullptr,&wf,nullptr,&hdrTv); continue; }
#endif
                break;
            }
            if (n == 0) break;
            hdrSent += n;
        }
        // 5WHY: If header send timed out before completion, the server
        // received a truncated HTTP request. Sending body data to a server
        // that's waiting for the rest of the headers produces garbage.
        // Skip the body send and mark the upload as failed.
        bool headerComplete = (hdrSent == postHeaders.size());
        // Send body in chunks (EAGAIN-safe: select() for writability on stall)
        // 5WHY: body-send loop had NO wall-clock guard — a stalled server
        // would cause an infinite while-loop. The 180s diagnostic task watchdog
        // fires AFTER the function returns, so it cannot interrupt an in-progress
        // infinite loop.  Added 30s body-send guard matching the header-send guard.
        int sent = 0; const char* dp = uploadData.constData();
        QElapsedTimer bodySendGuard; bodySendGuard.start();
        if (headerComplete) {
        while (sent < dataSize) {
            if (bodySendGuard.elapsed() > 30000) break;
            int chunk = qMin(dataSize - sent, 32768);
            auto n = ::send(sock, dp + sent, chunk, 0);
            if (n < 0) {
#if defined(_WIN32)
                if (WSAGetLastError() == WSAEWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); timeval tv2={2,0}; select(sock+1,nullptr,&wf,nullptr,&tv2); continue; }
#else
                if (errno == EAGAIN || errno == EWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); timeval tv2={2,0}; select(sock+1,nullptr,&wf,nullptr,&tv2); continue; }
#endif
                break;
            }
            if (n == 0) break;
            sent += n;
        }
        } // headerComplete
        // Read response -- only meaningful if headers were fully sent.
        // 5WHY: If header send timed out (headerComplete=false), the server
        // received a truncated HTTP request. Skip response read and mark
        // the entire upload tier as failed.
        bool uploadOk = headerComplete;
        if (headerComplete) {
        char buf[4096];
        fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
        struct timeval tv = {30, 0};  // generous 30s response read timeout
        int selRet = select(sock + 1, &fdset, nullptr, nullptr, &tv);
        if (selRet > 0 && FD_ISSET(sock, &fdset)) {
            ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
            if (n > 0) {
                buf[n] = '\0';
                QByteArray rsp(buf, (int)n);
                // If response looks like HTTP, validate status line.
                // 5WHY: required exactly " 200 " — many speed-test servers
                // return 201 Created or 204 No Content for POST /upload.
                // Accept any 2xx status (success) instead of only 200.
                if (rsp.startsWith("HTTP/")) {
                    int slEnd = rsp.indexOf('\r');
                    QByteArray sl = (slEnd > 0) ? rsp.left(slEnd) : rsp.left(24);
                    int sp1 = sl.indexOf(' ');
                    if (sp1 > 0) {
                        int sp2 = sl.indexOf(' ', sp1 + 1);
                        QByteArray code = (sp2 > 0) ? sl.mid(sp1 + 1, sp2 - sp1 - 1) : sl.mid(sp1 + 1);
                        bool is2xx = code.startsWith("2");
                        if (!is2xx) uploadOk = false;
                    }
                }
                // Non-HTTP response -- assume OK (server-specific protocol)
            }
            // n==0 (orderly close) or n<0 (recv error) 鈫?keep uploadOk=true;
            // the bytes were already sent and timed.
        }
        // selRet<=0 (select timeout) 鈫?keep uploadOk=true; response is optional.
        } // headerComplete -- skip response read if headers weren't fully sent
        int ulMs = static_cast<int>(ulTimer.elapsed());
        closeSocket(sock);

        ulTotalMs += ulMs;
        // 5WHY: previously only checked sent>0, ignoring whether the server
        // actually accepted the upload. Now requires HTTP 2xx response.
        double mbps = (uploadOk && sent > 0 && ulMs > 0) ? (sent * 8.0 / (ulMs / 1000.0) / 1000000.0) : 0;
        if (mbps > 0.01) {
            ulResults.append(mbps);
            out.append(QStringLiteral("  %1  %2  %3")
                .arg(QStringLiteral("%1 KB").arg(sizeKb).rightJustified(10, ' '))
                .arg(QStringLiteral("%1 Mbit/s").arg(mbps, 0, 'f', 2).rightJustified(16, ' '))
                .arg(QStringLiteral("%1 ms").arg(ulMs).rightJustified(8, ' ')));
        } else {
            out.append(QStringLiteral("  %1  %2  %3")
                .arg(QStringLiteral("%1 KB").arg(sizeKb).rightJustified(10, ' '))
                .arg(QStringLiteral("(timeout)").rightJustified(16, ' '))
                .arg(QStringLiteral("-").rightJustified(8, ' ')));
        }
    }

    double ulSpeed = avgTopN(ulResults);

    out.append(QString());
    out.append(QStringLiteral("------------------------------------------------------------------"));
    out.append(QStringLiteral("  Upload: %1 Mbit/s%2")
        .arg(ulSpeed, 0, 'f', 2)
        .arg(ulResults.size() >= 5 ? QStringLiteral("  (avg of top %1)").arg(qMin(5, (int)ulResults.size())) : QString()));

    // === Results ===
    out.append(QString());
    out.append(QString());
    out.append(QStringLiteral("=================================================================="));
    out.append(QStringLiteral("                    Speed Test Results"));
    out.append(QStringLiteral("=================================================================="));
    out.append(QString());
    out.append(QStringLiteral("  Server:      %1 (%2, %3)").arg(best->sponsor, best->name, best->country));
    out.append(QStringLiteral("  Latency:     %1 ms").arg(bestLatency));
    out.append(QStringLiteral("  Download:    %1 Mbit/s").arg(dlSpeed, 0, 'f', 2));
    out.append(QStringLiteral("  Upload:      %1 Mbit/s").arg(ulSpeed, 0, 'f', 2));
    out.append(QStringLiteral("  Duration:    %1 s").arg(totalTimer.elapsed() / 1000.0, 0, 'f', 1));
    out.append(QString());
    out.append(QStringLiteral("=================================================================="));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    if (!hasConnectivity) {
        r.status = DiagStatus::Fail;
        r.summary = QStringLiteral("No internet connectivity");
    } else if (dlSpeed > 0.1 || ulSpeed > 0.1) {
        r.status = DiagStatus::Pass;
        r.summary = QStringLiteral("Connected -- %1 / %2 Mbit/s").arg(dlSpeed, 0, 'f', 1).arg(ulSpeed, 0, 'f', 1);
    } else {
        r.status = DiagStatus::Warning;
        r.summary = QStringLiteral("Connected -- speed test incomplete");
    }
    r.durationMs = totalTimer.elapsed();
    return r;
}

}
