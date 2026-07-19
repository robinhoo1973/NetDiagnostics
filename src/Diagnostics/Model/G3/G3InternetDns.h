#pragma once
#include "Diagnostics/Model/GBase.h"
namespace G1G2G3Native {
// SpeedTest — shared between geoIPLoc, internetConnectivity, and GeoProbe.
// Class declaration here (shared header), build() + server DB stay in G3.
#define ADD_SERVER(c, h, p, n, sp) s.host=h; s.port=p; s.name=n; s.sponsor=sp; s.country=c; s.url=QStringLiteral("http://%1:%2").arg(h).arg(p); m[c].append(s);
class SpeedTest {
public:
    struct Server { QString host; int port; QString name, sponsor, country, url; };
    SpeedTest() { build(); }
    QVector<Server> serversForCountry(const QString& hint) const;
    QVector<Server> allServers() const;
    static QString detectCountry(int = 3000);
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

DiagnosticResult netskopeStatus(DiagId id);
DiagnosticResult dnsServers(DiagId id);
DiagnosticResult dnsCache(DiagId id);
DiagnosticResult dnsPollution(DiagId id);
DiagnosticResult geoIPLoc(DiagId id);
DiagnosticResult internetConnectivity(DiagId id);
}
