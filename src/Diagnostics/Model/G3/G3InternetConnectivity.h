#pragma once
#include "Diagnostics/Model/GBase.h"
namespace G1G2G3Native {

// ── SpeedTest server database ───────────────────────────────────────
// Shared by: GeoProbe (ProbeScheduler/ProbeExecutor), internetConnectivity.
// Hardcoded global speed-test server list organized by country code.
#define ADD_SERVER(c, h, p, n, sp) s.host=h; s.port=p; s.name=n; s.sponsor=sp; s.country=c; s.url=QStringLiteral("http://%1:%2").arg(h).arg(p); m[c].append(s);
class SpeedTest {
public:
    struct Server { QString host; int port; QString name, sponsor, country, url; };
    SpeedTest() { build(); }
    QVector<Server> serversForCountry(const QString& hint) const;
    QVector<Server> allServers() const;
private: void build(); QMap<QString, QVector<Server>> m;
};
inline void SpeedTest::build() {
    static QMap<QString, QVector<Server>> sDb = []() {
        QMap<QString, QVector<Server>> m;
        Server s;
        #include "G3ServerDb.inc"
        return m;
    }();
    m = sDb;
}
#undef ADD_SERVER

DiagnosticResult internetConnectivity(DiagId id);

} // namespace G1G2G3Native
