// =============================================================================
// G3InternetDns.cpp — SpeedTest method implementations
// =============================================================================
#include "Diagnostics/Model/G3/G3InternetDns.h"

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

} // namespace G1G2G3Native
