// =============================================================================
// G1G2G3Native.cpp 闁?Pure C++ G1/G2/G3 diagnostics 闁?ZERO shell commands
// Linux: getifaddrs, /proc/net, /sys/class/net, ioctl, netlink, socket APIs
// Windows: GetAdaptersAddresses, GetExtendedTcpTable, GetIpForwardTable2, etc.
// Output format: matches Windows CLI tools (ipconfig, route print, arp -a,
// netstat -an, netsh, nslookup)
// =============================================================================
#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif
#include "engine/diagnostic/G1G2G3Native.h"
#include "util/DebugSwitch.h"
#include <QMutex>
#include <QElapsedTimer>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QRegularExpression>
#include <QVariantMap>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <atomic>
#include <chrono>
#include <thread>
#include <climits>
#include <csignal>
#include "util/DnsResolver.h"
#include "util/DiagnosticFormatter.h"
#include "util/NetUtil.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <wlanapi.h>
#include <netioapi.h>
#include <winhttp.h>
#include <tlhelp32.h>
#define close closesocket
#elif defined(__APPLE__)
// macOS / iOS 闁?use AF_LINK+sockaddr_dl, no /proc, no /sys, no linux/wireless.h
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <resolv.h>
#include <unistd.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>
#ifdef __linux__
#include <net/if_arp.h>
#endif
#include <net/if_types.h>
#ifdef PLATFORM_IOS
#include "engine/IosWiFiHelper.h"
#include "engine/task/IosNetworkInfo.h"
#endif
#else
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <linux/wireless.h>
#include <linux/sockios.h>
#include <linux/if_packet.h>
#endif

namespace G1G2G3Native {

static int tcpPingMs(const QString& host, int port); // forward

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// SpeedTest 闁?built-in server registry + selection
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
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


// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁?
// Helpers (continued)
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁?

static QString macToStr(const unsigned char* mac) {
    return QStringLiteral("%1-%2-%3-%4-%5-%6")
        .arg(mac[0], 2, 16, QLatin1Char('0'))
        .arg(mac[1], 2, 16, QLatin1Char('0'))
        .arg(mac[2], 2, 16, QLatin1Char('0'))
        .arg(mac[3], 2, 16, QLatin1Char('0'))
        .arg(mac[4], 2, 16, QLatin1Char('0'))
        .arg(mac[5], 2, 16, QLatin1Char('0'));
}

// Thread-safe IPv4 formatting (inet_ntoa uses a shared static buffer and is
// unsafe in the concurrent diagnostic thread pool).
static QString ip4ToStr(struct in_addr a) {
    char buf[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &a, buf, sizeof(buf));
    return QString::fromLatin1(buf);
}

static QString ipToStr(uint32_t ip) {
    struct in_addr a; a.s_addr = ip; // ip is already network byte order from /proc/net/*
    return ip4ToStr(a);
}

static bool hasNonEmptyValue(const QVariantMap& values, const char* key) {
    const auto it = values.constFind(QLatin1String(key));
    return it != values.cend() && !it->toString().trimmed().isEmpty();
}

static bool hasCellularIdentity(const QVariantMap& cell) {
    return hasNonEmptyValue(cell, "carrierName")
        || hasNonEmptyValue(cell, "radioAccess")
        || (hasNonEmptyValue(cell, "mcc") && hasNonEmptyValue(cell, "mnc"));
}

static QString cellularSummary(const QVariantMap& cell) {
    const QString carrier = cell.value(QStringLiteral("carrierName")).toString().trimmed();
    const QString radio = cell.value(QStringLiteral("radioAccess")).toString().trimmed();
    if (!carrier.isEmpty() && !radio.isEmpty())
        return QStringLiteral("Carrier: %1 (%2)").arg(carrier, radio);
    if (!carrier.isEmpty())
        return QStringLiteral("Carrier: %1").arg(carrier);
    if (!radio.isEmpty())
        return QStringLiteral("Radio: %1").arg(radio);
    return QStringLiteral("Cellular service detected");
}

static const char* tcpStateName(int st) {
    switch(st){case 1:return"ESTABLISHED";case 2:return"SYN_SENT";case 3:return"SYN_RECV";
    case 4:return"FIN_WAIT1";case 5:return"FIN_WAIT2";case 6:return"TIME_WAIT";
    case 7:return"CLOSE";case 8:return"CLOSE_WAIT";case 9:return"LAST_ACK";
    case 10:return"LISTEN";case 11:return"CLOSING";default:return"UNKNOWN";}
}

#ifndef _WIN32
// Parse /proc/net/tcp (or tcp6/udp/udp6) 闁?hex format:
//   sl local_address rem_address st tx_queue rx_queue tr tm->when retrnsmt uid timeout inode
// Example: 0: 00000000:0016 00000000:0000 0A 00000000:00000000 00:00000000 00000000     0        0 12345
struct ProcNetConn {
    QString localIp; int localPort;
    QString remoteIp; int remotePort;
    int state; // TCP state (0A=LISTEN, 01=ESTABLISHED etc.)
    uint32_t uid;
    bool isIPv6;
};
#endif

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// G1 闁?Network Adapters (ipconfig /all format)
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
DiagnosticResult networkAdapters(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

    out.append(QString());
    out.append(QStringLiteral("Network Adapters (ifconfig -s style)"));
    out.append(QString());

#ifdef _WIN32
    ULONG bufLen = 15000;
    QByteArray buf(bufLen, '\0');
    PIP_ADAPTER_ADDRESSES adapters = (PIP_ADAPTER_ADDRESSES)buf.data();
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_ANYCAST;
    if (GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, adapters, &bufLen) != NO_ERROR) {
        buf.resize(bufLen);
        adapters = (PIP_ADAPTER_ADDRESSES)buf.data();
        if (GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, adapters, &bufLen) != NO_ERROR) {
            r.rawOutput = QStringLiteral("No adapters found");
            r.details = r.rawOutput; r.status = DiagStatus::Warning;
            r.summary = QStringLiteral("Failed to enumerate adapters");
            r.durationMs = t.elapsed(); return r;
        }
    }
    for (auto* a = adapters; a; a = a->Next) {
        QString ifType = (a->IfType == IF_TYPE_ETHERNET_CSMACD) ? QStringLiteral("Ethernet")
            : (a->IfType == IF_TYPE_IEEE80211) ? QStringLiteral("Wireless")
            : QStringLiteral("Other");
        out.append(QStringLiteral("%1 adapter %2:").arg(ifType, QString::fromWCharArray(a->FriendlyName)));
        out.append(QString());
        if (a->PhysicalAddressLength > 0)
            out.append(QStringLiteral("   Physical Address. . . . . . . . . : %1").arg(macToStr(a->PhysicalAddress)));
        for (auto* ua = a->FirstUnicastAddress; ua; ua = ua->Next) {
            char ip[64]; DWORD ipLen = sizeof(ip);
            WSAAddressToStringA(ua->Address.lpSockaddr, ua->Address.iSockaddrLength, nullptr, ip, &ipLen);
            out.append(QStringLiteral("   IPv4 Address. . . . . . . . . . . : %1").arg(QString::fromLatin1(ip)));
        }
        for (auto* gw = a->FirstGatewayAddress; gw; gw = gw->Next) {
            char ip[64]; DWORD ipLen = sizeof(ip);
            WSAAddressToStringA(gw->Address.lpSockaddr, gw->Address.iSockaddrLength, nullptr, ip, &ipLen);
            out.append(QStringLiteral("   Default Gateway . . . . . . . . . : %1").arg(QString::fromLatin1(ip)));
        }
        out.append(QString());
    }
#elif defined(PLATFORM_IOS)
    // 闁冲厜鍋撻柍鍏夊亾 iOS: use getifaddrs + SystemConfiguration framework 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋?
    // iOS restricts: MAC addresses, /sys/class/net, /proc, netlink, ARP, routing table
    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) != 0) {
        r.rawOutput = QStringLiteral("No adapters found");
        r.details = r.rawOutput; r.status = DiagStatus::Warning;
        r.summary = QStringLiteral("Failed to enumerate adapters");
        r.durationMs = t.elapsed(); return r;
    }

    // Collect per-interface info
    struct IfInfo { QString name; QStringList ips; int flags; };
    QMap<QString, IfInfo> ifMap;
    for (auto* p = ifa; p; p = p->ifa_next) {
        if (!p->ifa_addr) continue;
        IfInfo& info = ifMap[QString::fromLatin1(p->ifa_name)];
        info.name = QString::fromLatin1(p->ifa_name);
        info.flags = p->ifa_flags;
        if (p->ifa_addr->sa_family == AF_INET) {
            auto* sa = (struct sockaddr_in*)p->ifa_addr;
            char ipBuf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sa->sin_addr, ipBuf, sizeof(ipBuf));
            info.ips.append(QString::fromLatin1(ipBuf));
        }
    }

    // 闁冲厜鍋撻柍鍏夊亾 WiFi SSID (needs com.apple.developer.networking.wifi-info) 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾
    QString wifiSSID = iosCopyWiFiSSID();

    freeifaddrs(ifa);

    // 闁冲厜鍋撻柍鍏夊亾 Build output table 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋?
    static const QVector<DiagnosticFormatter::ColSpec> kNetCols = {
        {"Iface",       12, false},
        {"Type",        12, false},
        {"Status",      10, false},
        {"IPv4 Address",18, false},
        {"SSID",        20, false},
    };
    QList<QStringList> netRows;

    for (auto it = ifMap.begin(); it != ifMap.end(); ++it) {
        const IfInfo& info = it.value();
        bool isLoopback = (info.flags & IFF_LOOPBACK);
        bool isUp = (info.flags & IFF_UP) && (info.flags & IFF_RUNNING);

        QString ifType;
        if (isLoopback) ifType = QStringLiteral("Loopback");
        else if (info.name.startsWith("en")) ifType = QStringLiteral("WiFi");
        else if (info.name.startsWith("pdp_ip")) ifType = QStringLiteral("Cellular");
        else if (info.name.startsWith("utun") || info.name.startsWith("llw")) ifType = QStringLiteral("VPN/Tunnel");
        else ifType = QStringLiteral("Other");

        QString status = isLoopback ? QStringLiteral("UP") : (isUp ? QStringLiteral("UP") : QStringLiteral("DOWN"));
        QString ip4 = info.ips.isEmpty() ? (isLoopback ? QStringLiteral("127.0.0.1") : QStringLiteral("-")) : info.ips.join(',');
        QString ssid = (!isLoopback && info.name.startsWith("en") && !wifiSSID.isEmpty()) ? wifiSSID : QStringLiteral("-");

        netRows.append({info.name, ifType, status, ip4, ssid});
    }
    out.append(DiagnosticFormatter::formatTable(kNetCols, netRows));

    // 闁冲厜鍋撻柍鍏夊亾 Cellular info 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋?
    QVariantMap cell = iosCellularInfo();
    const bool hasCellIdentity = hasCellularIdentity(cell);
    if (hasCellIdentity) {
        out.append(QString());
        out.append(QStringLiteral("Cellular Information:"));
        const QVariantList sims = cell.value(QStringLiteral("sims")).toList();
        if (sims.size() > 1) {
            for (const QVariant& v : sims) {
                const QVariantMap sim = v.toMap();
                const QString carrier = sim.value(QStringLiteral("carrierName")).toString();
                const QString radio = sim.value(QStringLiteral("radioAccess")).toString();
                out.append(QStringLiteral("  SIM %1: %2%3")
                    .arg(QString::number(sim.value(QStringLiteral("slot")).toInt()),
                         carrier.isEmpty() ? QStringLiteral("(carrier hidden)") : carrier,
                         radio.isEmpty() ? QString() : QStringLiteral(" \u2014 %1").arg(radio)));
            }
        } else {
            if (hasNonEmptyValue(cell, "carrierName"))
                out.append(QStringLiteral("  Carrier: %1").arg(cell["carrierName"].toString()));
            if (hasNonEmptyValue(cell, "radioAccess"))
                out.append(QStringLiteral("  Radio Access: %1").arg(cell["radioAccess"].toString()));
        }
        const QString cellIp = iosInterfaceIPv4(QStringLiteral("pdp_ip0"));
        const QString cellGw = iosGatewayForInterface(QStringLiteral("pdp_ip0"));
        out.append(QStringLiteral("  IP Address: %1").arg(cellIp.isEmpty() ? QStringLiteral("(not assigned)") : cellIp));
        if (!cellGw.isEmpty())
            out.append(QStringLiteral("  Gateway: %1").arg(cellGw));
        if (hasNonEmptyValue(cell, "signalNotice"))
            out.append(QStringLiteral("  Signal: %1").arg(cell["signalNotice"].toString()));
    }

