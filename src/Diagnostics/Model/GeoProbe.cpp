// =============================================================================
// GeoProbe.cpp — Geographic Probe Engine (facade implementation)
// =============================================================================
#include "Diagnostics/Model/GeoProbe.h"
#include "Diagnostics/Model/ProbeScheduler.h"
#include "Diagnostics/Model/ProbeExecutor.h"
#include "Diagnostics/Model/ProbeFeedback.h"
#include "Common/Services/ProbeDatabase.h"
#include <QMap>

GeoProbe::GeoProbe() {
    m_database  = new ProbeDatabase();
    m_scheduler = new ProbeScheduler(m_database);
    m_executor  = new ProbeExecutor(m_database);
    m_feedback  = new ProbeFeedback(m_database, m_scheduler);
    m_executor->start();   // persistent worker thread
}

GeoProbe::~GeoProbe() {
    m_executor->shutdown();
    delete m_feedback;
    delete m_executor;
    delete m_scheduler;
    delete m_database;
}

void GeoProbe::probe(const ProbeConfig& config) {
    m_scheduler->submit(config);
    // Wake the Executor if new Waiting tasks were created
    if (m_database->hasWaitingTasks()) {
        m_executor->notify();
    }
}

ProbeResult GeoProbe::getFeedback(const ProbeConfig& config) {
    return m_feedback->get(config);
}

// ── Region tag mapping (static, shared by Executor/Feedback/Scheduler) ─
QStringList GeoProbe::regionTags(const QString& cc) {
    static const QMap<QString, QStringList> map = {
        // East Asia
        {"CN",{"Asia","Asia/East Asia","Asia/East Asia/Greater China"}},
        {"HK",{"Asia","Asia/East Asia","Asia/East Asia/Greater China"}},
        {"TW",{"Asia","Asia/East Asia","Asia/East Asia/Greater China"}},
        {"MO",{"Asia","Asia/East Asia","Asia/East Asia/Greater China"}},
        {"JP",{"Asia","Asia/East Asia"}},
        {"KR",{"Asia","Asia/East Asia"}},
        {"MN",{"Asia","Asia/East Asia"}},
        {"SG",{"Asia","Asia/Southeast Asia"}},
        {"TH",{"Asia","Asia/Southeast Asia"}},
        {"MY",{"Asia","Asia/Southeast Asia"}},
        {"ID",{"Asia","Asia/Southeast Asia"}},
        {"PH",{"Asia","Asia/Southeast Asia"}},
        {"VN",{"Asia","Asia/Southeast Asia"}},
        {"IN",{"Asia","Asia/South Asia"}},
        {"PK",{"Asia","Asia/South Asia"}},
        {"BD",{"Asia","Asia/South Asia"}},
        {"LK",{"Asia","Asia/South Asia"}},
        {"AE",{"Asia","Asia/Middle East"}},
        {"SA",{"Asia","Asia/Middle East"}},
        {"TR",{"Asia","Asia/Middle East"}},
        {"IL",{"Asia","Asia/Middle East"}},
        {"QA",{"Asia","Asia/Middle East"}},
        {"EG",{"Africa","Africa/North Africa"}},
        {"GB",{"Europe","Europe/Western Europe"}},
        {"DE",{"Europe","Europe/Western Europe"}},
        {"FR",{"Europe","Europe/Western Europe"}},
        {"NL",{"Europe","Europe/Western Europe"}},
        {"IT",{"Europe","Europe/Southern Europe"}},
        {"ES",{"Europe","Europe/Southern Europe"}},
        {"GR",{"Europe","Europe/Southern Europe"}},
        {"SE",{"Europe","Europe/Northern Europe"}},
        {"RU",{"Europe","Europe/Eastern Europe"}},
        {"PL",{"Europe","Europe/Eastern Europe"}},
        {"UA",{"Europe","Europe/Eastern Europe"}},
        {"CH",{"Europe","Europe/Western Europe"}},
        {"AT",{"Europe","Europe/Western Europe"}},
        {"BE",{"Europe","Europe/Western Europe"}},
        {"NO",{"Europe","Europe/Northern Europe"}},
        {"FI",{"Europe","Europe/Northern Europe"}},
        {"DK",{"Europe","Europe/Northern Europe"}},
        {"IE",{"Europe","Europe/Western Europe"}},
        {"PT",{"Europe","Europe/Southern Europe"}},
        {"US",{"North America"}},
        {"CA",{"North America"}},
        {"MX",{"North America"}},
        {"BR",{"South America"}},
        {"AR",{"South America"}},
        {"CO",{"South America"}},
        {"CL",{"South America"}},
        {"PE",{"South America"}},
        {"ZA",{"Africa","Africa/Southern Africa"}},
        {"NG",{"Africa","Africa/West Africa"}},
        {"KE",{"Africa","Africa/East Africa"}},
        {"AU",{"Oceania"}},
        {"NZ",{"Oceania"}},
    };
    return map.value(cc, {cc});
}
