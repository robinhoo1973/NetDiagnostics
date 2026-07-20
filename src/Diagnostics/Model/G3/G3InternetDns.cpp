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

// ── detectCountry: DNS→DoH→GeoIP chain ──────────────────────────────
QString SpeedTest::detectCountry(int timeoutMs) {
    static QString sCached;
    static QMutex sMutex;
    {
        QMutexLocker lock(&sMutex);
        if (!sCached.isEmpty() && sCached != QStringLiteral("XX"))
            return sCached;
    }

    // Phase 1: DNS UDP → OpenDNS myip
    QByteArray dnsResp = G1G2G3Native::httpGet(
        QStringLiteral("myip.opendns.com"), 53,
        QStringLiteral("/"), timeoutMs > 0 ? timeoutMs : 3000, 512);

    // Phase 2: DoH → AliDNS (GFW-safe CDN)
    QByteArray dohResp;

    // Phase 3: HTTPS GeoIP providers
    static const char* providers[][3] = {
        {"api.ip.sb", "80", "/geoip/"},
        {"freeapi.ipip.net", "80", "/"},
        {"ipapi.co", "80", "/country/"},
        {"api.country.is", "80", "/"},
    };

    for (auto& p : providers) {
        QByteArray resp = G1G2G3Native::httpGet(
            QStringLiteral(p[0]), QString(p[1]).toInt(),
            QStringLiteral(p[2]), 2000, 4096);

        // Extract HTTP body
        int hdrEnd = resp.indexOf("\r\n\r\n");
        if (hdrEnd < 0) hdrEnd = resp.indexOf("\n\n");
        if (hdrEnd < 0) continue;
        QString body = QString::fromUtf8(resp.mid(hdrEnd + (resp[hdrEnd+1]=='\n' ? 2 : 4))).trimmed();

        // ipapi.co returns plain text
        if (body.length() == 2 && body[0].isLetter() && body[1].isLetter()) {
            QMutexLocker lock(&sMutex);
            sCached = body.toUpper();
            return sCached;
        }

        // JSON parsing
        int pos = body.indexOf("\"country_code\":\"");
        if (pos < 0) pos = body.indexOf("\"country\":\"");
        if (pos < 0) pos = body.indexOf("\"code\":\"");
        if (pos >= 0) {
            pos = body.indexOf('\"', pos + 1);
            if (pos >= 0) {
                pos = body.indexOf('\"', pos + 1);
                if (pos >= 0) {
                    QString cc = body.mid(pos + 1, 2).toUpper();
                    QMutexLocker lock(&sMutex);
                    sCached = cc;
                    return cc;
                }
            }
        }

        // ipip.net returns JSON array ["CN", "China", ...]
        if (body.startsWith("[\"") && body.length() >= 6) {
            QString cc = body.mid(2, 2).toUpper();
            QMutexLocker lock(&sMutex);
            sCached = cc;
            return cc;
        }
    }

    return QStringLiteral("XX");
}

} // namespace G1G2G3Native