#else
    // Linux: use getifaddrs
    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) != 0) {
        r.rawOutput = QStringLiteral("No adapters found");
        r.details = r.rawOutput; r.status = DiagStatus::Warning;
        r.summary = QStringLiteral("Failed to enumerate adapters");
        r.durationMs = t.elapsed(); return r;
    }

    // Collect per-interface info
    struct IfInfo { QString name; QStringList ips; QString mac; int flags; };
    QMap<QString, IfInfo> ifMap;
    for (auto* p = ifa; p; p = p->ifa_next) {
        if (!p->ifa_addr) continue;
        IfInfo& info = ifMap[QString::fromLatin1(p->ifa_name)];
        info.name = QString::fromLatin1(p->ifa_name);
        info.flags = p->ifa_flags;
        if (p->ifa_addr->sa_family == AF_INET) {
            auto* sa = (struct sockaddr_in*)p->ifa_addr;
                info.ips.append(ip4ToStr(sa->sin_addr));
        }
#ifndef __APPLE__
        if (p->ifa_addr->sa_family == AF_PACKET && p->ifa_addr->sa_data) {
            auto* sll = (struct sockaddr_ll*)p->ifa_addr;
            unsigned char mac[6];
            memcpy(mac, sll->sll_addr, 6);
            if (mac[0] || mac[1] || mac[2] || mac[3] || mac[4] || mac[5])
                info.mac = macToStr(mac);
        }
#endif
    }
    freeifaddrs(ifa);

    // 闁冲厜鍋撻柍鍏夊亾 ifconfig -s style table with MAC/IPv4 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋?
    static const QVector<DiagnosticFormatter::ColSpec> kNetCols = {
        {"Iface",       12, false},
        {"MTU",          4, true},
        {"Status",      10, false},
        {"MAC Address", 20, false},
        {"IPv4 Address", 0, false},
    };
    QList<QStringList> netRows;

    for (auto it = ifMap.begin(); it != ifMap.end(); ++it) {
        const IfInfo& info = it.value();
        bool isLoopback = (info.flags & IFF_LOOPBACK);

        // Read MTU, operstate from /sys
        QString mtu = QStringLiteral("-"), state = QStringLiteral("DOWN");
        QFile mtuFile(QStringLiteral("/sys/class/net/%1/mtu").arg(info.name));
        if (mtuFile.open(QIODevice::ReadOnly)) mtu = QString::fromLatin1(mtuFile.readAll().trimmed());
        QFile stateFile(QStringLiteral("/sys/class/net/%1/operstate").arg(info.name));
        if (stateFile.open(QIODevice::ReadOnly)) state = QString::fromLatin1(stateFile.readAll().trimmed()).toUpper();
        if (isLoopback) state = QStringLiteral("UP");

        QString mac = info.mac.isEmpty() ? QStringLiteral("-") : info.mac;
        QString ip4 = info.ips.isEmpty() ? (isLoopback ? QStringLiteral("127.0.0.1") : QStringLiteral("-")) : info.ips.join(',');

        netRows.append({info.name, mtu, state, mac, ip4});
    }
    out.append(DiagnosticFormatter::formatTable(kNetCols, netRows));
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = QStringLiteral("Network adapters enumerated");
    r.durationMs = t.elapsed();
    return r;
}

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// G1 闁?Active Connections (netstat -an format)
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
DiagnosticResult activeConnections(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

    out.append(QString());
    out.append(QStringLiteral("Active Connections (netstat -an style)"));
    out.append(QString());

    // Collect raw data first to compute aligned IP:Port widths
    struct ConnEntry { QString proto, localIp, remoteIp, state; int localPort, remotePort; };
    QList<ConnEntry> rawConns;

#ifdef _WIN32
    ULONG bufLen = 0;
    GetExtendedTcpTable(nullptr, &bufLen, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    QByteArray tcpBuf(bufLen, '\0');
    auto* tcpTable = (MIB_TCPTABLE_OWNER_PID*)tcpBuf.data();
    if (GetExtendedTcpTable(tcpTable, &bufLen, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
        for (DWORD i = 0; i < tcpTable->dwNumEntries; i++) {
            auto& row = tcpTable->table[i];
            struct in_addr la, ra;
            la.S_un.S_addr = row.dwLocalAddr; ra.S_un.S_addr = row.dwRemoteAddr;
            rawConns.append({QStringLiteral("TCP"),
                ip4ToStr(la), ip4ToStr(ra),
                QString::fromLatin1(tcpStateName(row.dwState)),
                (int)ntohs((u_short)row.dwLocalPort), (int)ntohs((u_short)row.dwRemotePort)});
        }
    }
#else
    auto parseProcNet = [&](const QString& path, const QString& proto, bool isUdp) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return;
        QTextStream ts(&f);
        ts.readLine();
        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            if (line.isEmpty()) continue;
            QStringList cols = line.split(QRegularExpression("\\s+"));
            if (cols.size() < 10) continue;
            QStringList local = cols[1].split(':');
            QStringList remote = cols[2].split(':');
            auto hexToIp = [](const QString& hex) -> QString {
                bool ok; uint32_t ip = hex.toUInt(&ok, 16);
                return ipToStr(ip);
            };
            int state = cols[3].toInt(nullptr, 16);
            rawConns.append({proto,
                hexToIp(local[0]), remote.size()>0 ? hexToIp(remote[0]) : QStringLiteral("0.0.0.0"),
                isUdp ? QStringLiteral("*:*") : QString::fromLatin1(tcpStateName(state)),
                local[1].toInt(nullptr, 16),
                remote.size()>1 ? remote[1].toInt(nullptr, 16) : 0});
        }
    };
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID) && !defined(__APPLE__)
    parseProcNet(QStringLiteral("/proc/net/tcp"),  QStringLiteral("TCP"),  false);
    parseProcNet(QStringLiteral("/proc/net/tcp6"), QStringLiteral("TCP6"), false);
    parseProcNet(QStringLiteral("/proc/net/udp"),  QStringLiteral("UDP"),  true);
    parseProcNet(QStringLiteral("/proc/net/udp6"), QStringLiteral("UDP6"), true);
#endif
#endif

    // Compute max IP width and max port width for each address column
    int localIpW = 0, remoteIpW = 0, portW = 0;
    for (const auto& c : rawConns) {
        localIpW  = qMax(localIpW,  static_cast<int>(c.localIp.length()));
        remoteIpW = qMax(remoteIpW, static_cast<int>(c.remoteIp.length()));
        portW     = qMax(portW, static_cast<int>(QString::number(c.localPort).length()));
        portW     = qMax(portW, static_cast<int>(QString::number(c.remotePort).length()));
    }
    if (portW < 5) portW = 5; // minimum port column width

    // Build formatted rows: IP left-justified + ":" + port left-justified
    QList<QStringList> connRows;
    for (const auto& c : rawConns) {
        connRows.append({
            c.proto,
            QStringLiteral("%1:%2").arg(c.localIp.leftJustified(localIpW)).arg(QString::number(c.localPort).leftJustified(portW)),
            QStringLiteral("%1:%2").arg(c.remoteIp.leftJustified(remoteIpW)).arg(QString::number(c.remotePort).leftJustified(portW)),
            c.state});
    }

    static const QVector<DiagnosticFormatter::ColSpec> kConnCols = {
        {"Proto",           6, false},
        {"Local Address",   0, false},
        {"Foreign Address", 0, false},
        {"State",           0, false},
    };
    if (!connRows.isEmpty())
        out.append(DiagnosticFormatter::formatTable(kConnCols, connRows));
    else {
#ifdef PLATFORM_IOS
        out.append(QStringLiteral("  [iOS] Active connections: unavailable (restricted by Apple)"));
        out.append(QStringLiteral("  iOS sandbox prevents reading /proc/net/tcp — use Xcode network monitor"));
#else
        out.append(QStringLiteral("  (no active connections)"));
#endif
    }
    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
#ifdef PLATFORM_IOS
    r.status = DiagStatus::Skipped;
    r.summary = QStringLiteral("Unavailable on iOS (sandbox restricts socket enumeration)");
#elif defined(__APPLE__)
    r.status = DiagStatus::Skipped;
    r.summary = QStringLiteral("Unavailable on macOS (/proc not available; use netstat -an)");
#else
    r.status = DiagStatus::Pass;
    r.summary = QStringLiteral("Active connections enumerated");
#endif
    r.durationMs = t.elapsed();
    return r;
}

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// G1 闁?Cellular Info (iOS: CoreTelephony; other platforms: not available)
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
DiagnosticResult cellularInfo(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Cellular Information:"));
    out.append(QString());

#ifdef PLATFORM_IOS
    QVariantMap cell = iosCellularInfo();
    const bool hasCellIdentity = hasCellularIdentity(cell);
    const QString cellIp = iosInterfaceIPv4(QStringLiteral("pdp_ip0"));
    const QString cellGw = iosGatewayForInterface(QStringLiteral("pdp_ip0"));
    const QVariantList sims = cell.value(QStringLiteral("sims")).toList();
    const bool multiSim = sims.size() > 1;
    if (hasCellIdentity || !sims.isEmpty()) {
        if (multiSim)
            out.append(QStringLiteral("  %1 SIM / eSIM lines active:").arg(sims.size()));
        for (const QVariant& v : sims) {
            const QVariantMap sim = v.toMap();
            const QString pad = multiSim ? QStringLiteral("    ") : QStringLiteral("  ");
            if (multiSim)
                out.append(QStringLiteral("  SIM %1:").arg(sim.value(QStringLiteral("slot")).toInt()));
            const QString carrier = sim.value(QStringLiteral("carrierName")).toString();
            out.append(QStringLiteral("%1Carrier: %2").arg(pad,
                carrier.isEmpty() ? QStringLiteral("(hidden by iOS 16+)") : carrier));
            const QString radio = sim.value(QStringLiteral("radioAccess")).toString();
            if (!radio.isEmpty())
                out.append(QStringLiteral("%1Radio Access: %2").arg(pad, radio));
        }
        out.append(QStringLiteral("  %1: %2")
            .arg(multiSim ? QStringLiteral("Data IP (active line)") : QStringLiteral("IP Address"),
                 cellIp.isEmpty() ? QStringLiteral("(not assigned)") : cellIp));
        if (!cellGw.isEmpty())
            out.append(QStringLiteral("  Gateway: %1").arg(cellGw));
        if (hasNonEmptyValue(cell, "signalNotice"))
            out.append(QStringLiteral("  Signal: %1").arg(cell["signalNotice"].toString()));
        out.append(QString());
        r.status = DiagStatus::Pass;
        if (multiSim) {
            QStringList rats;
            for (const QVariant& v : sims) {
                const QString ra = v.toMap().value(QStringLiteral("radioAccess")).toString();
                rats.append(ra.isEmpty() ? QStringLiteral("\u2014") : ra);
            }
            r.summary = QStringLiteral("%1 SIMs (%2)").arg(sims.size()).arg(rats.join(QStringLiteral(", ")));
        } else {
            r.summary = cellularSummary(cell);
        }
    } else {
        out.append(QStringLiteral("  No cellular service available"));
        if (!cellIp.isEmpty())
            out.append(QStringLiteral("  IP Address: %1").arg(cellIp));
        if (!cellGw.isEmpty())
            out.append(QStringLiteral("  Gateway: %1").arg(cellGw));
        if (hasNonEmptyValue(cell, "signalNotice"))
            out.append(QStringLiteral("  Signal: %1").arg(cell["signalNotice"].toString()));
        r.status = DiagStatus::Info; r.summary = QStringLiteral("No cellular service");
    }
#else
    out.append(QStringLiteral("  [Skipped] Cellular info requires iOS — not applicable on this platform"));
    r.status = DiagStatus::Skipped; r.summary = QStringLiteral("Not applicable (iOS only)");
#endif
    out.append(QString());
    r.rawOutput = out.join('\n'); r.details = r.rawOutput;
    return r;
}

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// G1 闁?IP Configuration (Windows ipconfig /all format 1:1)
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
DiagnosticResult ipConfiguration(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

#ifdef _WIN32
    // 闁冲厜鍋撻柍鍏夊亾 Windows IP Configuration header 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾
    char hostname[256]; DWORD hnLen = sizeof(hostname);
    GetComputerNameExA(ComputerNameDnsHostname, hostname, &hnLen);
    out.append(QString());
    out.append(QStringLiteral("Windows IP Configuration"));
    out.append(QString());
    out.append(QStringLiteral("   Host Name . . . . . . . . . . . . : %1").arg(QString::fromLatin1(hostname)));
    out.append(QStringLiteral("   Primary Dns Suffix  . . . . . . . :"));
    out.append(QStringLiteral("   Node Type . . . . . . . . . . . . : Hybrid"));
    out.append(QStringLiteral("   IP Routing Enabled. . . . . . . . : No"));
    out.append(QStringLiteral("   WINS Proxy Enabled. . . . . . . . : No"));
    out.append(QString());

    // Per-adapter details via GetAdaptersAddresses
    ULONG bufLen = 15000;
    QByteArray buf(bufLen, '\0');
    PIP_ADAPTER_ADDRESSES adapters = (PIP_ADAPTER_ADDRESSES)buf.data();
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS|GAA_FLAG_INCLUDE_PREFIX, nullptr, adapters, &bufLen) == NO_ERROR) {
        for (auto* a = adapters; a; a = a->Next) {
            QString ifType = (a->IfType == IF_TYPE_IEEE80211) ? QStringLiteral("Wireless LAN adapter")
                          : (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) ? QStringLiteral("Unknown adapter")
                          : QStringLiteral("Ethernet adapter");
            out.append(QStringLiteral("%1 %2:").arg(ifType, QString::fromWCharArray(a->FriendlyName)));
            out.append(QString());
            out.append(QStringLiteral("   Connection-specific DNS Suffix  . :"));
            if (a->PhysicalAddressLength > 0)
                out.append(QStringLiteral("   Physical Address. . . . . . . . . : %1").arg(macToStr(a->PhysicalAddress)));
            out.append(QStringLiteral("   DHCP Enabled. . . . . . . . . . . : %1").arg(a->Flags & IP_ADAPTER_DHCP_ENABLED ? "Yes" : "No"));
            out.append(QStringLiteral("   Autoconfiguration Enabled . . . . : Yes"));
            for (auto* ua = a->FirstUnicastAddress; ua; ua = ua->Next) {
                char ip[64]; DWORD ipLen = sizeof(ip);
                WSAAddressToStringA(ua->Address.lpSockaddr, ua->Address.iSockaddrLength, nullptr, ip, &ipLen);
                int family = ua->Address.lpSockaddr->sa_family;
                if (family == AF_INET) {
                    out.append(QStringLiteral("   IPv4 Address. . . . . . . . . . . : %1 (Preferred)").arg(QString::fromLatin1(ip)));
                } else if (family == AF_INET6) {
                    out.append(QStringLiteral("   Link-local IPv6 Address . . . . . : %1 (Preferred)").arg(QString::fromLatin1(ip)));
                }
            }
            for (auto* gw = a->FirstGatewayAddress; gw; gw = gw->Next) {
                char ip[64]; DWORD ipLen = sizeof(ip);
                WSAAddressToStringA(gw->Address.lpSockaddr, gw->Address.iSockaddrLength, nullptr, ip, &ipLen);
                out.append(QStringLiteral("   Default Gateway . . . . . . . . . : %1").arg(QString::fromLatin1(ip)));
            }
            if (a->Dhcpv4Server.iSockaddrLength > 0) {
                char ip[64]; DWORD ipLen = sizeof(ip);
                WSAAddressToStringA(a->Dhcpv4Server.lpSockaddr, a->Dhcpv4Server.iSockaddrLength, nullptr, ip, &ipLen);
                out.append(QStringLiteral("   DHCP Server . . . . . . . . . . . : %1").arg(QString::fromLatin1(ip)));
            }
            bool dnsFirst = true;
            for (auto* dns = a->FirstDnsServerAddress; dns; dns = dns->Next) {
                char ip[64]; DWORD ipLen = sizeof(ip);
                WSAAddressToStringA(dns->Address.lpSockaddr, dns->Address.iSockaddrLength, nullptr, ip, &ipLen);
                out.append(QStringLiteral("   DNS Servers . . . . . . . . . . . : %1").arg(QString::fromLatin1(ip)));
                if (dnsFirst) dnsFirst = false;
            }
            out.append(QStringLiteral("   NetBIOS over Tcpip. . . . . . . . : Enabled"));
            out.append(QString());
        }
    }
