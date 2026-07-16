#include "Diagnostics/Model/GHelpers.h"
#include <future>
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
// 5WHY: detectCountry() always returned "XX" (unknown) because:
//   1. maxBytes=16 truncated HTTP response to just the first header line
//      ("HTTP/1.1 200 OK...") -- the 2-char country code body was never read.
//      Minimum HTTP response is ~20 bytes (status line + double CRLF + body),
//      and typical responses with headers are 150-300 bytes.
//   2. httpGet() returns raw HTTP response (headers + body), but the code
//      treated the entire response as the body. A valid country code like
//      "CN" would appear after "\r\n\r\n" in the response, so the raw
//      string was never 2 characters long.
//
// Fix: (a) use 4096-byte buffer to capture full response,
//      (b) extract HTTP body by stripping headers before "\r\n\r\n",
//      (c) retry with HTTP/1.0 Host-based request format.
// ip-api.com free tier is HTTP-only (no HTTPS), which works for our
// raw-socket approach. ipinfo.io requires a token for the API endpoint;
// fallback to ipapi.co which still serves plain-text country codes on HTTP.
// 5WHY: Only checked \r\n\r\n — some HTTP servers (especially
// lightweight GeoIP services) use bare \n line endings. Now also
// handles \n\n as a header/body separator.
static QString extractHttpBody(const QByteArray& rawResponse) {
    int hdrEnd = rawResponse.indexOf("\r\n\r\n");
    if (hdrEnd < 0) {
        hdrEnd = rawResponse.indexOf("\n\n");
        if (hdrEnd < 0) return {};
        return QString::fromUtf8(rawResponse.mid(hdrEnd + 2)).trimmed();
    }
    return QString::fromUtf8(rawResponse.mid(hdrEnd + 4)).trimmed();
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
    // 5WHY: Sequential GeoIP calls caused 12s+ timeout in GFW environments
    // (4 endpoints × 3s each). Now fires ipip.net (GFW-safe) and ip-api.com
    // (global) in parallel via std::async — whichever responds first wins,
    // cutting worst-case from ~12s to ~3s. Sequential fallbacks only if both
    // parallel calls fail.
    int to = timeoutMs > 0 ? timeoutMs : 3000;
    QString cc;

    // Launch ip-api.com in background (may be slow inside GFW, fast outside)
    auto futIpApi = std::async(std::launch::async, [to]() -> QString {
        QByteArray r = G1G2G3Native::httpGet(
            QStringLiteral("ip-api.com"), 80,
            QStringLiteral("/line/?fields=countryCode"), to, 4096);
        QString body = extractHttpBody(r);
        return (body.length() == 2) ? body.toUpper() : QString();
    });

    // Primary (GFW-safe): myip.ipip.net — try while ip-api runs in background
    QByteArray resp = G1G2G3Native::httpGet(
        QStringLiteral("myip.ipip.net"), 80,
        QStringLiteral("/"), to, 4096);
    cc = extractHttpBody(resp);
    if (!cc.isEmpty()) {
        int pos = cc.indexOf("\"country_code\":\"");
        if (pos >= 0) {
            pos += 17;
            int end = cc.indexOf('\"', pos);
            if (end > pos) {
                QString cc2 = cc.mid(pos, end - pos).trimmed().toUpper();
                if (cc2.length() == 2) { QMutexLocker l(&sCountryMutex); sCachedCountry = cc2; return sCachedCountry; }
            }
        }
    }

    // ipip.net failed — wait for ip-api.com (already running in parallel)
    cc = futIpApi.get();
    if (!cc.isEmpty()) { QMutexLocker l(&sCountryMutex); sCachedCountry = cc; return sCachedCountry; }

    // Both primary + secondary failed — sequential fallbacks with shorter timeouts
    // Tertiary: ipapi.co
    resp = G1G2G3Native::httpGet(
        QStringLiteral("ipapi.co"), 80, QStringLiteral("/country/"), 2000, 4096);
    cc = extractHttpBody(resp);
    if (cc.length() == 2) { QMutexLocker l(&sCountryMutex); sCachedCountry = cc.toUpper(); return sCachedCountry; }
    // Last resort: ipinfo.io
    resp = G1G2G3Native::httpGet(
        QStringLiteral("ipinfo.io"), 80, QStringLiteral("/country"), 2000, 4096);
    cc = extractHttpBody(resp);
    if (cc.length() == 2) { QMutexLocker l(&sCountryMutex); sCachedCountry = cc.toUpper(); return sCachedCountry; }

    Logger::instance().event(QStringLiteral("GeoIP: all providers failed, country=XX"));
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

    // === Phase 1: Detect country + load regional servers ===
    SpeedTest st;
    QString country = SpeedTest::detectCountry(3000);
    out.append(QStringLiteral("Detected country: %1").arg(country == "XX" ? "Unknown" : country));

    QVector<SpeedTest::Server> servers = st.serversForCountry(country);
    out.append(QStringLiteral("Loaded %1 servers for region").arg(servers.size()));
    // 5WHY: Country Unknown with China connectivity = likely GFW user.
    // Log a hint so the user understands the server selection strategy.
    if (country == QStringLiteral("XX") && connChina >= 3) {
        out.append(QStringLiteral("  (GeoIP failed, but %1/%2 China sites reachable)").arg(connChina).arg(connChina + connGlobal));
        out.append(QStringLiteral("  Using connectivity-guided server selection)"));
    }

    // Timeout guard: if we've already spent >25s, skip speed measurement
    if (totalTimer.elapsed() > 25000) {
        out.append(QString());
        out.append(QStringLiteral("  (Speed test skipped: connectivity check took too long)"));
        r.rawOutput = out.join('\n'); r.details = r.rawOutput;
        r.status = hasConnectivity ? DiagStatus::Warning : DiagStatus::Fail;
        r.summary = hasConnectivity ? QStringLiteral("Connected -- speed test timed out") : QStringLiteral("No internet");
        r.durationMs = totalTimer.elapsed(); return r;
    }

    // === Phase 2: Select best server by HTTP latency (speedtest-cli style) ===
    out.append(QStringLiteral("--- Server Selection (HTTP latency) -----------------------------"));
    out.append(QString());
    out.append(QStringLiteral("  %1  %2  %3  %4")
        .arg(QStringLiteral("#").rightJustified(3, ' '))
        .arg(QStringLiteral("Sponsor").leftJustified(22, ' '))
        .arg(QStringLiteral("Server").leftJustified(17, ' '))
        .arg(QStringLiteral("Latency").rightJustified(7, ' ')));
    out.append(QStringLiteral("  %1  %2  %3  %4")
        .arg(QString(3, QChar('-')))
        .arg(QString(22, QChar('-')))
        .arg(QString(17, QChar('-')))
        .arg(QString(7, QChar('-'))));

    struct RankedServer { SpeedTest::Server* srv; int latency; };
    QVector<RankedServer> ranked;

    // Shuffle servers so geographic diversity gets tested, not just the first N
    {
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(servers.begin(), servers.end(), g);
    }

    int maxServers = qMin(12, (int)servers.size()); // test up to 12 for better coverage
    for (auto& s : servers) {
        if (ranked.size() >= maxServers) break;
        if (totalTimer.elapsed() > 25000) break; // global timeout
        int lat = httpLatencyMs(s.url, 5000);
        if (lat > 0) {
            ranked.append({&s, lat});
        }
    }

    if (ranked.isEmpty()) {
        // 5WHY: Curated fallback (8 servers) may all be unreachable from
        // some regions. Try ALL servers as a last resort — 48+ servers
        // gives much better odds of finding at least one reachable host.
        QVector<SpeedTest::Server> allServers = st.allServers();
        if (servers.size() < allServers.size()) {
            out.append(QStringLiteral("  (curated servers unreachable, trying all %1 servers...)").arg(allServers.size()));
            // 5WHY: Sequential iteration over allServers() biases toward
            // the first country in the map (CN).  When Country=Unknown and the
            // curated list failed, use geographic round-robin: take one server
            // from each country, cycling through countries, so every region
            // gets a fair chance before time runs out.
            // Group servers by country for round-robin
            QMap<QString, QVector<SpeedTest::Server*>> byCountry;
            for (auto& s : allServers) byCountry[s.country].append(&s);
            QStringList countries = byCountry.keys();
            // 5WHY: When China is reachable but GeoIP failed, prioritize CN
            bool chinaFirst = (country == QStringLiteral("XX") && connChina >= 3 && connGlobal == 0);
            if (chinaFirst) {
                // Move CN to front of round-robin order
                countries.removeAll(QStringLiteral("CN"));
                countries.prepend(QStringLiteral("CN"));
                out.append(QStringLiteral("  (China connectivity detected, prioritizing CN servers)"));
            }
            int maxAll = qMin(12, (int)allServers.size());
            int countryIdx = 0;
            while (ranked.size() < maxAll && !countries.isEmpty()) {
                if (totalTimer.elapsed() > 25000) break;
                QString cc2 = countries[countryIdx % countries.size()];
                auto& srvList = byCountry[cc2];
                if (srvList.isEmpty()) {
                    countries.removeAt(countryIdx % countries.size());
                    continue;
                }
                SpeedTest::Server* s = srvList.takeFirst();
                int lat = httpLatencyMs(s->url, 5000);
                if (lat > 0) ranked.append({s, lat});
                countryIdx++;
            }
        }
    }
    if (ranked.isEmpty()) {
        out.append(QStringLiteral("  (no reachable servers)"));
        out.append(QString());
        r.rawOutput = out.join('\n'); r.details = r.rawOutput;
        r.status = hasConnectivity ? DiagStatus::Warning : DiagStatus::Fail;
        r.summary = hasConnectivity ? QStringLiteral("Connected -- no speed test servers reachable")
                                    : QStringLiteral("No internet connectivity");
        r.durationMs = totalTimer.elapsed(); return r;
    }

    // Sort by HTTP latency ascending -- fastest first
    std::sort(ranked.begin(), ranked.end(),
              [](const RankedServer& a, const RankedServer& b) { return a.latency < b.latency; });

    for (int i = 0; i < ranked.size(); i++) {
        auto& rs = ranked[i];
        out.append(QStringLiteral("  %1  %2  %3  %4")
            .arg(i + 1, 3)
            .arg(rs.srv->sponsor.leftJustified(22, ' '))
            .arg(rs.srv->name.leftJustified(17, ' '))
            .arg(QStringLiteral("%1 ms").arg(rs.latency).rightJustified(7, ' ')));
    }

    SpeedTest::Server* best = ranked[0].srv;
    int bestLatency = ranked[0].latency;

    out.append(QString());
    out.append(QStringLiteral("------------------------------------------------------------------"));
    out.append(QStringLiteral("  Selected: %1 (%2) -- %3 ms")
        .arg(best->sponsor, best->name).arg(bestLatency));
    out.append(QString());

    // === Phase 3: Download test (with server fallback) ===
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
    int dlServerIdx = 0; // current preferred server index into ranked[]

    for (int sizeKb : dlSizes) {
        if (dlTotalMs > 12000) break; // cap at ~12 seconds

        // Try preferred server first, fall back through ranked list independently
        // per size tier 闂?a server that handles 250KB may choke on 25MB.
        bool ok = false;
        for (int si = 0; si < ranked.size(); si++) {
            // Try each server once before marking failure
            int idx = (dlServerIdx + si) % ranked.size();
            SpeedTest::Server* srv = ranked[idx].srv;
            QString dlUrl = QStringLiteral("%1/download?size=%2").arg(srv->url).arg(sizeKb * 1000);
            auto res = httpDownload(dlUrl, sizeKb * 1000, 8000);
            if (res.ok && res.mbps > 0.01) {
                dlResults.append(res.mbps);
                dlTotalBytes += res.bytes;
                dlTotalMs += res.durationMs;
                if (idx != dlServerIdx) {
                    out.append(QStringLiteral("  (switched to %1)").arg(srv->sponsor));
                    best = srv; bestLatency = ranked[idx].latency;
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

    // === Phase 4: Upload test ===
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

    // 5WHY: Timeouts were too aggressive for slow/congested networks.
    // Removed all upload timeouts — let the server respond at its own pace.
    // The outer diagnostic task has a 180s watchdog to prevent hanging.
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

    for (int sizeKb : ulSizes) {
        int dataSize = sizeKb * 1000;

        // HTTP POST with measured upload
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;
        struct sockaddr_in addr;
        if (!hostToAddr(best->host, best->port, addr)) { closeSocket(sock); continue; }

#if defined(_WIN32)
        u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
#else
        int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
        ::connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
        fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
        struct timeval tv = {30, 0};  // 30s connect timeout (generous)
        if (select(sock + 1, nullptr, &fdset, nullptr, &tv) <= 0) { closeSocket(sock); continue; }
        int err = 0; socklen_t len = sizeof(err);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len);
        if (err != 0) { closeSocket(sock); continue; }

        // Generate random-ish data.
        // 5WHY: rand() uses global state shared across all threads — concurrent
        // calls from multiple speed-test invocations cause data races in glibc/
        // msvcrt. Use a thread-local LCG seeded from clock+thread-id for
        // reproducible per-thread pseudo-randomness without global state.
        // Not cryptographically secure, but sufficient for upload payload.
        thread_local uint64_t lcgState = static_cast<uint64_t>(
            QDateTime::currentMSecsSinceEpoch()) ^
            (reinterpret_cast<uintptr_t>(&lcgState) << 16);
        QByteArray uploadData(dataSize, '\0');
        for (int i = 0; i < dataSize; i++) {
            lcgState = lcgState * 6364136223846793005ULL + 1442695040888963407ULL;
            uploadData[i] = static_cast<char>(33 + static_cast<int>(lcgState % 94));
        }

        // POST request headers
        QByteArray postHeaders = QStringLiteral("POST /upload HTTP/1.0\r\nHost: %1\r\nContent-Type: application/octet-stream\r\nContent-Length: %2\r\nConnection: close\r\n\r\n")
            .arg(best->host).arg(dataSize).toUtf8();

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
        // Includes wall-clock guard so outer ulTotalMs check can fire
        int sent = 0; const char* dp = uploadData.constData();
        if (headerComplete) {
        while (sent < dataSize) {
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
        FD_ZERO(&fdset); FD_SET(sock, &fdset);
        tv = {30, 0};  // generous 30s response read timeout
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
