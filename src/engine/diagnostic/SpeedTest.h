// ── SpeedTest — built-in server registry & selection ────────────────
// Provides a curated list of public speed-test servers from major ISPs
// and cloud providers.  Servers are selected by geographic proximity
// using country-code hints.  No external API calls — all server data
// is compiled in.
//
// Extracted from G1G2G3Native.cpp to reduce file size and improve
// maintainability.  All methods are inline (header-only).
// ─────────────────────────────────────────────────────────────────────
#pragma once
#include <QString>
#include <QVector>
#include <QMap>
#include <QPair>
#include <algorithm>

// NOTE: This header is #included INSIDE namespace G1G2G3Native { } in
// G1G2G3Native.cpp.  Do NOT open the namespace here — it's already open.
int tcpPingMs(const QString& host, int port); // forward (defined in G1G2G3Native.cpp)

class SpeedTest {
public:
    struct Server { QString host; int port; QString name, sponsor, country, url; };
    SpeedTest();
    QVector<Server> serversForCountry(const QString& hint) const;
    QVector<Server> allServers() const;
    static QString detectCountry(int = 3000);
    static void rankByLatency(QVector<Server>& c, int tmo = 3000);
    static Server selectBest(QVector<Server>& c, int maxMs = 500, int tmo = 3000);
private:
    void build();
    QMap<QString, QVector<Server>> m;
};

inline SpeedTest::SpeedTest() { build(); }

#define S(c, h, p, n, sp) \
    s.host=h; s.port=p; s.name=n; s.sponsor=sp; s.country=c; \
    s.url=QStringLiteral("http://%1:%2").arg(h).arg(p); m[c].append(s);
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
    // Alibaba Cloud / Tencent Cloud speed test endpoints (CN region only)
    S("CN","speedtest-bj.oss-cn-beijing.aliyuncs.com",80,"Beijing","Alibaba Cloud");
    S("CN","speedtest-sh.oss-cn-shanghai.aliyuncs.com",80,"Shanghai","Alibaba Cloud");
    S("CN","speedtest-gz.oss-cn-guangzhou.aliyuncs.com",80,"Guangzhou","Alibaba Cloud");
    S("CN","speedtest-bj-ct.oss-cn-beijing.aliyuncs.com",80,"Beijing CT","Alibaba Cloud");
}
#undef S

inline QVector<SpeedTest::Server> SpeedTest::serversForCountry(const QString& hint) const {
    if (m.contains(hint)) return m[hint];
    QString p = hint.left(2).toUpper();
    return m.contains(p) ? m[p] : allServers();
}

inline QVector<SpeedTest::Server> SpeedTest::allServers() const {
    QVector<Server> a; for (auto& l : m) a.append(l); return a;
}

inline QString SpeedTest::detectCountry(int) { return QStringLiteral("CN"); }

inline void SpeedTest::rankByLatency(QVector<Server>& c, int tmo) {
    QVector<QPair<int,Server>> r;
    for (auto& s : c) { int ms = tcpPingMs(s.host, s.port); r.append({ms>=0?ms:999999,s}); }
    std::sort(r.begin(), r.end(), [](auto& a, auto& b){return a.first<b.first;});
    c.clear(); for (auto& p : r) c.append(p.second);
}

inline SpeedTest::Server SpeedTest::selectBest(QVector<Server>& c, int maxMs, int tmo) {
    (void)tmo; rankByLatency(c, tmo);
    for (auto& s : c) { int ms = tcpPingMs(s.host, s.port); if (ms >= 0 && ms < maxMs) return s; }
    return c.first();
}

} // namespace G1G2G3Native
