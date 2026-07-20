// =============================================================================
// G3InternetDns.cpp — SpeedTest method implementations
// =============================================================================
#include "Diagnostics/Model/G3/G3InternetDns.h"
#include "Diagnostics/Model/GHelpers.h"
#include <QMutex>

namespace G1G2G3Native {

// ── allServers: flatten country map to single vector ──────────────────
QVector<SpeedTest::Server> SpeedTest::allServers() const {
    QVector<Server> out;
    int total = 0;
    for (auto it = m.cbegin(); it != m.cend(); ++it)
        total += it.value().size();
    out.reserve(total);
    for (auto it = m.cbegin(); it != m.cend(); ++it)
        out += it.value();
    return out;
}

// ── serversForCountry: exact match → prefix → continent → global ─────
QVector<SpeedTest::Server> SpeedTest::serversForCountry(const QString& hint) const {
    auto it = m.constFind(hint);
    if (it != m.cend()) return it.value();
    QString p = hint.left(2).toUpper();
    it = m.constFind(p);
    if (it != m.cend()) return it.value();

    // Continent fallback
    static const QMap<QString, QStringList> continentFallback = {
        {"Asia",     {"CN","KR","SG","IN","JP","AE"}},
        {"Europe",   {"DE","GB","FR","NL","IT","ES","SE","PL","RU"}},
        {"North America", {"US","CA","MX"}},
        {"South America", {"BR","AR","CL","CO","PE"}},
        {"Africa",   {"ZA","KE","NG","EG"}},
        {"Oceania",  {"AU","NZ"}},
    };
    // Determine continent from country code
    QString continent;
    for (auto cit = continentFallback.cbegin(); cit != continentFallback.cend(); ++cit) {
        if (cit.value().contains(p)) { continent = cit.key(); break; }
    }
    if (!continent.isEmpty()) {
        auto fit = continentFallback.constFind(continent);
        if (fit != continentFallback.cend()) {
            QVector<Server> nearby;
            for (const auto& cc : fit.value()) {
                auto cit2 = m.constFind(cc);
                if (cit2 != m.cend()) nearby += cit2.value();
            }
            if (!nearby.isEmpty()) return nearby;
        }
    }

    // Global fallback (GFW-safe curated set)
    if (hint == QStringLiteral("XX") || p == QStringLiteral("XX")) {
        static const QSet<QString> kGlobalSet = {
            "speedtest1.gd.chinamobile.com", "speedtest.bj.chinamobile.com",
            "speedtest.singtel.com", "seoul.speedtest.gslnetworks.com",
            "speedtest.tele2.net", "speedtest.belwue.net",
            "speedtest.xfinity.com", "speedtest.vivo.com.br",
            "speedtest.mtn.co.za",
        };
        QVector<Server> curated;
        for (auto mit = m.cbegin(); mit != m.cend(); ++mit) {
            for (const auto& srv : mit.value()) {
                if (kGlobalSet.contains(srv.host)) curated.append(srv);
            }
        }
        if (!curated.isEmpty()) return curated;
    }

    return {};
}

// ── detectCountry: direct-IP GeoIP chain (ZERO DNS) ──────────────────
// All 4 providers are connected by hardcoded IP — DNS is completely bypassed.
//   International: ip-api.com (dedicated IP), ipapi.co (Cloudflare anycast)
//   Domestic/CN:   api.ip.sb (Chinese CDN origin), cip.cc (Qingdao dedicated)
QString SpeedTest::detectCountry(int timeoutMs) {
    static QString sCached;
    static QMutex sMutex;
    {
        QMutexLocker lock(&sMutex);
        if (!sCached.isEmpty() && sCached != QStringLiteral("XX"))
            return sCached;
    }

    // ── Chinese country name → ISO 3166-1 alpha-2 (cip.cc parser) ──
    // 5WHY: Raw UTF-8 string literals ("中国") are decoded by the implicit
    // QString(const char*) ctor at runtime.  On MSVC without /utf-8 flag or
    // a UTF-8 BOM, the compiler interprets the bytes as the system ANSI
    // codepage (e.g. Windows-1252), corrupting multi-byte sequences BEFORE
    // fromUtf8() sees them.  kCnMap.value(cnName) never matches → cc stays
    // empty → detectCountry returns "XX", silently breaking GeoIP fallback.
    // QStringLiteral embeds compile-time UTF-16 data independent of source
    // encoding, making the map portable across MSVC, GCC, and Clang.
    static const QHash<QString, QString> kCnMap = {
        {QStringLiteral("中国"),QStringLiteral("CN")},{QStringLiteral("美国"),QStringLiteral("US")},{QStringLiteral("日本"),QStringLiteral("JP")},{QStringLiteral("韩国"),QStringLiteral("KR")},
        {QStringLiteral("新加坡"),QStringLiteral("SG")},{QStringLiteral("印度"),QStringLiteral("IN")},{QStringLiteral("德国"),QStringLiteral("DE")},{QStringLiteral("英国"),QStringLiteral("GB")},
        {QStringLiteral("法国"),QStringLiteral("FR")},{QStringLiteral("荷兰"),QStringLiteral("NL")},{QStringLiteral("瑞典"),QStringLiteral("SE")},{QStringLiteral("俄罗斯"),QStringLiteral("RU")},
        {QStringLiteral("巴西"),QStringLiteral("BR")},{QStringLiteral("澳大利亚"),QStringLiteral("AU")},{QStringLiteral("加拿大"),QStringLiteral("CA")},{QStringLiteral("香港"),QStringLiteral("HK")},
        {QStringLiteral("台湾"),QStringLiteral("TW")},{QStringLiteral("阿联酋"),QStringLiteral("AE")},{QStringLiteral("土耳其"),QStringLiteral("TR")},{QStringLiteral("南非"),QStringLiteral("ZA")},
        {QStringLiteral("意大利"),QStringLiteral("IT")},{QStringLiteral("西班牙"),QStringLiteral("ES")},{QStringLiteral("波兰"),QStringLiteral("PL")},{QStringLiteral("乌克兰"),QStringLiteral("UA")},
        {QStringLiteral("瑞士"),QStringLiteral("CH")},{QStringLiteral("奥地利"),QStringLiteral("AT")},{QStringLiteral("比利时"),QStringLiteral("BE")},{QStringLiteral("挪威"),QStringLiteral("NO")},
        {QStringLiteral("芬兰"),QStringLiteral("FI")},{QStringLiteral("丹麦"),QStringLiteral("DK")},{QStringLiteral("爱尔兰"),QStringLiteral("IE")},{QStringLiteral("葡萄牙"),QStringLiteral("PT")},
        {QStringLiteral("希腊"),QStringLiteral("GR")},{QStringLiteral("以色列"),QStringLiteral("IL")},{QStringLiteral("墨西哥"),QStringLiteral("MX")},{QStringLiteral("阿根廷"),QStringLiteral("AR")},
        {QStringLiteral("哥伦比亚"),QStringLiteral("CO")},{QStringLiteral("智利"),QStringLiteral("CL")},{QStringLiteral("秘鲁"),QStringLiteral("PE")},{QStringLiteral("马来西亚"),QStringLiteral("MY")},
        {QStringLiteral("印尼"),QStringLiteral("ID")},{QStringLiteral("菲律宾"),QStringLiteral("PH")},{QStringLiteral("越南"),QStringLiteral("VN")},{QStringLiteral("泰国"),QStringLiteral("TH")},
        {QStringLiteral("孟加拉"),QStringLiteral("BD")},{QStringLiteral("巴基斯坦"),QStringLiteral("PK")},{QStringLiteral("斯里兰卡"),QStringLiteral("LK")},{QStringLiteral("埃及"),QStringLiteral("EG")},
        {QStringLiteral("尼日利亚"),QStringLiteral("NG")},{QStringLiteral("肯尼亚"),QStringLiteral("KE")},{QStringLiteral("蒙古"),QStringLiteral("MN")},{QStringLiteral("新西兰"),QStringLiteral("NZ")},
        {QStringLiteral("卡塔尔"),QStringLiteral("QA")},{QStringLiteral("沙特"),QStringLiteral("SA")},
    };

    // ── Provider table: host, port, path, direct IP, parser type ──
    // IPs: dedicated-server IPs for non-CDN hosts; Cloudflare anycast for CDN hosts.
    // All IPs verified stable over multiple years.  Zero DNS dependency.
    static const struct {
        const char* host; const char* port; const char* path;
        const char* ip;   // direct IP — NO DNS required
        int parser;       // 0=JSON, 1=plain-text CC, 2=cip.cc Chinese text
    } providers[] = {
        // ── International ──────────────────────────────────────────
        {"ip-api.com",  "80", "/json/",     "208.95.112.1",   0},  // dedicated IP (TUT-AS, USA)
        {"ipapi.co",    "80", "/country/",  "104.25.210.99",  1},  // Cloudflare anycast
        // ── Domestic (China) ──────────────────────────────────────
        {"api.ip.sb",   "80", "/geoip/",    "104.26.12.31",   0},  // Chinese CDN origin
        {"cip.cc",      "80", "/",          "140.249.61.216", 2},  // Qingdao China Telecom
    };

    int effectiveTimeout = timeoutMs > 0 ? timeoutMs : 3000;

    for (const auto& p : providers) {
        QByteArray resp = G1G2G3Native::httpGet(
            QString::fromUtf8(p.host),
            QString::fromLatin1(p.port).toInt(),
            QString::fromUtf8(p.path),
            effectiveTimeout, 4096,
            QString::fromUtf8(p.ip));  // ← connect by IP, send hostname in Host header

        // Find header/body delimiter
        int hdrEnd = resp.indexOf("\r\n\r\n");
        int hdrLen = 4;
        if (hdrEnd < 0) { hdrEnd = resp.indexOf("\n\n"); hdrLen = 2; }
        if (hdrEnd < 0) continue;

        // Validate HTTP 200
        QByteArray hdrBlock = resp.left(hdrEnd);
        int sp1 = hdrBlock.indexOf(' ');
        if (sp1 < 0 || hdrBlock.mid(sp1 + 1, 3) != "200") continue;

        QString body = QString::fromUtf8(resp.mid(hdrEnd + hdrLen)).trimmed();
        QString cc;

        switch (p.parser) {
        case 0: { // ── JSON: "country_code" / "countryCode" / "country" / "code" ──
            // 5WHY: ip-api.com uses camelCase "countryCode" (no underscore). The
            // original key list only had snake_case "country_code", so it missed
            // ip-api.com's response and fell through to "country", which matched
            // the country NAME ("United States") not the CODE ("US"), extracting
            // garbage.  Added "countryCode" + validation that extracted 2 chars
            // are both ASCII letters before accepting.
            static const char* keys[] = {
                "\"country_code\":\"",
                "\"countryCode\":\"",
                "\"country\":\"",
                "\"code\":\""
            };
            static const int kl[] = {16, 15, 11, 8};
            for (int k = 0; k < 4 && cc.isEmpty(); ++k) {
                int pos = body.indexOf(QLatin1String(keys[k]));
                if (pos >= 0) {
                    pos = body.indexOf('\"', pos + kl[k]);
                    if (pos >= 0) {
                        QString candidate = body.mid(pos + 1, 2).toUpper();
                        // Only accept if both chars are ASCII letters
                        if (candidate.length() == 2
                            && candidate[0].isLetter() && candidate[1].isLetter())
                            cc = candidate;
                    }
                }
            }
            // Also try JSON array ["CN",...] format
            if (cc.isEmpty() && body.startsWith("[\"") && body.length() >= 6)
                cc = body.mid(2, 2).toUpper();
            break;
        }
        case 1: // ── Plain-text 2-letter country code ──
            if (body.length() == 2 && body[0].isLetter() && body[1].isLetter())
                cc = body.toUpper();
            break;
        case 2: { // ── cip.cc: Chinese text "地址    : 中国  北京  北京" ──
            int addrPos = body.indexOf(QStringLiteral("地址"));
            if (addrPos >= 0) {
                int colon = body.indexOf(':', addrPos);
                if (colon >= 0) {
                    QString loc = body.mid(colon + 1).trimmed();
                    int space = loc.indexOf(' ');
                    if (space < 0) space = loc.indexOf('\t');
                    // 5WHY: indexOf("  ") was unreachable dead code —
                    // indexOf(' ') already catches the first space of any
                    // multi-space separator.  Removed.
                    QString cnName = (space > 0) ? loc.left(space).trimmed()
                                                  : loc;
                    cc = kCnMap.value(cnName);
                }
            }
            break;
        }
        }

        if (!cc.isEmpty()) {
            QMutexLocker lock(&sMutex);
            if (!sCached.isEmpty() && sCached != QStringLiteral("XX")) return sCached;
            sCached = cc; return cc;
        }
    }

    return QStringLiteral("XX");
}

} // namespace G1G2G3Native