#else
    // 闁冲厜鍋撻柍鍏夊亾 Linux IP Configuration (ipconfig /all style) 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋?
    char hostname[256];
    out.append(QString());
    out.append(QStringLiteral("IP Configuration"));
    out.append(QString());
    out.append(QStringLiteral("   Host Name . . . . . . . . . . . . : %1")
        .arg(gethostname(hostname, sizeof(hostname)) == 0 ? QString::fromLatin1(hostname) : QStringLiteral("Unknown")));
    // IP forwarding status
    QFile ipForward(QStringLiteral("/proc/sys/net/ipv4/ip_forward"));
    bool routingEnabled = false;
    if (ipForward.open(QIODevice::ReadOnly))
        routingEnabled = ipForward.readAll().trimmed() == "1";
    out.append(QStringLiteral("   IP Routing Enabled. . . . . . . . : %1").arg(routingEnabled ? "Yes" : "No"));
    // DNS suffix search list from resolv.conf
    QStringList dnsServers, searchDomains;
    QFile resolv(QStringLiteral("/etc/resolv.conf"));
    if (resolv.open(QIODevice::ReadOnly)) {
        QTextStream ts(&resolv);
        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            if (line.startsWith("nameserver ")) dnsServers.append(line.mid(11));
            else if (line.startsWith("search ")) {
                for (const auto& d : line.mid(7).split(' ', Qt::SkipEmptyParts))
                    searchDomains.append(d);
            }
            else if (line.startsWith("domain ")) searchDomains.append(line.mid(7));
        }
    }
    if (!searchDomains.isEmpty())
        out.append(QStringLiteral("   DNS Suffix Search List. . . . . . : %1").arg(searchDomains.join(' ')));
    out.append(QString());

    // Per-adapter details via getifaddrs + /sys/class/net
    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) == 0) {
        struct IfInfo { QString name; QStringList ips4; QStringList ips6; QStringList masks4; QString mac; int flags; };
        QMap<QString, IfInfo> ifMap;
        for (auto* p = ifa; p; p = p->ifa_next) {
            if (!p->ifa_addr) continue;
            IfInfo& info = ifMap[QString::fromLatin1(p->ifa_name)];
            info.name = QString::fromLatin1(p->ifa_name);
            info.flags = p->ifa_flags;
            if (p->ifa_addr->sa_family == AF_INET) {
                auto* sa = (struct sockaddr_in*)p->ifa_addr;
                info.ips4.append(ip4ToStr(sa->sin_addr));
            } else if (p->ifa_addr->sa_family == AF_INET6) {
                char buf6[INET6_ADDRSTRLEN];
                auto* sa6 = (struct sockaddr_in6*)p->ifa_addr;
                inet_ntop(AF_INET6, &sa6->sin6_addr, buf6, sizeof(buf6));
                info.ips6.append(QString::fromLatin1(buf6));
            }
            if (p->ifa_netmask && p->ifa_netmask->sa_family == AF_INET) {
                auto* nm = (struct sockaddr_in*)p->ifa_netmask;
                info.masks4.append(ip4ToStr(nm->sin_addr));
            }
#ifndef __APPLE__
            if (p->ifa_addr->sa_family == AF_PACKET) {
                auto* sll = (struct sockaddr_ll*)p->ifa_addr;
                unsigned char mac[6]; memcpy(mac, sll->sll_addr, 6);
                info.mac = macToStr(mac);
            }
#endif
        }
        freeifaddrs(ifa);

        // Build gateway map: ifName 闁?{dest, gw, mask}
        struct RouteEntry { QString ifName; uint32_t dest; uint32_t gw; uint32_t mask; };
        QVector<RouteEntry> routes;
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID) && !defined(__APPLE__)
        QFile routeFile(QStringLiteral("/proc/net/route"));
        if (routeFile.open(QIODevice::ReadOnly)) {
            QTextStream ts(&routeFile);
            ts.readLine(); // header
            while (!ts.atEnd()) {
                QString line = ts.readLine().trimmed();
                if (line.isEmpty()) continue;
                QStringList cols = line.split('\t');
                if (cols.size() >= 11) {
                    RouteEntry re;
                    re.ifName = cols[0];
                    bool ok1, ok2, ok3;
                    re.dest = cols[1].toUInt(&ok1, 16);
                    re.gw   = cols[2].toUInt(&ok2, 16);
                    re.mask = cols[7].toUInt(&ok3, 16);
                    if (ok1 && ok2 && ok3) routes.append(re);
                }
            }
        }
#endif // !PLATFORM_IOS

        for (auto it = ifMap.begin(); it != ifMap.end(); ++it) {
            const auto& info = it.value();
            bool isLoopback = (info.flags & IFF_LOOPBACK);
            QString ifName = info.name;

            // Determine adapter type
            QString adapterLabel;
            if (isLoopback)
                adapterLabel = QStringLiteral("Unknown adapter %1:").arg(ifName);
#ifdef PLATFORM_IOS
            else if (ifName.startsWith("en"))
                adapterLabel = QStringLiteral("Wireless LAN adapter %1:").arg(ifName);
            else if (ifName.startsWith("pdp_ip"))
                adapterLabel = QStringLiteral("Cellular adapter %1:").arg(ifName);
#else
            else if (QFile::exists(QStringLiteral("/sys/class/net/%1/wireless").arg(ifName)))
                adapterLabel = QStringLiteral("Wireless LAN adapter %1:").arg(ifName);
#endif
            else
                adapterLabel = QStringLiteral("Ethernet adapter %1:").arg(ifName);

            out.append(adapterLabel);
            out.append(QString());

            // Connection-specific DNS Suffix
            out.append(QStringLiteral("   Connection-specific DNS Suffix  . :"));

            // Description (driver info from sysfs 闁?Linux only)
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID) && !defined(__APPLE__)
            QFile descFile(QStringLiteral("/sys/class/net/%1/device/uevent").arg(ifName));
            if (descFile.open(QIODevice::ReadOnly)) {
                QString uevent = QString::fromLatin1(descFile.readAll());
                for (const auto& line : uevent.split('\n')) {
                    if (line.startsWith("DRIVER="))
                        out.append(QStringLiteral("   Description . . . . . . . . . . . : %1").arg(line.mid(7)));
                }
            }
#endif // !PLATFORM_IOS (sysfs description)

            // Physical Address (MAC)
            if (!info.mac.isEmpty() && !info.mac.startsWith("00-00-00"))
                out.append(QStringLiteral("   Physical Address. . . . . . . . . : %1").arg(info.mac));

            // DHCP Enabled
            bool dhcpEnabled =
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID) && !defined(__APPLE__)
                QFile::exists(QStringLiteral("/run/systemd/netif/leases/%1").arg(ifName))
                            || QFile::exists(QStringLiteral("/var/lib/dhcp/dhclient.%1.leases").arg(ifName));
#else
                true;  // iOS always uses system-managed DHCP
#endif
            out.append(QStringLiteral("   DHCP Enabled. . . . . . . . . . . : %1").arg(dhcpEnabled ? "Yes" : "No"));
            out.append(QStringLiteral("   Autoconfiguration Enabled . . . . : Yes"));

            // IPv6 addresses
            for (const auto& ip6 : info.ips6) {
                if (!ip6.startsWith("fe80:"))
                    out.append(QStringLiteral("   IPv6 Address. . . . . . . . . . . : %1 (Preferred)").arg(ip6));
                else
                    out.append(QStringLiteral("   Link-local IPv6 Address . . . . . : %1 (Preferred)").arg(ip6));
            }

            // IPv4 addresses
            for (int i = 0; i < info.ips4.size(); i++) {
                out.append(QStringLiteral("   IPv4 Address. . . . . . . . . . . : %1 (Preferred)").arg(info.ips4[i]));
                if (i < info.masks4.size())
                    out.append(QStringLiteral("   Subnet Mask . . . . . . . . . . . : %1").arg(info.masks4[i]));
            }

            // Lease info from dhclient or systemd-networkd (Linux only)
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID) && !defined(__APPLE__)
            if (dhcpEnabled) {
                QStringList leasePaths = {
                    QStringLiteral("/run/systemd/netif/leases/%1").arg(ifName),
                    QStringLiteral("/var/lib/dhcp/dhclient.%1.leases").arg(ifName)
                };
                for (const auto& lp : leasePaths) {
                    QFile lease(lp);
                    if (lease.open(QIODevice::ReadOnly)) {
                        QTextStream ls(&lease);
                        while (!ls.atEnd()) {
                            QString lline = ls.readLine().trimmed();
                            if (lline.startsWith("ROUTER="))
                                out.append(QStringLiteral("   DHCP Server . . . . . . . . . . . : %1").arg(lline.mid(7)));
                        }
                    }
                }
            }
#endif // !PLATFORM_IOS (lease files)

            // Default Gateway
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID) && !defined(__APPLE__)
            for (const auto& re : routes) {
                if (re.ifName == ifName && re.dest == 0 && re.gw != 0)
                    out.append(QStringLiteral("   Default Gateway . . . . . . . . . : %1").arg(ipToStr(re.gw)));
            }
#endif

            // DNS Servers
            if (!dnsServers.isEmpty()) {
                bool first = true;
                for (const auto& dns : dnsServers) {
                    out.append(QStringLiteral("   DNS Servers . . . . . . . . . . . : %1").arg(dns));
                    if (first) first = false;
                }
            }

            // Link speed + MTU (from sysfs 闁?Linux only)
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID) && !defined(__APPLE__)
            QFile speedFile(QStringLiteral("/sys/class/net/%1/speed").arg(ifName));
            if (speedFile.open(QIODevice::ReadOnly)) {
                QString s = QString::fromLatin1(speedFile.readAll().trimmed());
                if (!s.isEmpty() && s != "-1")
                    out.append(QStringLiteral("   Link Speed . . . . . . . . . . . . : %1 Mbps").arg(s));
            }
            QFile mtuFile(QStringLiteral("/sys/class/net/%1/mtu").arg(ifName));
            if (mtuFile.open(QIODevice::ReadOnly))
                out.append(QStringLiteral("   MTU . . . . . . . . . . . . . . . : %1").arg(QString::fromLatin1(mtuFile.readAll().trimmed())));
