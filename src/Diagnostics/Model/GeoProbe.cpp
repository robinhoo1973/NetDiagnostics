// =============================================================================
// GeoProbe.cpp — Geographic Probe Engine (facade implementation)
// =============================================================================
#include "Diagnostics/Model/GeoProbe.h"
#include "Diagnostics/Model/ProbeScheduler.h"
#include "Diagnostics/Model/ProbeExecutor.h"
#include "Diagnostics/Model/ProbeFeedback.h"
#include "Common/Services/ProbeDatabase.h"
#include <QMap>
#include <QFile>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QTextStream>

// ── Server DB macro (same format as G3ServerDb.inc) ─────────────────
#define ADD_SERVER(c, h, p, n, sp) \
    s.host=h; s.port=p; s.name=n; s.sponsor=sp; s.country=c; \
    s.url=QStringLiteral("http://%1:%2").arg(h).arg(p); m[c].append(s);

// ── Lazy-loaded server database ─────────────────────────────────────
static QMap<QString, QVector<ProbeServer>> sServerDb;
static bool sServerDbLoaded = false;

void GeoProbe::ensureServerDbLoaded() {
    if (sServerDbLoaded) return;

    // Step 1: try runtime-updated DB (from ServerDbUpdater downloads)
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                 + QStringLiteral("/G3ServerDb.inc");
    if (QFile::exists(path)) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QRegularExpression re(QLatin1String(
                R"re(ADD_SERVER\("([^"]+)","([^"]+)",(\d+),"([^"]*)","([^"]*)"\);)re"));
            QTextStream ts(&f);
            while (!ts.atEnd()) {
                auto m = re.match(ts.readLine().trimmed());
                if (m.hasMatch()) {
                    ProbeServer s;
                    s.country = m.captured(1);
                    s.host    = m.captured(2);
                    s.port    = m.captured(3).toInt();
                    s.name    = m.captured(4);
                    s.sponsor = m.captured(5);
                    s.url = QStringLiteral("http://%1:%2").arg(s.host).arg(s.port);
                    sServerDb[s.country].append(s);
                }
            }
        }
    }

    // Step 2: fallback — compiled-in server DB
    if (sServerDb.isEmpty()) {
        QMap<QString, QVector<ProbeServer>> m;
        ProbeServer s;
        #include "Diagnostics/Model/G3/G3ServerDb.inc"
        sServerDb = m;
    }

    sServerDbLoaded = true;
}

QVector<ProbeServer> GeoProbe::allServers() {
    ensureServerDbLoaded();
    QVector<ProbeServer> out;
    int total = 0;
    for (auto it = sServerDb.cbegin(); it != sServerDb.cend(); ++it)
        total += it.value().size();
    out.reserve(total);
    for (auto it = sServerDb.cbegin(); it != sServerDb.cend(); ++it)
        out += it.value();
    return out;
}

QVector<ProbeServer> GeoProbe::serversForCountry(const QString& hint) {
    ensureServerDbLoaded();
    auto it = sServerDb.constFind(hint);
    if (it != sServerDb.cend()) return it.value();
    QString p = hint.left(2).toUpper();
    it = sServerDb.constFind(p);
    if (it != sServerDb.cend()) return it.value();

    // Continent fallback
    static const QMap<QString, QStringList> continentFallback = {
        {"Asia",     {"CN","KR","SG","IN","JP","AE"}},
        {"Europe",   {"DE","GB","FR","NL","IT","ES","SE","PL","RU"}},
        {"North America", {"US","CA","MX"}},
        {"South America", {"BR","AR","CL","CO","PE"}},
        {"Africa",   {"ZA","KE","NG","EG"}},
        {"Oceania",  {"AU","NZ"}},
    };
    QString continent;
    for (auto cit = continentFallback.cbegin(); cit != continentFallback.cend(); ++cit) {
        if (cit.value().contains(p)) { continent = cit.key(); break; }
    }
    if (!continent.isEmpty()) {
        auto fit = continentFallback.constFind(continent);
        if (fit != continentFallback.cend()) {
            QVector<ProbeServer> nearby;
            for (const auto& cc : fit.value()) {
                auto cit2 = sServerDb.constFind(cc);
                if (cit2 != sServerDb.cend()) nearby += cit2.value();
            }
            if (!nearby.isEmpty()) return nearby;
        }
    }
    return {};
}

