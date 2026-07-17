#pragma once
#include "Diagnostics/Model/GBase.h"
namespace G1G2G3Native {
// SpeedTest — shared between vpnStatus and speedTest diagnostics.
// 5WHY: Was a thin forward-decl (only detectCountry).  vpnStatus also needs
// Server struct, allServers(), serversForCountry(), and constructor — all
// defined in the FULL class.  Now defined here so both .cpp files see it.
#define S(c, h, p, n, sp) s.host=h; s.port=p; s.name=n; s.sponsor=sp; s.country=c; s.url=QStringLiteral("http://%1:%2").arg(h).arg(p); m[c].append(s);
class SpeedTest {
public:
    struct Server { QString host; int port; QString name, sponsor, country, url; };
    SpeedTest() { build(); }
    QVector<Server> serversForCountry(const QString& hint) const;
    QVector<Server> allServers() const;
    static QString detectCountry(int = 3000);
private: void build(); QMap<QString, QVector<Server>> m;
};
inline void SpeedTest::build() { Server s;
#include "G3ServerDb.inc"
}
#undef S

DiagnosticResult netskopeStatus(DiagId id);
DiagnosticResult dnsServers(DiagId id);
DiagnosticResult dnsCache(DiagId id);
DiagnosticResult dnsPollution(DiagId id);
DiagnosticResult vpnStatus(DiagId id);
DiagnosticResult speedTest(DiagId id);
}