#endif // !PLATFORM_IOS (sysfs speed/MTU)

            out.append(QString());
        }
    }
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = QStringLiteral("IP configuration collected");
    r.durationMs = t.elapsed();
    return r;
}

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// G1 闁?WiFi Diagnostics (netsh wlan show interfaces format)
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
DiagnosticResult wifiDiagnostics(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

    out.append(QString());
    out.append(QStringLiteral("Wireless LAN information:"));
    out.append(QString());

#ifdef _WIN32
    HANDLE hClient = nullptr;
    DWORD negotiatedVer = 0;
    if (WlanOpenHandle(2, nullptr, &negotiatedVer, &hClient) == ERROR_SUCCESS) {
        PWLAN_INTERFACE_INFO_LIST ifList = nullptr;
        if (WlanEnumInterfaces(hClient, nullptr, &ifList) == ERROR_SUCCESS) {
            for (DWORD i = 0; i < ifList->dwNumberOfItems; i++) {
                auto& wi = ifList->InterfaceInfo[i];
                out.append(QStringLiteral("   Name . . . . . . . . . . . . : %1").arg(QString::fromWCharArray(wi.strInterfaceDescription)));
                {
    wchar_t guidStr[40] = {};
    StringFromGUID2(wi.InterfaceGuid, guidStr, 40);
    out.append(QStringLiteral("   GUID . . . . . . . . . . . . : %1").arg(QString::fromWCharArray(guidStr)));
}
                out.append(QStringLiteral("   State. . . . . . . . . . . . : %1").arg(wi.isState == wlan_interface_state_connected ? "connected" : "disconnected"));
                out.append(QString());
            }
            WlanFreeMemory(ifList);
        }
        WlanCloseHandle(hClient, nullptr);
    }
#else
    // Linux: wireless extensions + /sys/class/net/<wireless_iface>/
    // 闁冲厜鍋撻柍鍏夊亾 WiFi table 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾
    static const QVector<DiagnosticFormatter::ColSpec> kWifiCols = {
        {"Interface", 12, false},
        {"SSID",      20, false},
        {"BSSID",     17, false},
        {"Channel",    8, false},
        {"Signal",     7, false},
        {"Bitrate",    0, false},
    };
    QList<QStringList> wifiRows;
    QSet<QString> seenWifi;
#ifdef PLATFORM_IOS
    QString iosWifiSsidCaptured;
#endif

    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) == 0) {
        for (auto* p = ifa; p; p = p->ifa_next) {
            QString ifName = QString::fromLatin1(p->ifa_name);
#ifdef PLATFORM_IOS
            // iOS: detect WiFi by interface name prefix (en0/en1 are WiFi)
            if (!ifName.startsWith("en"))
                continue;
#else
            if (!QFile::exists(QStringLiteral("/sys/class/net/%1/wireless").arg(ifName)))
                continue;
#endif
            if (seenWifi.contains(ifName)) continue;
            seenWifi.insert(ifName);

            QString ssid = QStringLiteral("-"), bssid = QStringLiteral("-");
            QString channel = QStringLiteral("-"), signal = QStringLiteral("-"), bitrate = QStringLiteral("-");

#ifdef PLATFORM_IOS
            // iOS: retrieve WiFi SSID and BSSID via shared helper
            QVariantMap wifiData = iosWiFiInfo();
            ssid = wifiData.value("ssid", "").toString();
            if (ssid.isEmpty()) ssid = QStringLiteral("-");
            else iosWifiSsidCaptured = ssid;
            bssid = wifiData.value("bssid", "").toString();
            if (bssid.isEmpty()) bssid = QStringLiteral("-");
#endif

#ifdef __linux__
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock >= 0) {
                struct iwreq wrq; memset(&wrq, 0, sizeof(wrq));
                strncpy(wrq.ifr_name, ifName.toUtf8().constData(), IFNAMSIZ - 1);

                char essid[IW_ESSID_MAX_SIZE + 1] = {};
                wrq.u.essid.pointer = essid; wrq.u.essid.length = IW_ESSID_MAX_SIZE + 1; wrq.u.essid.flags = 0;
                if (ioctl(sock, SIOCGIWESSID, &wrq) == 0 && wrq.u.essid.length > 0)
                    ssid = QString::fromUtf8(essid, wrq.u.essid.length);

                if (ioctl(sock, SIOCGIWAP, &wrq) == 0 && wrq.u.ap_addr.sa_family == ARPHRD_ETHER)
                    bssid = macToStr((unsigned char*)wrq.u.ap_addr.sa_data);

                if (ioctl(sock, SIOCGIWFREQ, &wrq) == 0) {
                    double freq = wrq.u.freq.m / 1e9;
                    channel = QStringLiteral("%1 (%2 GHz)").arg((int)((freq - 2.412) / 0.005 + 1)).arg(freq, 0, 'f', 3);
                }
                closeSocket(sock);
            }
#endif

#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID) && !defined(__APPLE__)
            QFile wfile(QStringLiteral("/proc/net/wireless"));
            if (wfile.open(QIODevice::ReadOnly)) {
                QTextStream ts(&wfile); ts.readLine(); ts.readLine();
                while (!ts.atEnd()) {
                    QString line = ts.readLine().trimmed();
                    if (line.startsWith(ifName + ':')) {
                        QStringList cols = line.split(QRegularExpression("\\s+"));
                        if (cols.size() >= 4) signal = cols[3].replace('.', "") + " dBm";
                        break;
                    }
                }
            }
#endif // !PLATFORM_IOS (/proc/net/wireless)

#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID) && !defined(__APPLE__)
            QFile rateFile(QStringLiteral("/sys/class/net/%1/wireless/bitrate").arg(ifName));
            if (rateFile.open(QIODevice::ReadOnly)) bitrate = QString::fromLatin1(rateFile.readAll().trimmed());
#endif

            wifiRows.append({ifName, ssid, bssid, channel, signal, bitrate});
        }
        freeifaddrs(ifa);
    }
    out.append(DiagnosticFormatter::formatTable(kWifiCols, wifiRows));
    if (wifiRows.isEmpty()) out.append(QStringLiteral("  (no wireless interfaces detected)"));
#ifdef PLATFORM_IOS
    {
        const QString wifiIp = iosInterfaceIPv4(QStringLiteral("en0"));
        const QString wifiGw = iosGatewayForInterface(QStringLiteral("en0"));
        out.append(QString());
        out.append(QStringLiteral("  IP Address: %1").arg(wifiIp.isEmpty() ? QStringLiteral("(not connected)") : wifiIp));
        if (!wifiGw.isEmpty())
            out.append(QStringLiteral("  Gateway: %1").arg(wifiGw));
        out.append(QStringLiteral("  Channel/Signal/Bitrate: unavailable on iOS (no public API)"));
        if (iosWifiSsidCaptured.isEmpty()) {
            out.append(QString());
            out.append(QStringLiteral("  Note: SSID/BSSID need the \"Access WiFi Information\" entitlement,"));
            out.append(QStringLiteral("        Location permission, and an active WiFi connection."));
        }
    }
#endif
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = out.size() > 3 ? DiagStatus::Pass : DiagStatus::Info;
    r.summary = QStringLiteral("WiFi diagnostics complete");
    r.durationMs = t.elapsed();
    return r;
}

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// G1 闁?NIC Advanced (wmic nic format)
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
DiagnosticResult nicAdvanced(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

    out.append(QString());
#ifdef _WIN32
    out.append(QStringLiteral("NIC Advanced Properties:"));
    // ... Windows-specific registry queries would go here
    out.append(QStringLiteral("  (use wmic nic / Device Manager for full details)"));
#else
    out.append(QStringLiteral("NIC Advanced Properties (table mode):"));
    out.append(QString());
    static const QVector<DiagnosticFormatter::ColSpec> kNicCols = {
        {"Interface",   12, false},
        {"Speed",        6, true},
        {"Duplex",       6, false},
        {"MTU",          4, true},
        {"Carrier",      7, false},
        {"State",       10, false},
        {"MAC Address", 17, false},
    };
    QList<QStringList> nicRows;

    QSet<QString> seenNic;
    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) == 0) {
        for (auto* p = ifa; p; p = p->ifa_next) {
            QString ifName = QString::fromLatin1(p->ifa_name);
            if (ifName == "lo") continue;
            if (!(p->ifa_flags & IFF_UP)) continue;
            if (seenNic.contains(ifName)) continue;
            seenNic.insert(ifName);

            auto rd = [&](const QString& prop) {
#ifdef PLATFORM_IOS
                // iOS: only MTU is available via getifaddrs; other props are restricted
                if (prop == "mtu") {
                    // SIOCGIFMTU ioctl not available on iOS sandbox; use standard value
                    return QStringLiteral("1500");
                }
                if (prop == "operstate") return QStringLiteral("up");
                return QStringLiteral("-");
#else
                QFile f(QStringLiteral("/sys/class/net/%1/%2").arg(ifName, prop));
                if (f.open(QIODevice::ReadOnly)) return QString::fromLatin1(f.readAll().trimmed());
                return QStringLiteral("-");
#endif
            };

            nicRows.append({ifName, rd("speed"), rd("duplex"), rd("mtu"),
                rd("carrier"), rd("operstate"), rd("address")});
        }
        freeifaddrs(ifa);
    }
    out.append(DiagnosticFormatter::formatTable(kNicCols, nicRows));
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = QStringLiteral("NIC properties collected");
    r.durationMs = t.elapsed();
    return r;
}

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// G1 闁?Wired Diagnostics
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
DiagnosticResult wiredDiagnostics(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

#ifdef PLATFORM_IOS
    // iOS devices have no wired Ethernet NIC, and /sys/class/net is inaccessible,
    // so every field would be blank. Report as Skipped (consistent with ARP/TCP).
    out.append(QString());
    out.append(QStringLiteral("Wired Information:"));
    out.append(QString());
    out.append(QStringLiteral("  [iOS] No wired Ethernet interface — not applicable on iOS devices."));
    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Skipped;
    r.summary = QStringLiteral("Not applicable on iOS (no wired NIC)");
    r.durationMs = t.elapsed();
    return r;
#endif

#ifdef _WIN32
    out.append(QString());
    out.append(QStringLiteral("(use wmic nic get on Windows)"));
    r.rawOutput = out.join('\n'); r.details = r.rawOutput;
    r.status = DiagStatus::Info; r.summary = QStringLiteral("Windows wired diagnostics delegate to wmic");
    r.durationMs = t.elapsed(); return r;
#else

    out.append(QString());
    out.append(QStringLiteral("Wired Information (table mode):"));
    out.append(QString());
    static const QVector<DiagnosticFormatter::ColSpec> kWiredCols = {
        {"Interface",   12, false},
        {"Speed",        6, true},
        {"Duplex",       6, false},
        {"MTU",          4, true},
        {"Link",         4, false},
        {"State",       10, false},
        {"MAC Address", 17, false},
    };
    QList<QStringList> wiredDataRows;

    QSet<QString> seenWired;
    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) == 0) {
        for (auto* p = ifa; p; p = p->ifa_next) {
            QString ifName = QString::fromLatin1(p->ifa_name);
            if (ifName == "lo") continue;
            if (!(p->ifa_flags & IFF_UP)) continue;
            if (seenWired.contains(ifName)) continue;
            // Skip wireless interfaces
#ifdef PLATFORM_IOS
            // iOS: classify by interface name prefix
            if (ifName.startsWith("en") || ifName.startsWith("pdp_ip")) continue;
#else
            if (QFile::exists(QStringLiteral("/sys/class/net/%1/wireless").arg(ifName))) continue;
#endif
            seenWired.insert(ifName);

            auto rd = [&](const QString& prop) {
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID) && !defined(__APPLE__)
                QFile f(QStringLiteral("/sys/class/net/%1/%2").arg(ifName, prop));
                if (f.open(QIODevice::ReadOnly)) return QString::fromLatin1(f.readAll().trimmed());
#endif
                return QStringLiteral("-");
            };

            wiredDataRows.append({ifName, rd("speed"), rd("duplex"), rd("mtu"),
                rd("carrier"), rd("operstate"), rd("address")});
        }
        freeifaddrs(ifa);
    }
    out.append(DiagnosticFormatter::formatTable(kWiredCols, wiredDataRows));
    if (wiredDataRows.isEmpty()) out.append(QStringLiteral("  (no wired interfaces detected)"));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = out.size() > 3 ? DiagStatus::Pass : DiagStatus::Info;
    r.summary = QStringLiteral("Wired diagnostics complete");
    r.durationMs = t.elapsed();
    return r;
#endif
}

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// G1 闁?DHCP Status (ipconfig /all DHCP section format)
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
DiagnosticResult dhcpStatus(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;
    QStringList dhcpSummary; // "eth0=192.168.1.100"

    out.append(QString());
    out.append(QStringLiteral("DHCP Client Status"));
    out.append(QString());

#ifdef _WIN32
    ULONG bufLen = 15000;
    QByteArray buf(bufLen, '\0');
    PIP_ADAPTER_ADDRESSES adapters = (PIP_ADAPTER_ADDRESSES)buf.data();
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS, nullptr, adapters, &bufLen) == NO_ERROR) {
        for (auto* a = adapters; a; a = a->Next) {
            bool dhcp = (a->Flags & IP_ADAPTER_DHCP_ENABLED) != 0;
            out.append(QStringLiteral("   Adapter: %1").arg(QString::fromWCharArray(a->FriendlyName)));
            out.append(QStringLiteral("   DHCP Enabled. . . . . . . . . . . : %1").arg(dhcp ? "Yes" : "No"));
            if (a->Dhcpv4Server.iSockaddrLength > 0) {
                char ip[64]; DWORD ipLen = sizeof(ip);
                WSAAddressToStringA(a->Dhcpv4Server.lpSockaddr, a->Dhcpv4Server.iSockaddrLength, nullptr, ip, &ipLen);
                out.append(QStringLiteral("   DHCP Server . . . . . . . . . . . : %1").arg(QString::fromLatin1(ip)));
            }
            if (dhcp && a->FirstUnicastAddress) {
                char ip[64]; DWORD ipLen = sizeof(ip);
                WSAAddressToStringA(a->FirstUnicastAddress->Address.lpSockaddr, a->FirstUnicastAddress->Address.iSockaddrLength, nullptr, ip, &ipLen);
                dhcpSummary.append(QStringLiteral("%1=%2").arg(QString::fromWCharArray(a->FriendlyName), QString::fromLatin1(ip)));
            }
            out.append(QString());
        }
    }
#else
#ifdef PLATFORM_IOS
    out.append(QStringLiteral("  [iOS] DHCP lease details: unavailable (restricted by Apple)"));
    out.append(QStringLiteral("  DHCP is system-managed on iOS; lease files are not accessible."));