#undef ADD_SERVER

GeoProbe& GeoProbe::instance() {
    static GeoProbe inst;
    return inst;
}

GeoProbe::GeoProbe() {
    m_database  = new ProbeDatabase();
    m_executor  = new ProbeExecutor(m_database);
    m_scheduler = new ProbeScheduler(m_database, m_executor);
    m_feedback  = new ProbeFeedback(m_database, m_scheduler);
}

GeoProbe::~GeoProbe() {
    m_executor->requestStop();
    delete m_feedback;
    delete m_scheduler;
    delete m_executor;
    delete m_database;
}

void GeoProbe::probe(const ProbeConfig& config) {
    m_scheduler->submit(config);
}

ProbeResult GeoProbe::getFeedback(const ProbeConfig& config) {
    return m_feedback->get(config);
}

void GeoProbe::clear() {
    m_database->clear();
}

// ── Region tag mapping (static, shared by Executor/Feedback/Scheduler) ─
QStringList GeoProbe::regionTags(const QString& cc) {
    static const QMap<QString, QStringList> map = {
        {"CN",{"Asia","Asia/East Asia","Asia/East Asia/Greater China"}},
        {"HK",{"Asia","Asia/East Asia","Asia/East Asia/Greater China"}},
        {"TW",{"Asia","Asia/East Asia","Asia/East Asia/Greater China"}},
        {"MO",{"Asia","Asia/East Asia","Asia/East Asia/Greater China"}},
        {"JP",{"Asia","Asia/East Asia"}},{"KR",{"Asia","Asia/East Asia"}},{"MN",{"Asia","Asia/East Asia"}},
        {"SG",{"Asia","Asia/Southeast Asia"}},{"TH",{"Asia","Asia/Southeast Asia"}},
        {"MY",{"Asia","Asia/Southeast Asia"}},{"ID",{"Asia","Asia/Southeast Asia"}},
        {"PH",{"Asia","Asia/Southeast Asia"}},{"VN",{"Asia","Asia/Southeast Asia"}},
        {"IN",{"Asia","Asia/South Asia"}},{"PK",{"Asia","Asia/South Asia"}},
        {"BD",{"Asia","Asia/South Asia"}},{"LK",{"Asia","Asia/South Asia"}},
        {"AE",{"Asia","Asia/Middle East"}},{"SA",{"Asia","Asia/Middle East"}},
        {"TR",{"Asia","Asia/Middle East"}},{"IL",{"Asia","Asia/Middle East"}},{"QA",{"Asia","Asia/Middle East"}},
        {"EG",{"Africa","Africa/North Africa"}},
        {"GB",{"Europe","Europe/Western Europe"}},{"DE",{"Europe","Europe/Western Europe"}},
        {"FR",{"Europe","Europe/Western Europe"}},{"NL",{"Europe","Europe/Western Europe"}},
        {"IT",{"Europe","Europe/Southern Europe"}},{"ES",{"Europe","Europe/Southern Europe"}},
        {"GR",{"Europe","Europe/Southern Europe"}},{"SE",{"Europe","Europe/Northern Europe"}},
        {"RU",{"Europe","Europe/Eastern Europe"}},{"PL",{"Europe","Europe/Eastern Europe"}},
        {"UA",{"Europe","Europe/Eastern Europe"}},{"CH",{"Europe","Europe/Western Europe"}},
        {"AT",{"Europe","Europe/Western Europe"}},{"BE",{"Europe","Europe/Western Europe"}},
        {"NO",{"Europe","Europe/Northern Europe"}},{"FI",{"Europe","Europe/Northern Europe"}},
        {"DK",{"Europe","Europe/Northern Europe"}},{"IE",{"Europe","Europe/Western Europe"}},
        {"PT",{"Europe","Europe/Southern Europe"}},
        {"US",{"North America"}},{"CA",{"North America"}},{"MX",{"North America"}},
        {"BR",{"South America"}},{"AR",{"South America"}},{"CO",{"South America"}},
        {"CL",{"South America"}},{"PE",{"South America"}},
        {"ZA",{"Africa","Africa/Southern Africa"}},{"NG",{"Africa","Africa/West Africa"}},
        {"KE",{"Africa","Africa/East Africa"}},
        {"AU",{"Oceania"}},{"NZ",{"Oceania"}},
    };
    return map.value(cc, {cc});
}