#else
    bool anyDhcp = false;
    // 1. systemd-networkd lease files (most detailed)
    QDir leaseDir(QStringLiteral("/run/systemd/netif/leases"));
    if (leaseDir.exists()) {
        for (const auto& fi : leaseDir.entryInfoList(QDir::Files)) {
            QFile f(fi.absoluteFilePath());
            if (f.open(QIODevice::ReadOnly)) {
                QString ifName = fi.fileName();
                out.append(QStringLiteral("   Interface: %1").arg(ifName));
                out.append(QStringLiteral("   DHCP Enabled. . . . . . . . . . . : Yes"));
                anyDhcp = true;
                QTextStream ts(&f);
                while (!ts.atEnd()) {
                    QString line = ts.readLine().trimmed();
                    if (line.startsWith("ADDRESS="))
                        { QString ip = line.mid(8); out.append(QStringLiteral("   IPv4 Address. . . . . . . . . . . : %1 (Preferred)").arg(ip)); if (!dhcpSummary.contains(ifName + "=" + ip)) dhcpSummary.append(QStringLiteral("%1=%2").arg(ifName, ip)); }
                    else if (line.startsWith("NETMASK="))
                        out.append(QStringLiteral("   Subnet Mask . . . . . . . . . . . : %1").arg(line.mid(8)));
                    else if (line.startsWith("ROUTER="))
                        out.append(QStringLiteral("   DHCP Server / Gateway . . . . . . : %1").arg(line.mid(7)));
                    else if (line.startsWith("SERVER_ADDRESS="))
                        out.append(QStringLiteral("   DHCP Server . . . . . . . . . . . : %1").arg(line.mid(15)));
                    else if (line.startsWith("DNS="))
                        out.append(QStringLiteral("   DNS Servers . . . . . . . . . . . : %1").arg(line.mid(4)));
                    else if (line.startsWith("LEASE_TIME=")) {
                        int secs = line.mid(11).toInt();
                        out.append(QStringLiteral("   Lease Duration . . . . . . . . . : %1").arg(secs >= 3600 ? QStringLiteral("%1 hours").arg(secs / 3600) : QStringLiteral("%1 seconds").arg(secs)));
                    }
                }
                out.append(QString());
            }
        }
    }

    // 2. dhclient leases as fallback
    QDir dhclientDir(QStringLiteral("/var/lib/dhcp"));
    if (dhclientDir.exists()) {
        for (const auto& fi : dhclientDir.entryInfoList({"dhclient*.leases"}, QDir::Files)) {
            QFile f(fi.absoluteFilePath());
            if (f.open(QIODevice::ReadOnly)) {
                QTextStream ts(&f);
                QString currentIface, currentIp;
                while (!ts.atEnd()) {
                    QString line = ts.readLine().trimmed();
                    if (line.startsWith("interface ")) currentIface = line.mid(10).remove('"').remove(';');
                    if (line.startsWith("fixed-address ")) currentIp = line.mid(14).remove(' ').remove(';');
                    if (line.contains("dhcp-server-identifier"))
                        out.append(QStringLiteral("   DHCP Server . . . . . . . . . . . : %1").arg(line.section(' ', -1).remove(';')));
                    if (line.contains("renew ")) out.append(QStringLiteral("   Lease Renew . . . . . . . . . . . : %1").arg(line.section(' ', 2, 3).remove(';')));
                    if (line.contains("expire ")) out.append(QStringLiteral("   Lease Expires . . . . . . . . . . : %1").arg(line.section(' ', 2, 3).remove(';')));
                }
                if (!currentIface.isEmpty() && !currentIp.isEmpty()) {
                    anyDhcp = true;
                    out.append(QStringLiteral("   IPv4 Address. . . . . . . . . . . : %1 (Preferred)").arg(currentIp));
                    if (!dhcpSummary.contains(QStringLiteral("%1=%2").arg(currentIface, currentIp)))
                        dhcpSummary.append(QStringLiteral("%1=%2").arg(currentIface, currentIp));
                }
                out.append(QString());
            }
        }
    }

    // 3. Check /proc/net/route for gateways (DHCP routers) on interfaces without lease files
    if (!anyDhcp) {
        QFile routeFile(QStringLiteral("/proc/net/route"));
        if (routeFile.open(QIODevice::ReadOnly)) {
            QTextStream ts(&routeFile); ts.readLine(); // header
            while (!ts.atEnd()) {
                QStringList cols = ts.readLine().trimmed().split('\t');
                if (cols.size() >= 11 && cols[2].toUInt(nullptr, 16) != 0) {
                    out.append(QStringLiteral("   Interface: %1 (via DHCP — inferred from default route)").arg(cols[0]));
                    out.append(QStringLiteral("   Default Gateway . . . . . . . . . : %1").arg(ipToStr(cols[2].toUInt(nullptr, 16))));
                    out.append(QStringLiteral("   DHCP Enabled. . . . . . . . . . . : Likely Yes"));
                    out.append(QString());
                }
            }
        }
    }

    if (!anyDhcp && out.size() <= 4)
        out.append(QStringLiteral("   No DHCP lease information found (static IP or managed externally)"));
#endif // !PLATFORM_IOS
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = dhcpSummary.isEmpty() ? QStringLiteral("No DHCP leases found (static IP?)")
                 : QStringLiteral("DHCP: %1").arg(dhcpSummary.join(QStringLiteral(", ")));
    r.durationMs = t.elapsed();
    return r;
}

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// G2 闁?Routing Table (route print format)
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
DiagnosticResult routingTable(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

    out.append(QString());
    out.append(QStringLiteral("==========================================================================="));
    out.append(QStringLiteral("Interface List"));
    out.append(QStringLiteral("==========================================================================="));

#ifdef _WIN32
    // Get interface list
    ULONG bufLen = 15000;
    QByteArray buf(bufLen, '\0');
    PIP_ADAPTER_ADDRESSES adapters = (PIP_ADAPTER_ADDRESSES)buf.data();
    if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, adapters, &bufLen) == NO_ERROR) {
        for (auto* a = adapters; a; a = a->Next)
            out.append(QStringLiteral("  %1...%2 ......%3")
                .arg(a->Ipv6IfIndex, 4).arg(macToStr(a->PhysicalAddress))
                .arg(QString::fromWCharArray(a->FriendlyName)));
    }

    out.append(QStringLiteral("==========================================================================="));
    out.append(QString());
    out.append(QStringLiteral("IPv4 Route Table"));
    out.append(QStringLiteral("==========================================================================="));
    out.append(QStringLiteral("Active Routes:"));
    static const QVector<DiagnosticFormatter::ColSpec> kRouteCols = {
        {"Network Destination", 22, true},
        {"Netmask",             16, true},
        {"Gateway",             16, true},
        {"Interface",           10, false},
        {"Metric",               6, true},
    };
    QList<QStringList> routeRows;

    PMIB_IPFORWARD_TABLE2 ft = nullptr;
    if (GetIpForwardTable2(AF_INET, &ft) == NO_ERROR) {
        for (ULONG i = 0; i < ft->NumEntries; i++) {
            auto& row = ft->Table[i];
            struct in_addr dest = row.DestinationPrefix.Prefix.Ipv4.sin_addr;
            struct in_addr gw = row.NextHop.Ipv4.sin_addr;
            // Convert prefix length to netmask
            int prefixLen = row.DestinationPrefix.PrefixLength;
            uint32_t maskVal = (prefixLen == 0) ? 0 : (~0u << (32 - prefixLen));
            struct in_addr mask; mask.S_un.S_addr = htonl(maskVal);
            // Interface index 闁?name
            QString ifName = QString::number(row.InterfaceIndex);
            MIB_IF_ROW2 ifRow; ZeroMemory(&ifRow, sizeof(ifRow));
            ifRow.InterfaceIndex = row.InterfaceIndex;
            if (GetIfEntry2(&ifRow) == NO_ERROR) {
                ifName = QString::fromWCharArray(ifRow.Alias);
            }
            routeRows.append({
                ip4ToStr(dest),
                ip4ToStr(mask),
                ip4ToStr(gw),
                ifName.left(9),
                QString::number(row.SitePrefixLength)});
        }
        FreeMibTable(ft);
    }
    if (!routeRows.isEmpty())
        out.append(DiagnosticFormatter::formatTable(kRouteCols, routeRows));
#else
    // Linux: parse /proc/net/route
    out.append(QStringLiteral("==========================================================================="));
    out.append(QString());
    out.append(QStringLiteral("IPv4 Route Table"));
    out.append(QStringLiteral("==========================================================================="));
    out.append(QStringLiteral("Active Routes:"));
    static const QVector<DiagnosticFormatter::ColSpec> kRouteCols = {
        {"Network Destination", 22, true},
        {"Netmask",             16, true},
        {"Gateway",             16, true},
        {"Interface",           10, false},
        {"Metric",               6, true},
    };
    QList<QStringList> routeRows;

#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID) && !defined(__APPLE__)
    QFile routeFile(QStringLiteral("/proc/net/route"));
    if (routeFile.open(QIODevice::ReadOnly)) {
        QTextStream ts(&routeFile);
        ts.readLine(); // skip header
        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            if (line.isEmpty()) continue;
            QStringList cols = line.split('\t');
            if (cols.size() >= 11) {
                QString ifName = cols[0];
                bool ok;
                uint32_t dest = cols[1].toUInt(&ok, 16);
                uint32_t gw   = cols[2].toUInt(&ok, 16);
                uint32_t mask = cols[7].toUInt(&ok, 16);
                uint32_t metric = cols[6].toUInt(&ok, 16);

                routeRows.append({ipToStr(dest), ipToStr(mask),
                    gw ? ipToStr(gw) : QStringLiteral("On-link"),
                    ifName.left(9), QString::number(metric)});
            }
        }
    }
    if (!routeRows.isEmpty())
        out.append(DiagnosticFormatter::formatTable(kRouteCols, routeRows));
#endif // !PLATFORM_IOS
#ifdef PLATFORM_IOS
    out.append(QStringLiteral("  [iOS] Routing table: unavailable (restricted by Apple)"));
#endif
#endif

    out.append(QStringLiteral("==========================================================================="));
    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = QStringLiteral("Routing table collected");
    r.durationMs = t.elapsed();
    return r;
}

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// G2 闁?ARP Table (arp -a format)
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
DiagnosticResult arpTable(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

    out.append(QString());

#ifdef _WIN32
    PMIB_IPNET_TABLE2 table = nullptr;
    if (GetIpNetTable2(AF_INET, &table) == NO_ERROR && table) {
        out.append(QStringLiteral("Interface: (all)"));
        out.append(QStringLiteral("  Internet Address         Physical Address        Type"));
        out.append(QStringLiteral("  -----------------------  ----------------------  --------"));
        for (ULONG i = 0; i < table->NumEntries; i++) {
            auto& row = table->Table[i];
            struct in_addr ip = row.Address.Ipv4.sin_addr;
            out.append(QStringLiteral("  %1  %2  %3")
                .arg(ip4ToStr(ip), -24)
                .arg(macToStr((const unsigned char*)&row.PhysicalAddress), -23)
                .arg(row.State == NlnsReachable ? "dynamic" : "static"));
        }
        FreeMibTable(table);
    }
#else
    // Linux: parse /proc/net/arp
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID) && !defined(__APPLE__)
    QFile arpFile(QStringLiteral("/proc/net/arp"));
    if (arpFile.open(QIODevice::ReadOnly)) {
        QTextStream ts(&arpFile);
        QString header = ts.readLine(); // skip header
        out.append(QStringLiteral("Interface: (all)"));
        static const QVector<DiagnosticFormatter::ColSpec> kArpCols = {
            {"Internet Address",  24, true},
            {"Physical Address",  23, true},
            {"Type",               0, false},
        };
        QList<QStringList> arpRows;

        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            if (line.isEmpty()) continue;
            QStringList cols = line.split(QRegularExpression("\\s+"));
            if (cols.size() >= 5) {
                QString ip = cols[0];
                QString mac = cols[3];
                QString type = (cols[2] == "0x2") ? "static" : "dynamic";
                arpRows.append({ip, mac, type});
            }
        }
        out.append(QStringLiteral("  ") + DiagnosticFormatter::formatTable(kArpCols, arpRows).join(QStringLiteral("\n  ")));
    } else {
        out.append(QStringLiteral("  (ARP table not available)"));
    }
#else  // PLATFORM_IOS
    out.append(QStringLiteral("  [iOS] ARP table: unavailable (no public link-layer API)"));
#endif // !PLATFORM_IOS
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
#ifdef PLATFORM_IOS
    r.status = DiagStatus::Skipped;
    r.summary = QStringLiteral("Unavailable on iOS (no public ARP API)");
#else
    r.status = DiagStatus::Pass;
    r.summary = QStringLiteral("ARP table collected");
#endif
    r.durationMs = t.elapsed();
    return r;
}

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// Remaining G2/G3 stubs with native implementations
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?

DiagnosticResult networkProfile(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Network Profile Information:"));
    out.append(QString());

#ifdef _WIN32
    out.append(QStringLiteral("  (use netsh advfirewall / Get-NetConnectionProfile for details)"));
#else
    char hostname[256] = {};
    gethostname(hostname, sizeof(hostname));
    out.append(QStringLiteral("  Hostname: %1").arg(QString::fromLatin1(hostname)));

    // Check common network profiles via /proc/sys/net
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID) && !defined(__APPLE__)
    QFile fwd(QStringLiteral("/proc/sys/net/ipv4/ip_forward"));
    if (fwd.open(QIODevice::ReadOnly))
        out.append(QStringLiteral("  IP Forwarding: %1").arg(QString::fromLatin1(fwd.readAll().trimmed()) == "1" ? "Enabled" : "Disabled"));
#endif // !PLATFORM_IOS
#ifdef PLATFORM_IOS
    out.append(QStringLiteral("  [iOS] IP forwarding: unavailable (restricted by Apple)"));
#endif
#endif // _WIN32

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = QStringLiteral("Network profile collected");
    r.durationMs = 0;
    return r;
}

DiagnosticResult tcpSettings(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("TCP/IP Settings (table mode):"));
    out.append(QString());

#ifdef _WIN32
    out.append(QStringLiteral("  (use netsh int tcp show global for details)"));
#else
    static const QVector<DiagnosticFormatter::ColSpec> kTcpCols = {
        {"Setting", 20, false},
        {"Value",    0, false},
    };
    QList<QStringList> tcpRows;
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID) && !defined(__APPLE__)
    auto readSys = [&](const QString& path, const QString& label) {
        QFile f(path);
        QString val = f.open(QIODevice::ReadOnly) ? QString::fromLatin1(f.readAll().trimmed()) : QStringLiteral("-");
        tcpRows.append({label, val});
    };
    readSys(QStringLiteral("/proc/sys/net/ipv4/tcp_congestion_control"), QStringLiteral("Congestion Control"));
    readSys(QStringLiteral("/proc/sys/net/ipv4/tcp_window_scaling"), QStringLiteral("Window Scaling"));
    readSys(QStringLiteral("/proc/sys/net/ipv4/tcp_timestamps"), QStringLiteral("Timestamps"));
    readSys(QStringLiteral("/proc/sys/net/ipv4/tcp_sack"), QStringLiteral("Selective ACK"));
    readSys(QStringLiteral("/proc/sys/net/ipv4/tcp_fastopen"), QStringLiteral("TCP Fast Open"));
#else
    out.append(QStringLiteral("  [iOS] TCP settings: unavailable (restricted by Apple)"));
#endif
    if (!tcpRows.isEmpty())
        out.append(DiagnosticFormatter::formatTable(kTcpCols, tcpRows));
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
#ifdef PLATFORM_IOS
    r.status = DiagStatus::Skipped;
    r.summary = QStringLiteral("Unavailable on iOS (kernel sysctls restricted)");
#else
    r.status = DiagStatus::Pass;
    r.summary = QStringLiteral("TCP settings collected");
#endif
    r.durationMs = 0;
    return r;
}

DiagnosticResult defaultGateway(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

    out.append(QString());
    out.append(QStringLiteral("Default Gateway:"));
    out.append(QString());

    QString defaultGw = QStringLiteral("Not found");
#ifdef _WIN32
    PMIB_IPFORWARD_TABLE2 ft = nullptr;
    if (GetIpForwardTable2(AF_INET, &ft) == NO_ERROR) {
        for (ULONG i = 0; i < ft->NumEntries; i++) {
            if (ft->Table[i].DestinationPrefix.PrefixLength == 0) {
                struct in_addr gw = ft->Table[i].NextHop.Ipv4.sin_addr;
                defaultGw = ip4ToStr(gw);
                out.append(QStringLiteral("  Default Gateway: %1").arg(defaultGw));
                break;
            }
        }
        FreeMibTable(ft);
    }
#else
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID) && !defined(__APPLE__)
    QFile routeFile(QStringLiteral("/proc/net/route"));
    if (routeFile.open(QIODevice::ReadOnly)) {
        QTextStream ts(&routeFile);
        ts.readLine();
        while (!ts.atEnd()) {
            QStringList cols = ts.readLine().trimmed().split('\t');
            if (cols.size() >= 11 && cols[1].toUInt(nullptr, 16) == 0) {
                uint32_t gw = cols[2].toUInt(nullptr, 16);
                if (gw != 0) {
                    defaultGw = ipToStr(gw);
                    out.append(QStringLiteral("  Default Gateway . . . . . . . . . : %1").arg(defaultGw));
                }
            }
        }
    }
#else  // PLATFORM_IOS
    out.append(QStringLiteral("  [iOS] Default gateway: unavailable (restricted by Apple)"));
#endif // !PLATFORM_IOS
#endif
    if (defaultGw == QStringLiteral("Not found"))
        out.append(QStringLiteral("  No default gateway configured"));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
#if defined(__APPLE__) && !defined(PLATFORM_IOS)
    r.status = DiagStatus::Skipped;
    r.summary = QStringLiteral("Unavailable on macOS (/proc/net/route not available; use netstat -rn)");
#else
    r.status = (defaultGw != QStringLiteral("Not found")) ? DiagStatus::Pass : DiagStatus::Warning;
    r.summary = (defaultGw != QStringLiteral("Not found"))
        ? QStringLiteral("Default gateway: %1").arg(defaultGw)
        : QStringLiteral("No default gateway");
#endif
    r.durationMs = t.elapsed();
    return r;
}

DiagnosticResult proxySettings(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Proxy Configuration (table mode):"));
    out.append(QString());

    static const QVector<DiagnosticFormatter::ColSpec> kProxyCols = {
        {"Variable", 16, false},
        {"Value",     0, false},
    };
    QList<QStringList> proxyRows;

#ifdef _WIN32
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG cfg = {};
    if (WinHttpGetIEProxyConfigForCurrentUser(&cfg)) {
        if (cfg.lpszProxy) proxyRows.append({QStringLiteral("HTTP Proxy"), QString::fromWCharArray(cfg.lpszProxy)});
        if (cfg.lpszProxyBypass) proxyRows.append({QStringLiteral("Bypass"), QString::fromWCharArray(cfg.lpszProxyBypass)});
        GlobalFree(cfg.lpszProxy); GlobalFree(cfg.lpszProxyBypass);
    }
#else
    const char* vars[] = {"HTTP_PROXY","HTTPS_PROXY","FTP_PROXY","NO_PROXY","http_proxy","https_proxy","no_proxy"};
    for (auto* v : vars) {
        const char* val = getenv(v);
        if (val && val[0])
            proxyRows.append({QString::fromLatin1(v), QString::fromLatin1(val)});
    }
#endif
    if (!proxyRows.isEmpty())
        out.append(DiagnosticFormatter::formatTable(kProxyCols, proxyRows));
    else
        out.append(QStringLiteral("  No proxy configured"));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Info;
    r.summary = QStringLiteral("Proxy settings collected");
    r.durationMs = 0;
    return r;
}

// 闁冲厜鍋撻柍鍏夊亾 G3 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾

DiagnosticResult netskopeStatus(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Security Proxy Status:"));
    out.append(QString());

    bool found = false;
#ifdef _WIN32
    // Check for nsproxy.exe process
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe; pe.dwSize = sizeof(pe);
        if (Process32FirstW(hSnap, &pe)) {
            do {
                QString name = QString::fromWCharArray(pe.szExeFile);
                if (name.contains("nsproxy", Qt::CaseInsensitive) || name.contains("zsproxy", Qt::CaseInsensitive) ||
                    name.contains("zscaler", Qt::CaseInsensitive) || name.contains("netskope", Qt::CaseInsensitive)) {
                    out.append(QStringLiteral("  Found: %1 (PID %2)").arg(name).arg(pe.th32ProcessID));
                    found = true;
                }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }
#else
    // Check /proc for nsproxy/netskope/zscaler processes
    QDir procDir(QStringLiteral("/proc"));
    for (const auto& fi : procDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok; fi.fileName().toInt(&ok);
        if (!ok) continue; // not a PID directory
        QFile cmdLine(fi.absoluteFilePath() + "/comm");
        if (cmdLine.open(QIODevice::ReadOnly)) {
            QString comm = QString::fromLatin1(cmdLine.readAll().trimmed());
            if (comm.contains("nsproxy", Qt::CaseInsensitive) || comm.contains("zscaler", Qt::CaseInsensitive) ||
                comm.contains("netskope", Qt::CaseInsensitive) || comm.contains("zsproxy", Qt::CaseInsensitive)) {
                out.append(QStringLiteral("  Found: %1 (PID %2)").arg(comm, fi.fileName()));
                found = true;
            }
        }
    }
    if (!found) out.append(QStringLiteral("  No security proxy process detected"));
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = found ? DiagStatus::Pass : DiagStatus::Info;
    r.summary = found ? QStringLiteral("Security proxy detected") : QStringLiteral("No security proxy detected");
    r.durationMs = 0;
    return r;
}

DiagnosticResult dnsServers(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out, dnsList;
    out.append(QString());
    out.append(QStringLiteral("DNS Server Configuration (table mode):"));
    out.append(QString());

    static const QVector<DiagnosticFormatter::ColSpec> kDnsCols = {
        {"Source",   20, false},
        {"DNS Server", 0, false},
    };
    QList<QStringList> dnsRows;

#ifdef _WIN32
    ULONG bufLen = 15000;
    QByteArray buf(bufLen, '\0');
    PIP_ADAPTER_ADDRESSES adapters = (PIP_ADAPTER_ADDRESSES)buf.data();
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_UNICAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_ANYCAST, nullptr, adapters, &bufLen) == NO_ERROR) {
        for (auto* a = adapters; a; a = a->Next) {
            QString ifName = QString::fromWCharArray(a->FriendlyName);
            for (auto* dns = a->FirstDnsServerAddress; dns; dns = dns->Next) {
                char ip[64]; DWORD ipLen = sizeof(ip);
                WSAAddressToStringA(dns->Address.lpSockaddr, dns->Address.iSockaddrLength, nullptr, ip, &ipLen);
                QString ipStr = QString::fromLatin1(ip);
                dnsRows.append({ifName, ipStr});
                if (!dnsList.contains(ipStr)) dnsList.append(ipStr);
            }
        }
    }
#else
#ifdef PLATFORM_IOS
    // iOS: no /etc/resolv.conf 闁?use res_ninit
    struct __res_state res; memset(&res, 0, sizeof(res));
    if (res_ninit(&res) == 0) {
        for (int i = 0; i < res.nscount; i++) {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &res.nsaddr_list[i].sin_addr, ip, sizeof(ip));
            dnsRows.append({QStringLiteral("System DNS"), QString::fromLatin1(ip)});
            dnsList.append(QString::fromLatin1(ip));
        }
        res_nclose(&res);
    }
#else
    // Read /etc/resolv.conf
    QFile resolv(QStringLiteral("/etc/resolv.conf"));
    if (resolv.open(QIODevice::ReadOnly)) {
        QTextStream ts(&resolv);
        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            if (line.startsWith("nameserver ")) {
                QString ns = line.mid(11);
                dnsRows.append({QStringLiteral("resolv.conf"), ns});
                if (!dnsList.contains(ns)) dnsList.append(ns);
            }
            else if (line.startsWith("search "))
                dnsRows.append({QStringLiteral("search domains"), line.mid(7)});
        }
    }
    // Also check systemd-resolved stub
    QFile stub(QStringLiteral("/run/systemd/resolve/resolv.conf"));
    if (stub.open(QIODevice::ReadOnly)) {
        dnsRows.append({QStringLiteral("systemd-resolved"), QStringLiteral("(stub resolver active)")});
    }
#endif
#endif

    if (!dnsRows.isEmpty())
        out.append(DiagnosticFormatter::formatTable(kDnsCols, dnsRows));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = dnsList.isEmpty() ? QStringLiteral("No DNS servers found")
                 : QStringLiteral("DNS: %1").arg(dnsList.join(QStringLiteral(", ")));
    r.durationMs = 0;
    return r;
}

DiagnosticResult dnsCache(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;
    int cacheEntries = 0;
    bool hasCache = false;

    out.append(QString());

#ifdef _WIN32
    out.append(QStringLiteral("Windows IP Configuration"));
    out.append(QString());
    out.append(QStringLiteral("DNS Client Cache (ipconfig /displaydns format)"));
    out.append(QStringLiteral("=============================================="));
    out.append(QString());
    out.append(QStringLiteral("(Use 'ipconfig /displaydns' for full cache contents)"));
    out.append(QStringLiteral("To flush: ipconfig /flushdns"));
#else
    out.append(QStringLiteral("DNS Cache Information"));
    out.append(QString());
    // 闁冲厜鍋撻柍鍏夊亾 Try systemd-resolved cache (most common on modern Linux) 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾
    QFile cache(QStringLiteral("/run/systemd/resolve/cache"));
    if (cache.open(QIODevice::ReadOnly)) {
        QByteArray data = cache.readAll();
        hasCache = true;
        out.append(QStringLiteral("systemd-resolved DNS Cache"));
        out.append(QStringLiteral("=============================================="));
        out.append(QString());
        if (data.size() > 0) {
            // Parse cache entries: each entry is separated by blank line
            // Format: "example.com IN A 93.184.216.34" or similar
            QString text = QString::fromLatin1(data);
            QStringList entries = text.split('\n');
            for (const auto& line : entries) {
                QString trimmed = line.trimmed();
                if (trimmed.isEmpty()) {
                    out.append(QString());
                    continue;
                }
                // Parse: "hostname IN TYPE value" or "hostname IN TYPE ttl value"
                QStringList parts = trimmed.split(' ');
                if (parts.size() >= 4 && parts[1] == "IN") {
                    QString name = parts[0];
                    QString type = parts[2];
                    // Skip "IN" marker, extract TTL if present
                    QString dataPart;
                    int ttl = 0;
                    bool ok = false;
                    if (parts.size() >= 5) {
                        int val = parts[3].toInt(&ok);
                        if (ok && val > 0 && parts.size() >= 6) {
                            ttl = val;
                            dataPart = parts.mid(4).join(' ');
                        } else {
                            dataPart = parts.mid(3).join(' ');
                        }
                    }
                    // Show in ipconfig /displaydns style
                    cacheEntries++;
                    out.append(QStringLiteral("    %1").arg(name));
                    out.append(QStringLiteral("    ----------------------------------------"));
                    out.append(QStringLiteral("    Record Name . . . . . : %1").arg(name));
                    out.append(QStringLiteral("    Record Type . . . . . : %1").arg(type));
                    if (ttl > 0)
                        out.append(QStringLiteral("    Time To Live  . . . . : %1").arg(ttl));
                    out.append(QStringLiteral("    Data . . . . . . . . : %1").arg(dataPart));
                } else {
                    // Unparsed line 闁?show as-is
                    out.append(QStringLiteral("    %1").arg(trimmed));
                }
            }
        } else {
            out.append(QStringLiteral("    (cache is empty)"));
        }
    } else {
        // 闁冲厜鍋撻柍鍏夊亾 No systemd-resolved 闁?check and show resolution setup 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋?
        out.append(QStringLiteral("DNS Resolution Configuration"));
        out.append(QStringLiteral("=============================================="));
        out.append(QString());

        // Check for nscd
        if (QFile::exists(QStringLiteral("/var/db/nscd/hosts")))
            out.append(QStringLiteral("    nscd: active (hosts cache at /var/db/nscd/hosts)"));
        else if (QFile::exists(QStringLiteral("/var/cache/nscd/hosts")))
            out.append(QStringLiteral("    nscd: active (hosts cache at /var/cache/nscd/hosts)"));

        // Check for dnsmasq
        if (QFile::exists(QStringLiteral("/var/lib/misc/dnsmasq.leases")))
            out.append(QStringLiteral("    dnsmasq: active (leases at /var/lib/misc/dnsmasq.leases)"));

        // Show resolv.conf as the "current resolver" info
        QFile resolv(QStringLiteral("/etc/resolv.conf"));
        if (resolv.open(QIODevice::ReadOnly)) {
            QTextStream ts(&resolv);
            while (!ts.atEnd()) {
                QString line = ts.readLine().trimmed();
                if (line.isEmpty() || line.startsWith('#')) continue;
                if (line.startsWith("nameserver "))
                    out.append(QStringLiteral("    Nameserver . . . . . . . . : %1").arg(line.mid(11)));
                else if (line.startsWith("search "))
                    out.append(QStringLiteral("    DNS Suffix Search List. . : %1").arg(line.mid(7)));
                else if (line.startsWith("domain "))
                    out.append(QStringLiteral("    Connection-specific DNS . . : %1").arg(line.mid(7)));
                else if (line.startsWith("options "))
                    out.append(QStringLiteral("    Options . . . . . . . . . : %1").arg(line.mid(8)));
            }
        }

        // Show hosts file summary
        QFile hosts(QStringLiteral("/etc/hosts"));
        int hostEntryCount = 0;
        if (hosts.open(QIODevice::ReadOnly)) {
            QTextStream ts(&hosts);
            while (!ts.atEnd()) {
                QString line = ts.readLine().trimmed();
                if (!line.isEmpty() && !line.startsWith('#') && line.contains(' '))
                    hostEntryCount++;
            }
        }
        if (hostEntryCount > 0)
            out.append(QStringLiteral("    /etc/hosts entries . . . . : %1 static mappings").arg(hostEntryCount));
    }
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = hasCache ? DiagStatus::Pass : DiagStatus::Info;
    if (hasCache)
        r.summary = QStringLiteral("Cache active — %1 cached DNS entries").arg(cacheEntries);
    else
        r.summary = QStringLiteral("No local DNS cache detected");
    r.durationMs = (int)t.elapsed();
    return r;
}

DiagnosticResult dnsPollution(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

    out.append(QString());
    out.append(QStringLiteral("DNS Pollution / Hijacking Check"));
    out.append(QStringLiteral("================================="));
    out.append(QString());
    out.append(QStringLiteral("Tests whether non-existent domains resolve to IP addresses."));
    out.append(QStringLiteral("If they do, your DNS provider is redirecting NXDOMAIN responses"));
    out.append(QStringLiteral("(DNS hijacking / DNS pollution). A clean resolver returns NXDOMAIN."));
    out.append(QString());

    // Show current DNS server
    QFile resolv(QStringLiteral("/etc/resolv.conf"));
    if (resolv.open(QIODevice::ReadOnly)) {
        QTextStream ts(&resolv);
        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            if (line.startsWith("nameserver ")) {
                out.append(QStringLiteral("DNS Server: %1").arg(line.mid(11)));
                break;
            }
        }
    }
    out.append(QString());
    out.append(QStringLiteral("  %1  %2  %3")
        .arg(QStringLiteral("Test Domain"), -40)
        .arg(QStringLiteral("Result"), -16)
        .arg(QStringLiteral("Response")));
    out.append(QStringLiteral("  %1  %2  %3")
        .arg(QString(40, '-'))
        .arg(QString(16, '-'))
        .arg(QString(20, '-')));

    struct { const char* domain; } testCases[] = {
        {"thisdomainshouldnotexist12345.com"},
        {"nonexistent-test-domain-98765.org"},
        {"definitely-not-real-domain-42.net"},
    };

    int resolved = 0, clean = 0, timedOut = 0;
    QStringList hijackIPs;
    for (auto& tc : testCases) {
        QElapsedTimer probe; probe.start();
        QString ip = DnsResolver::instance().resolve(tc.domain, 4000);
        int elapsed = static_cast<int>(probe.elapsed());
        if (!ip.isEmpty()) {
            out.append(QStringLiteral("  %1  %2  %3")
                .arg(tc.domain, -40).arg(QStringLiteral("RESOLVED"), -16).arg(QStringLiteral("%1 (%2 ms)").arg(ip).arg(elapsed)));
            resolved++;
            if (!hijackIPs.contains(ip)) hijackIPs.append(ip);
        } else if (elapsed >= 4000) {
            out.append(QStringLiteral("  %1  %2  %3")
                .arg(tc.domain, -40).arg(QStringLiteral("TIMEOUT"), -16).arg(QStringLiteral("%1 ms").arg(elapsed)));
            timedOut++;
        } else {
            out.append(QStringLiteral("  %1  %2  %3")
                .arg(tc.domain, -40).arg(QStringLiteral("NXDOMAIN"), -16).arg(QStringLiteral("%1 ms").arg(elapsed)));
            clean++;
        }
    }

    out.append(QString());
    out.append(QStringLiteral("------------------------------------------------------------------"));
    out.append(QStringLiteral("Results: %1 resolved, %2 clean, %3 timed out")
        .arg(resolved).arg(clean).arg(timedOut));
    if (resolved > 0) {
        out.append(QStringLiteral("Verdict: DNS HIJACKING DETECTED — non-existent domains redirected to:"));
        for (const auto& ip : hijackIPs) out.append(QStringLiteral("  • %1").arg(ip));
        out.append(QString());
        out.append(QStringLiteral("This typically means your ISP or DNS provider is intercepting"));
        out.append(QStringLiteral("NXDOMAIN responses and redirecting to a search/advertising page."));
    } else if (timedOut > 0) {
        out.append(QStringLiteral("Verdict: INCONCLUSIVE — %1 probes timed out (DNS may be slow or filtered)").arg(timedOut));
    } else {
        out.append(QStringLiteral("Verdict: DNS CLEAN — no hijacking detected"));
    }

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = resolved > 0 ? DiagStatus::Warning : (timedOut > 0 ? DiagStatus::Info : DiagStatus::Pass);
    r.summary = resolved > 0 ? QStringLiteral("DNS hijack: %1 IPs").arg(hijackIPs.size())
               : timedOut > 0 ? QStringLiteral("DNS: %1 timeout(s)").arg(timedOut)
               : QStringLiteral("DNS clean");
    r.durationMs = t.elapsed();
    return r;
}

// internetConnectivity() removed 闁?merged into speedTest() (Phase 0 connectivity check)

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// Speed Test 闁?Speedtest.net protocol (Ookla-compatible)
// Mimics "speedtest-cli" output format
// =============================================================================
// Protocol:
//   1. GET /api/js/servers 闁?JSON server list
//   2. TCP ping each candidate 闁?pick lowest latency
//   3. GET {url}/download?size=N 闁?measure download throughput
//   4. POST {url}/upload 闁?measure upload throughput
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// Convert host (name or IP) to sockaddr_in 闁?returns true on success

// 闁冲厜鍋撻柍鍏夊亾 Simple HTTP GET via raw socket (no Qt event loop needed) 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾
static QByteArray httpGet(const QString& host, int port, const QString& path, int timeoutMs, int maxBytes) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return {};
    struct sockaddr_in addr;
    if (!hostToAddr(host, port, addr)) { closeSocket(sock); return {}; }

#ifdef _WIN32
    u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
    ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
    struct timeval tv = {timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    if (select(sock + 1, nullptr, &fdset, nullptr, &tv) <= 0) { closeSocket(sock); return {}; }
    int err = 0; socklen_t len = sizeof(err);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
    if (err != 0) { closeSocket(sock); return {}; }

    // Send HTTP request (loop handles partial sends, EAGAIN-safe)
    QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: NetDiagnostics/1.0\r\nAccept: */*\r\nConnection: close\r\n\r\n")
        .arg(path, host).toUtf8();
    int sent = 0;
    while (sent < req.size()) {
        auto n = ::send(sock, req.constData() + sent, req.size() - sent, 0);
        if (n < 0) {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); struct timeval wfTv = {1,0}; select(sock+1, nullptr, &wf, nullptr, &wfTv); continue; }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); struct timeval wfTv = {1,0}; select(sock+1, nullptr, &wf, nullptr, &wfTv); continue; }
#endif
            break;
        }
        if (n == 0) break;
        sent += n;
    }

    // Read response with wall-clock timeout
    QByteArray response; char buf[8192];
    QElapsedTimer recvTimer; recvTimer.start();
    while (response.size() < maxBytes) {
        FD_ZERO(&fdset); FD_SET(sock, &fdset);
        tv = {timeoutMs / 1000, (timeoutMs % 1000) * 1000};
        if (select(sock + 1, &fdset, nullptr, nullptr, &tv) <= 0) break;
        ssize_t n = recv(sock, buf, sizeof(buf), 0);
        if (n <= 0) break;
        response.append(buf, (int)n);
        // Wall-clock guard: abort if total recv time exceeds 30 s
        if (recvTimer.elapsed() > 30000) break;
    }
    closeSocket(sock);
    return response;
}

// 闁冲厜鍋撻柍鍏夊亾 HTTP download with throughput measurement 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋?
struct SpeedResult { double mbps; int bytes; int durationMs; bool ok; };
static SpeedResult httpDownload(const QString& urlStr, int targetBytes, int timeoutMs) {
    SpeedResult r = {0, 0, 0, false};
    // Parse URL 闁?host, port, path
    QString u = urlStr;
    if (!u.startsWith("http://")) return r;
    u = u.mid(7); // strip "http://"
    auto slash = u.indexOf('/');
    QString hostPort = (slash > 0) ? u.left(slash) : u;
    QString path = (slash > 0) ? u.mid(slash) : "/";

    QString host = hostPort; int port = 80;
    auto colon = hostPort.lastIndexOf(':');
    if (colon > 0) { host = hostPort.left(colon); port = hostPort.mid(colon + 1).toInt(); }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return r;
    struct sockaddr_in addr;
    if (!hostToAddr(host, port, addr)) { closeSocket(sock); return r; }

#ifdef _WIN32
    u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
    QElapsedTimer t; t.start();
    ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
    struct timeval tv = {3, 0};
    if (select(sock + 1, nullptr, &fdset, nullptr, &tv) <= 0) { closeSocket(sock); return r; }
    int err = 0; socklen_t len = sizeof(err);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
    if (err != 0) { closeSocket(sock); return r; }

    // Send HTTP GET (loop handles partial sends, EAGAIN-safe)
    QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: NetDiagnostics/1.0\r\nConnection: close\r\n\r\n")
        .arg(path, host).toUtf8();
    int reqSent = 0;
    while (reqSent < req.size()) {
        auto n = ::send(sock, req.constData() + reqSent, req.size() - reqSent, 0);
        if (n < 0) {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); struct timeval wfTv = {1,0}; select(sock+1, nullptr, &wf, nullptr, &wfTv); continue; }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); struct timeval wfTv = {1,0}; select(sock+1, nullptr, &wf, nullptr, &wfTv); continue; }
#endif
            break;
        }
        if (n == 0) break;
        reqSent += n;
    }

    // Read with timing 闁?measure throughput (wall-clock guarded)
    qint64 startNs = t.nsecsElapsed();
    QByteArray body;
    bool headersDone = false;
    char buf[32768];
    QElapsedTimer recvGuard; recvGuard.start();
    while (body.size() < targetBytes + 65536) {
        FD_ZERO(&fdset); FD_SET(sock, &fdset);
        tv = {timeoutMs / 1000, (timeoutMs % 1000) * 1000};
        if (select(sock + 1, &fdset, nullptr, nullptr, &tv) <= 0) break;
        ssize_t n = recv(sock, buf, sizeof(buf), 0);
        if (n <= 0) break;
        // Wall-clock guard: abort if total recv time exceeds 60 s
        if (recvGuard.elapsed() > 60000) break;
        if (!headersDone) {
            body.append(buf, (int)n);
            auto hdrEnd = body.indexOf("\r\n\r\n");
            if (hdrEnd >= 0) {
                body = body.mid(hdrEnd + 4);
                headersDone = true;
                startNs = t.nsecsElapsed(); // reset timer to body start
            }
        } else {
            body.append(buf, (int)n);
        }
    }
    closeSocket(sock);

    qint64 elapsedNs = t.nsecsElapsed() - startNs;
    if (elapsedNs <= 0) elapsedNs = 1;
    r.bytes = static_cast<int>(body.size());
    r.durationMs = (int)(elapsedNs / 1000000);
    if (r.bytes > 0 && r.durationMs > 0) {
        double bits = r.bytes * 8.0;
        double secs = r.durationMs / 1000.0;
        r.mbps = bits / secs / 1000000.0;
        r.ok = true;
    }
    return r;
}

// 闁冲厜鍋撻柍鍏夊亾 TCP ping (simple connect RTT) 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋?
static int tcpPingMs(const QString& host, int port) {
    QElapsedTimer t; t.start();
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    struct sockaddr_in addr;
    if (!hostToAddr(host, port, addr)) { closeSocket(sock); return -1; }
#ifdef _WIN32
    u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
    ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
    struct timeval tv = {2, 0};
    int sel = select(sock + 1, nullptr, &fdset, nullptr, &tv);
    int ms = static_cast<int>(t.elapsed());
    if (sel > 0) { int err = 0; socklen_t len = sizeof(err); getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len); if (err != 0) ms = -1; } else ms = -1;
    closeSocket(sock);
    return ms;
}

// 闁冲厜鍋撻柍鍏夊亾 HTTP latency via tiny file download (speedtest-cli style latency.txt) 闁冲厜鍋撻柍鍏夊亾
// Measures real application-layer RTT: DNS + TCP connect + HTTP request/response
// Much better predictor of download throughput than raw TCP ping.
static int httpLatencyMs(const QString& urlStr, int timeoutMs) {
    QElapsedTimer t; t.start();
    QString u = urlStr;
    if (!u.startsWith("http://")) return -1;
    u = u.mid(7);
    auto slash = u.indexOf('/');
    QString hostPort = (slash > 0) ? u.left(slash) : u;
    QString host = hostPort; int port = 80;
    auto colon = hostPort.lastIndexOf(':');
    if (colon > 0) { host = hostPort.left(colon); port = hostPort.mid(colon + 1).toInt(); }

    // Download latency.txt from server root 闁?speedtest-cli uses the root path
    // regardless of the download/upload URL structure
    QString latPath = QStringLiteral("/latency.txt");
    QByteArray resp = httpGet(host, port, latPath, timeoutMs, 256);
    if (resp.isEmpty()) return -1;

    // Parse HTTP response 闁?extract body after \r\n\r\n header terminator
    auto hdrEnd = resp.indexOf("\r\n\r\n");
    if (hdrEnd < 0) return -1;
    QByteArray body = resp.mid(hdrEnd + 4);
    // latency.txt should contain a small text like "test=...", we just need the time
    if (body.trimmed().isEmpty()) return -1;

    return static_cast<int>(t.elapsed());
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

    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    // Phase 0 闁?Quick connectivity check (TCP to well-known hosts)
    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
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

    struct { const char* host; int port; const char* name; } checkSites[] = {
        {"223.5.5.5", 53, "Alibaba DNS"},
        {"119.29.29.29", 53, "DNSPod DNS"},
        {"baidu.com", 443, "Baidu"},
    };
    int connOk = 0;
    for (auto& cs : checkSites) {
        int p = tcpPingMs(cs.host, cs.port);
        QString status, latency;
        if (p >= 0) { status = QStringLiteral("[OK]"); latency = QStringLiteral("%1 ms").arg(p); connOk++; }
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

    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    // Phase 1 闁?Detect country + load regional servers
    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    SpeedTest st;
    QString country = SpeedTest::detectCountry(3000);
    out.append(QStringLiteral("Detected country: %1").arg(country == "XX" ? "Unknown" : country));

    QVector<SpeedTest::Server> servers = st.serversForCountry(country);
    out.append(QStringLiteral("Loaded %1 servers for region").arg(servers.size()));

    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    // Timeout guard 闁?if we've already spent >25s, skip speed measurement
    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    if (totalTimer.elapsed() > 25000) {
        out.append(QString());
        out.append(QStringLiteral("  (Speed test skipped: connectivity check took too long)"));
        r.rawOutput = out.join('\n'); r.details = r.rawOutput;
        r.status = hasConnectivity ? DiagStatus::Warning : DiagStatus::Fail;
        r.summary = hasConnectivity ? QStringLiteral("Connected — speed test timed out") : QStringLiteral("No internet");
        r.durationMs = totalTimer.elapsed(); return r;
    }

    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    // Phase 2 闁?Select best server by HTTP latency (speedtest-cli style)
    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
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
    int maxServers = qMin(8, (int)servers.size()); // cap at 8 to avoid excessive time
    for (auto& s : servers) {
        if (ranked.size() >= maxServers) break;
        if (totalTimer.elapsed() > 25000) break; // global timeout
        int lat = httpLatencyMs(s.url, 5000);
        if (lat > 0) {
            ranked.append({&s, lat});
        }
    }

    if (ranked.isEmpty()) {
        out.append(QStringLiteral("  (no reachable servers)"));
        out.append(QString());
        r.rawOutput = out.join('\n'); r.details = r.rawOutput;
        r.status = hasConnectivity ? DiagStatus::Warning : DiagStatus::Fail;
        r.summary = hasConnectivity ? QStringLiteral("Connected — no speed test servers reachable")
                                    : QStringLiteral("No internet connectivity");
        r.durationMs = totalTimer.elapsed(); return r;
    }

    // Sort by HTTP latency ascending 闁?fastest first
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
    out.append(QStringLiteral("  Selected: %1 (%2) — %3 ms")
        .arg(best->sponsor, best->name).arg(bestLatency));
    out.append(QString());

    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    // Phase 3 闁?Download test (with server fallback)
    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    out.append(QString());
    out.append(QStringLiteral("--- Download Test ------------------------------------------------"));
    out.append(QString());
    out.append(QStringLiteral("  Server: %1").arg(best->host));
    out.append(QString());
    out.append(QStringLiteral("  %1  %2  %3")
        .arg(QStringLiteral("Size").rightJustified(10, ' '))
        .arg(QStringLiteral("Throughput").leftJustified(16, ' '))
        .arg(QStringLiteral("Time").rightJustified(6, ' ')));
    out.append(QStringLiteral("  %1  %2  %3")
        .arg(QString(10, QChar('-')))
        .arg(QString(16, QChar('-')))
        .arg(QString(6, QChar('-'))));

    // Progressive download sizes (KB): start small, ramp up
    int dlSizes[] = {250, 500, 750, 1000, 1500, 2000, 2500, 3000, 4000, 5000, 7500, 10000, 15000, 20000, 25000};
    QVector<double> dlResults;
    int dlTotalBytes = 0, dlTotalMs = 0;
    int dlServerIdx = 0; // current preferred server index into ranked[]

    for (int sizeKb : dlSizes) {
        if (dlTotalMs > 12000) break; // cap at ~12 seconds

        // Try preferred server first, fall back through ranked list independently
        // per size tier 闁?a server that handles 250KB may choke on 25MB.
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
                    .arg(QStringLiteral("%1 Mbit/s").arg(res.mbps, 0, 'f', 2).leftJustified(16, ' '))
                    .arg(QStringLiteral("%1 ms").arg(res.durationMs).rightJustified(6, ' ')));
                ok = true;
                break;
            }
        }
        if (!ok) {
            out.append(QStringLiteral("  %1  %2  %3")
                .arg(QStringLiteral("%1 KB").arg(sizeKb).rightJustified(10, ' '))
                .arg(QStringLiteral("(timeout)").leftJustified(16, ' '))
                .arg(QStringLiteral("-").rightJustified(6, ' ')));
            // Don't abort 闁?try next size tier even if this one failed
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

    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    // Phase 4 闁?Upload test
    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    out.append(QString());
    out.append(QStringLiteral("--- Upload Test --------------------------------------------------"));
    out.append(QString());
    out.append(QStringLiteral("  %1  %2  %3")
        .arg(QStringLiteral("Size").rightJustified(10, ' '))
        .arg(QStringLiteral("Throughput").leftJustified(16, ' '))
        .arg(QStringLiteral("Time").rightJustified(6, ' ')));
    out.append(QStringLiteral("  %1  %2  %3")
        .arg(QString(10, QChar('-')))
        .arg(QString(16, QChar('-')))
        .arg(QString(6, QChar('-'))));

    // Upload test: POST random data, increasing sizes
    int ulSizes[] = {100, 250, 500, 750, 1000, 1500, 2000, 2500, 3000, 4000};
    QVector<double> ulResults;
    int ulTotalMs = 0;

    for (int sizeKb : ulSizes) {
        if (ulTotalMs > 12000) break;
        int dataSize = sizeKb * 1000;

        // HTTP POST with measured upload
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) continue;
        struct sockaddr_in addr;
        if (!hostToAddr(best->host, best->port, addr)) { closeSocket(sock); continue; }

#ifdef _WIN32
        u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
#else
        int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
        ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
        fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
        struct timeval tv = {3, 0};
        if (select(sock + 1, nullptr, &fdset, nullptr, &tv) <= 0) { closeSocket(sock); continue; }
        int err = 0; socklen_t len = sizeof(err);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
        if (err != 0) { closeSocket(sock); continue; }

        // Generate random data
        QByteArray uploadData(dataSize, 'A');
        for (int i = 0; i < qMin(dataSize, 4096); i++) uploadData[i] = (char)('A' + (rand() % 26));

        // POST request headers
        QByteArray postHeaders = QStringLiteral("POST /upload HTTP/1.0\r\nHost: %1\r\nContent-Type: application/octet-stream\r\nContent-Length: %2\r\nConnection: close\r\n\r\n")
            .arg(best->host).arg(dataSize).toUtf8();

        QElapsedTimer ulTimer; ulTimer.start();
        // Send POST headers (EAGAIN-safe: select() for writability on stall)
        int hdrSent = 0;
        while (hdrSent < postHeaders.size()) {
            auto n = ::send(sock, postHeaders.constData() + hdrSent, postHeaders.size() - hdrSent, 0);
            if (n < 0) {
#ifdef _WIN32
                if (WSAGetLastError() == WSAEWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); timeval hdrTv={1,0}; select(sock+1,nullptr,&wf,nullptr,&hdrTv); continue; }
#else
                if (errno == EAGAIN || errno == EWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); timeval hdrTv={1,0}; select(sock+1,nullptr,&wf,nullptr,&hdrTv); continue; }
#endif
                break;
            }
            if (n == 0) break;
            hdrSent += n;
        }
        // Send body in chunks (EAGAIN-safe: select() for writability on stall)
        // Includes wall-clock guard so outer ulTotalMs check can fire
        int sent = 0; const char* dp = uploadData.constData();
        QElapsedTimer sendGuard; sendGuard.start();
        while (sent < dataSize) {
            int chunk = qMin(dataSize - sent, 32768);
            auto n = ::send(sock, dp + sent, chunk, 0);
            if (n < 0) {
#ifdef _WIN32
                if (WSAGetLastError() == WSAEWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); timeval tv2={2,0}; select(sock+1,nullptr,&wf,nullptr,&tv2); if (sendGuard.elapsed() > 10000) break; continue; }
#else
                if (errno == EAGAIN || errno == EWOULDBLOCK) { fd_set wf; FD_ZERO(&wf); FD_SET(sock, &wf); timeval tv2={2,0}; select(sock+1,nullptr,&wf,nullptr,&tv2); if (sendGuard.elapsed() > 10000) break; continue; }
#endif
                break;
            }
            if (n == 0) break;
            sent += n;
        }
        // Read response with proper error checking
        char buf[4096];
        FD_ZERO(&fdset); FD_SET(sock, &fdset);
        tv = {5, 0};
        int selRet = select(sock + 1, &fdset, nullptr, nullptr, &tv);
        if (selRet > 0 && FD_ISSET(sock, &fdset)) {
            recv(sock, buf, sizeof(buf), 0);
        }
        int ulMs = static_cast<int>(ulTimer.elapsed());
        closeSocket(sock);

        ulTotalMs += ulMs;
        double mbps = (sent > 0 && ulMs > 0) ? (sent * 8.0 / (ulMs / 1000.0) / 1000000.0) : 0;
        if (mbps > 0.01) {
            ulResults.append(mbps);
            out.append(QStringLiteral("  %1  %2  %3")
                .arg(QStringLiteral("%1 KB").arg(sizeKb).rightJustified(10, ' '))
                .arg(QStringLiteral("%1 Mbit/s").arg(mbps, 0, 'f', 2).leftJustified(16, ' '))
                .arg(QStringLiteral("%1 ms").arg(ulMs).rightJustified(6, ' ')));
        } else {
            out.append(QStringLiteral("  %1  %2  %3")
                .arg(QStringLiteral("%1 KB").arg(sizeKb).rightJustified(10, ' '))
                .arg(QStringLiteral("(timeout)").leftJustified(16, ' '))
                .arg(QStringLiteral("-").rightJustified(6, ' ')));
        }
    }

    double ulSpeed = avgTopN(ulResults);

    out.append(QString());
    out.append(QStringLiteral("------------------------------------------------------------------"));
    out.append(QStringLiteral("  Upload: %1 Mbit/s%2")
        .arg(ulSpeed, 0, 'f', 2)
        .arg(ulResults.size() >= 5 ? QStringLiteral("  (avg of top %1)").arg(qMin(5, (int)ulResults.size())) : QString()));

    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
    // Results
    // 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
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
        r.summary = QStringLiteral("Connected — ↓%1 ↑%2 Mbit/s").arg(dlSpeed, 0, 'f', 1).arg(ulSpeed, 0, 'f', 1);
    } else {
        r.status = DiagStatus::Warning;
        r.summary = QStringLiteral("Connected — speed test incomplete");
    }
    r.durationMs = totalTimer.elapsed();
    return r;
}

} // namespace G1G2G3Native
