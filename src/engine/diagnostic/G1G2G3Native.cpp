// =============================================================================
// G1G2G3Native.cpp — Pure C++ G1/G2/G3 diagnostics — ZERO shell commands
// Linux: getifaddrs, /proc/net, /sys/class/net, ioctl, netlink, socket APIs
// Windows: GetAdaptersAddresses, GetExtendedTcpTable, GetIpForwardTable2, etc.
// Output format: matches Windows CLI tools (ipconfig, route print, arp -a,
// netstat -an, netsh, nslookup)
// =============================================================================
#include "engine/diagnostic/G1G2G3Native.h"
#include <QElapsedTimer>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QRegularExpression>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <csignal>

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
// macOS / iOS — use AF_LINK+sockaddr_dl, no /proc, no /sys, no linux/wireless.h
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_arp.h>
#include <net/if_types.h>
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

// ═════════════════════════════════════════════════════════════════════════════
// Helpers
// ═════════════════════════════════════════════════════════════════════════════

static QString macToStr(const unsigned char* mac) {
    return QStringLiteral("%1-%2-%3-%4-%5-%6")
        .arg(mac[0], 2, 16, QLatin1Char('0'))
        .arg(mac[1], 2, 16, QLatin1Char('0'))
        .arg(mac[2], 2, 16, QLatin1Char('0'))
        .arg(mac[3], 2, 16, QLatin1Char('0'))
        .arg(mac[4], 2, 16, QLatin1Char('0'))
        .arg(mac[5], 2, 16, QLatin1Char('0'));
}

static QString ipToStr(uint32_t ip) {
    struct in_addr a; a.s_addr = ip; // ip is already network byte order from /proc/net/*
    return QString::fromLatin1(inet_ntoa(a));
}

static const char* tcpStateName(int st) {
    switch(st){case 1:return"ESTABLISHED";case 2:return"SYN_SENT";case 3:return"SYN_RECV";
    case 4:return"FIN_WAIT1";case 5:return"FIN_WAIT2";case 6:return"TIME_WAIT";
    case 7:return"CLOSE";case 8:return"CLOSE_WAIT";case 9:return"LAST_ACK";
    case 10:return"LISTEN";case 11:return"CLOSING";default:return"UNKNOWN";}
}

#ifndef _WIN32
// Parse /proc/net/tcp (or tcp6/udp/udp6) — hex format:
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

// ═════════════════════════════════════════════════════════════════════════════
// G1 — Network Adapters (ipconfig /all format)
// ═════════════════════════════════════════════════════════════════════════════
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
            info.ips.append(QString::fromLatin1(inet_ntoa(sa->sin_addr)));
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

    // ── ifconfig -s style table with MAC/IPv4 ─────────────────────────
    out.append(QStringLiteral("Iface        MTU   Status      MAC Address           IPv4 Address"));
    out.append(QStringLiteral("-----------  ----  ----------  --------------------  ---------------"));

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

        out.append(QStringLiteral("%1  %2  %3  %4  %5")
            .arg(info.name.leftJustified(12, ' ')).arg(mtu.rightJustified(4, ' '))
            .arg(state.leftJustified(10, ' ')).arg(mac.leftJustified(20, ' ')).arg(ip4));
    }
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = QStringLiteral("Network adapters enumerated");
    r.durationMs = t.elapsed();
    return r;
}

// ═════════════════════════════════════════════════════════════════════════════
// G1 — Active Connections (netstat -an format)
// ═════════════════════════════════════════════════════════════════════════════
DiagnosticResult activeConnections(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

    out.append(QString());
    out.append(QStringLiteral("Active Connections (netstat -an style)"));
    out.append(QString());
    out.append(QStringLiteral("  Proto  Local Address          Foreign Address        State"));

#ifdef _WIN32
    // Use GetExtendedTcpTable
    ULONG bufLen = 0;
    GetExtendedTcpTable(nullptr, &bufLen, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    QByteArray tcpBuf(bufLen, '\0');
    auto* tcpTable = (MIB_TCPTABLE_OWNER_PID*)tcpBuf.data();
    if (GetExtendedTcpTable(tcpTable, &bufLen, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
        for (DWORD i = 0; i < tcpTable->dwNumEntries; i++) {
            auto& row = tcpTable->table[i];
            struct in_addr la, ra;
            la.S_un.S_addr = row.dwLocalAddr; ra.S_un.S_addr = row.dwRemoteAddr;
            out.append(QStringLiteral("  TCP    %1:%2  %3:%4  %5")
                .arg(QString::fromLatin1(inet_ntoa(la)), -20)
                .arg(ntohs((u_short)row.dwLocalPort), 5)
                .arg(QString::fromLatin1(inet_ntoa(ra)), -20)
                .arg(ntohs((u_short)row.dwRemotePort), 5)
                .arg(QString::fromLatin1(tcpStateName(row.dwState))));
        }
    }
#else
    // Linux: parse /proc/net/tcp and /proc/net/udp
    auto parseProcNet = [](const QString& path, const QString& proto, QStringList& lines, bool isUdp) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return;
        QTextStream ts(&f);
        ts.readLine(); // skip header
        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            if (line.isEmpty()) continue;
            QStringList cols = line.split(QRegularExpression("\\s+"));
            if (cols.size() < 10) continue;

            // local_address: 0100007F:1F90 (hex IP:port)
            QStringList local = cols[1].split(':');
            QStringList remote = cols[2].split(':');

            auto hexToIp = [](const QString& hex) -> QString {
                bool ok; uint32_t ip = hex.toUInt(&ok, 16);
                return ipToStr(ip);
            };

            QString localIp = hexToIp(local[0]);
            int localPort = local[1].toInt(nullptr, 16);
            QString remoteIp = remote.size() > 0 ? hexToIp(remote[0]) : QStringLiteral("0.0.0.0");
            int remotePort = remote.size() > 1 ? remote[1].toInt(nullptr, 16) : 0;
            int state = cols[3].toInt(nullptr, 16);

            if (isUdp)
                lines.append(QStringLiteral("  %1  %2:%3  %4:%5  *:*")
                    .arg(proto.leftJustified(4, ' ')).arg(localIp.leftJustified(20, ' ')).arg(localPort, 5)
                    .arg(remoteIp.leftJustified(20, ' ')).arg(remotePort, 5));
            else
                lines.append(QStringLiteral("  %1  %2:%3  %4:%5  %6")
                    .arg(proto.leftJustified(4, ' ')).arg(localIp.leftJustified(20, ' ')).arg(localPort, 5)
                    .arg(remoteIp.leftJustified(20, ' ')).arg(remotePort, 5)
                    .arg(QString::fromLatin1(tcpStateName(state))));
        }
    };

    parseProcNet(QStringLiteral("/proc/net/tcp"), QStringLiteral("TCP"), out, false);
    parseProcNet(QStringLiteral("/proc/net/tcp6"), QStringLiteral("TCP6"), out, false);
    parseProcNet(QStringLiteral("/proc/net/udp"), QStringLiteral("UDP"), out, true);
    parseProcNet(QStringLiteral("/proc/net/udp6"), QStringLiteral("UDP6"), out, true);
#endif

    if (out.size() <= 3) out.append(QStringLiteral("  (no active connections)"));
    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = QStringLiteral("Active connections enumerated");
    r.durationMs = t.elapsed();
    return r;
}

// ═════════════════════════════════════════════════════════════════════════════
// G1 — IP Configuration (Windows ipconfig /all format 1:1)
// ═════════════════════════════════════════════════════════════════════════════
DiagnosticResult ipConfiguration(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

#ifdef _WIN32
    // ── Windows IP Configuration header ──────────────────────────────────
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
                    out.append(QStringLiteral("   IPv4 Address. . . . . . . . . . . : %1(Preferred)").arg(QString::fromLatin1(ip)));
                } else if (family == AF_INET6) {
                    out.append(QStringLiteral("   Link-local IPv6 Address . . . . . : %1(Preferred)").arg(QString::fromLatin1(ip)));
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
    // ── Linux IP Configuration (ipconfig /all style) ─────────────────────
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
                info.ips4.append(QString::fromLatin1(inet_ntoa(sa->sin_addr)));
            } else if (p->ifa_addr->sa_family == AF_INET6) {
                char buf6[INET6_ADDRSTRLEN];
                auto* sa6 = (struct sockaddr_in6*)p->ifa_addr;
                inet_ntop(AF_INET6, &sa6->sin6_addr, buf6, sizeof(buf6));
                info.ips6.append(QString::fromLatin1(buf6));
            }
            if (p->ifa_netmask && p->ifa_netmask->sa_family == AF_INET) {
                auto* nm = (struct sockaddr_in*)p->ifa_netmask;
                info.masks4.append(QString::fromLatin1(inet_ntoa(nm->sin_addr)));
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

        // Build /proc/net/route gateway map: ifName → {dest, gw, mask}
        struct RouteEntry { QString ifName; uint32_t dest; uint32_t gw; uint32_t mask; };
        QVector<RouteEntry> routes;
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

        for (auto it = ifMap.begin(); it != ifMap.end(); ++it) {
            const auto& info = it.value();
            bool isLoopback = (info.flags & IFF_LOOPBACK);
            QString ifName = info.name;

            // Determine adapter type
            QString adapterLabel;
            if (isLoopback)
                adapterLabel = QStringLiteral("Unknown adapter %1:").arg(ifName);
            else if (QFile::exists(QStringLiteral("/sys/class/net/%1/wireless").arg(ifName)))
                adapterLabel = QStringLiteral("Wireless LAN adapter %1:").arg(ifName);
            else
                adapterLabel = QStringLiteral("Ethernet adapter %1:").arg(ifName);

            out.append(adapterLabel);
            out.append(QString());

            // Connection-specific DNS Suffix
            out.append(QStringLiteral("   Connection-specific DNS Suffix  . :"));

            // Description (driver info from sysfs)
            QFile descFile(QStringLiteral("/sys/class/net/%1/device/uevent").arg(ifName));
            if (descFile.open(QIODevice::ReadOnly)) {
                QString uevent = QString::fromLatin1(descFile.readAll());
                for (const auto& line : uevent.split('\n')) {
                    if (line.startsWith("DRIVER="))
                        out.append(QStringLiteral("   Description . . . . . . . . . . . : %1").arg(line.mid(7)));
                }
            }

            // Physical Address (MAC)
            if (!info.mac.isEmpty() && !info.mac.startsWith("00-00-00"))
                out.append(QStringLiteral("   Physical Address. . . . . . . . . : %1").arg(info.mac));

            // DHCP Enabled
            bool dhcpEnabled = QFile::exists(QStringLiteral("/run/systemd/netif/leases/%1").arg(ifName))
                            || QFile::exists(QStringLiteral("/var/lib/dhcp/dhclient.%1.leases").arg(ifName));
            out.append(QStringLiteral("   DHCP Enabled. . . . . . . . . . . : %1").arg(dhcpEnabled ? "Yes" : "No"));
            out.append(QStringLiteral("   Autoconfiguration Enabled . . . . : Yes"));

            // IPv6 addresses
            for (const auto& ip6 : info.ips6) {
                if (!ip6.startsWith("fe80:"))
                    out.append(QStringLiteral("   IPv6 Address. . . . . . . . . . . : %1(Preferred)").arg(ip6));
                else
                    out.append(QStringLiteral("   Link-local IPv6 Address . . . . . : %1(Preferred)").arg(ip6));
            }

            // IPv4 addresses
            for (int i = 0; i < info.ips4.size(); i++) {
                out.append(QStringLiteral("   IPv4 Address. . . . . . . . . . . : %1(Preferred)").arg(info.ips4[i]));
                if (i < info.masks4.size())
                    out.append(QStringLiteral("   Subnet Mask . . . . . . . . . . . : %1").arg(info.masks4[i]));
            }

            // Lease info from dhclient or systemd-networkd
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

            // Default Gateway (from /proc/net/route)
            for (const auto& re : routes) {
                if (re.ifName == ifName && re.dest == 0 && re.gw != 0)
                    out.append(QStringLiteral("   Default Gateway . . . . . . . . . : %1").arg(ipToStr(re.gw)));
            }

            // DNS Servers
            if (!dnsServers.isEmpty()) {
                bool first = true;
                for (const auto& dns : dnsServers) {
                    out.append(QStringLiteral("   DNS Servers . . . . . . . . . . . : %1").arg(dns));
                    if (first) first = false;
                }
            }

            // Link speed + MTU
            QFile speedFile(QStringLiteral("/sys/class/net/%1/speed").arg(ifName));
            if (speedFile.open(QIODevice::ReadOnly)) {
                QString s = QString::fromLatin1(speedFile.readAll().trimmed());
                if (!s.isEmpty() && s != "-1")
                    out.append(QStringLiteral("   Link Speed . . . . . . . . . . . . : %1 Mbps").arg(s));
            }
            QFile mtuFile(QStringLiteral("/sys/class/net/%1/mtu").arg(ifName));
            if (mtuFile.open(QIODevice::ReadOnly))
                out.append(QStringLiteral("   MTU . . . . . . . . . . . . . . . : %1").arg(QString::fromLatin1(mtuFile.readAll().trimmed())));

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

// ═════════════════════════════════════════════════════════════════════════════
// G1 — WiFi Diagnostics (netsh wlan show interfaces format)
// ═════════════════════════════════════════════════════════════════════════════
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
                out.append(QStringLiteral("   GUID . . . . . . . . . . . . : %1").arg(QString::fromWCharArray(wi.strInterfaceDescription)));
                out.append(QStringLiteral("   State. . . . . . . . . . . . : %1").arg(wi.isState == wlan_interface_state_connected ? "connected" : "disconnected"));
                out.append(QString());
            }
            WlanFreeMemory(ifList);
        }
        WlanCloseHandle(hClient, nullptr);
    }
#else
    // Linux: wireless extensions + /sys/class/net/<wireless_iface>/
    // ── WiFi table header ────────────────────────────────────────────
    out.append(QStringLiteral("Interface    SSID                 BSSID            Channel   Signal   Bitrate"));
    out.append(QStringLiteral("---------    ----                 -----            -------   ------   -------"));

    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) == 0) {
        for (auto* p = ifa; p; p = p->ifa_next) {
            QString ifName = QString::fromLatin1(p->ifa_name);
            if (!QFile::exists(QStringLiteral("/sys/class/net/%1/wireless").arg(ifName)))
                continue;

            QString ssid = QStringLiteral("-"), bssid = QStringLiteral("-");
            QString channel = QStringLiteral("-"), signal = QStringLiteral("-"), bitrate = QStringLiteral("-");

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
                close(sock);
            }
#endif

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

            QFile rateFile(QStringLiteral("/sys/class/net/%1/wireless/bitrate").arg(ifName));
            if (rateFile.open(QIODevice::ReadOnly)) bitrate = QString::fromLatin1(rateFile.readAll().trimmed());

            out.append(QStringLiteral("%1  %2  %3  %4  %5  %6")
                .arg(ifName.leftJustified(12)).arg(ssid.leftJustified(20)).arg(bssid.leftJustified(17))
                .arg(channel.leftJustified(8)).arg(signal.leftJustified(7)).arg(bitrate));
        }
        freeifaddrs(ifa);
    }
    if (out.size() <= 5) out.append(QStringLiteral("  (no wireless interfaces detected)"));
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = out.size() > 3 ? DiagStatus::Pass : DiagStatus::Info;
    r.summary = QStringLiteral("WiFi diagnostics complete");
    r.durationMs = t.elapsed();
    return r;
}

// ═════════════════════════════════════════════════════════════════════════════
// G1 — NIC Advanced (wmic nic format)
// ═════════════════════════════════════════════════════════════════════════════
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
    out.append(QStringLiteral("Interface    Speed    Duplex   MTU   Carrier  State       MAC Address"));
    out.append(QStringLiteral("---------    -----    ------   ---   -------  -----       -----------"));

    QSet<QString> seenNic; // dedup: getifaddrs returns one entry per address family
    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) == 0) {
        for (auto* p = ifa; p; p = p->ifa_next) {
            QString ifName = QString::fromLatin1(p->ifa_name);
            if (ifName == "lo") continue;
            if (!(p->ifa_flags & IFF_UP)) continue;
            if (seenNic.contains(ifName)) continue;
            seenNic.insert(ifName);

            auto rd = [&](const QString& prop) {
                QFile f(QStringLiteral("/sys/class/net/%1/%2").arg(ifName, prop));
                if (f.open(QIODevice::ReadOnly)) return QString::fromLatin1(f.readAll().trimmed());
                return QStringLiteral("-");
            };

            out.append(QStringLiteral("%1  %2  %3  %4  %5  %6  %7")
                .arg(ifName.leftJustified(12, ' ')).arg(rd("speed").rightJustified(6, ' '))
                .arg(rd("duplex").leftJustified(6, ' ')).arg(rd("mtu").rightJustified(4, ' '))
                .arg(rd("carrier").leftJustified(7, ' ')).arg(rd("operstate").leftJustified(10, ' '))
                .arg(rd("address")));
        }
        freeifaddrs(ifa);
    }
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = QStringLiteral("NIC properties collected");
    r.durationMs = t.elapsed();
    return r;
}

// ═════════════════════════════════════════════════════════════════════════════
// G1 — Wired Diagnostics
// ═════════════════════════════════════════════════════════════════════════════
DiagnosticResult wiredDiagnostics(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

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
    out.append(QStringLiteral("Interface    Speed    Duplex   MTU   Link   State       MAC Address"));
    out.append(QStringLiteral("---------    -----    ------   ---   ----   -----       -----------"));

    QSet<QString> seenWired; // dedup: getifaddrs returns one entry per address family
    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) == 0) {
        for (auto* p = ifa; p; p = p->ifa_next) {
            QString ifName = QString::fromLatin1(p->ifa_name);
            if (ifName == "lo") continue;
            if (!(p->ifa_flags & IFF_UP)) continue;
            if (seenWired.contains(ifName)) continue;
            // Skip wireless interfaces
            if (QFile::exists(QStringLiteral("/sys/class/net/%1/wireless").arg(ifName))) continue;
            seenWired.insert(ifName);

            auto rd = [&](const QString& prop) {
                QFile f(QStringLiteral("/sys/class/net/%1/%2").arg(ifName, prop));
                if (f.open(QIODevice::ReadOnly)) return QString::fromLatin1(f.readAll().trimmed());
                return QStringLiteral("-");
            };

            out.append(QStringLiteral("%1  %2  %3  %4  %5  %6  %7")
                .arg(ifName.leftJustified(12, ' ')).arg(rd("speed").rightJustified(6, ' '))
                .arg(rd("duplex").leftJustified(6, ' ')).arg(rd("mtu").rightJustified(4, ' '))
                .arg(rd("carrier").leftJustified(4, ' ')).arg(rd("operstate").leftJustified(10, ' '))
                .arg(rd("address")));
        }
        freeifaddrs(ifa);
    }
    if (out.size() <= 3) out.append(QStringLiteral("  (no wired interfaces detected)"));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = out.size() > 3 ? DiagStatus::Pass : DiagStatus::Info;
    r.summary = QStringLiteral("Wired diagnostics complete");
    r.durationMs = t.elapsed();
    return r;
#endif
}

// ═════════════════════════════════════════════════════════════════════════════
// G1 — DHCP Status (ipconfig /all DHCP section format)
// ═════════════════════════════════════════════════════════════════════════════
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
                        { QString ip = line.mid(8); out.append(QStringLiteral("   IPv4 Address. . . . . . . . . . . : %1(Preferred)").arg(ip)); if (!dhcpSummary.contains(ifName + "=" + ip)) dhcpSummary.append(QStringLiteral("%1=%2").arg(ifName, ip)); }
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
                    out.append(QStringLiteral("   IPv4 Address. . . . . . . . . . . : %1(Preferred)").arg(currentIp));
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
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = dhcpSummary.isEmpty() ? QStringLiteral("No DHCP leases found (static IP?)")
                 : QStringLiteral("DHCP: %1").arg(dhcpSummary.join(QStringLiteral(", ")));
    r.durationMs = t.elapsed();
    return r;
}

// ═════════════════════════════════════════════════════════════════════════════
// G2 — Routing Table (route print format)
// ═════════════════════════════════════════════════════════════════════════════
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
    out.append(QStringLiteral("Network Destination        Netmask          Gateway       Interface  Metric"));

    // Get IPv4 forwarding table
    PMIB_IPFORWARD_TABLE2 ft = nullptr;
    if (GetIpForwardTable2(AF_INET, &ft) == NO_ERROR) {
        for (ULONG i = 0; i < ft->NumEntries; i++) {
            auto& row = ft->Table[i];
            struct in_addr dest, mask, gw, iface;
            dest.S_un.S_addr = row.DestinationPrefix.Prefix.S_un.S_addr;
            // ... would need full extract of prefix length to netmask
            out.append(QStringLiteral("  %1").arg(QString::fromLatin1(inet_ntoa(dest))));
        }
        FreeMibTable(ft);
    }
#else
    // Linux: parse /proc/net/route
    out.append(QStringLiteral("==========================================================================="));
    out.append(QString());
    out.append(QStringLiteral("IPv4 Route Table"));
    out.append(QStringLiteral("==========================================================================="));
    out.append(QStringLiteral("Active Routes:"));
    out.append(QStringLiteral("Network Destination        Netmask          Gateway       Interface  Metric"));

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

                out.append(QStringLiteral("  %1  %2  %3  %4  %5")
                    .arg(ipToStr(dest), 15)
                    .arg(ipToStr(mask), 15)
                    .arg(gw ? ipToStr(gw) : QStringLiteral("On-link"), 15)
                    .arg(ifName, 9)
                    .arg(metric));
            }
        }
    }
#endif

    out.append(QStringLiteral("==========================================================================="));
    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = QStringLiteral("Routing table collected");
    r.durationMs = t.elapsed();
    return r;
}

// ═════════════════════════════════════════════════════════════════════════════
// G2 — ARP Table (arp -a format)
// ═════════════════════════════════════════════════════════════════════════════
DiagnosticResult arpTable(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

    out.append(QString());

#ifdef _WIN32
    ULONG bufLen = 0;
    GetIpNetTable2(AF_INET, nullptr, &bufLen);
    QByteArray buf(bufLen, '\0');
    auto* table = (MIB_IPNET_TABLE2*)buf.data();
    if (GetIpNetTable2(AF_INET, table, &bufLen, FALSE) == NO_ERROR) {
        out.append(QStringLiteral("Interface: (all)"));
        out.append(QStringLiteral("  Internet Address      Physical Address      Type"));
        for (ULONG i = 0; i < table->NumEntries; i++) {
            auto& row = table->Table[i];
            struct in_addr ip; ip.s_addr = row.Address.s_addr();
            out.append(QStringLiteral("  %1           %2     %3")
                .arg(QString::fromLatin1(inet_ntoa(ip)), -20)
                .arg(macToStr((const unsigned char*)&row.PhysicalAddress))
                .arg(row.State == NlnsReachable ? "dynamic" : "static"));
        }
    }
#else
    // Linux: parse /proc/net/arp
    QFile arpFile(QStringLiteral("/proc/net/arp"));
    if (arpFile.open(QIODevice::ReadOnly)) {
        QTextStream ts(&arpFile);
        QString header = ts.readLine(); // skip header
        out.append(QStringLiteral("Interface: (all)"));
        out.append(QStringLiteral("  Internet Address      Physical Address      Type"));

        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            if (line.isEmpty()) continue;
            QStringList cols = line.split(QRegularExpression("\\s+"));
            if (cols.size() >= 5) {
                QString ip = cols[0];
                QString mac = cols[3];
                QString ifName = cols[5];
                QString type = (cols[2] == "0x2") ? "static" : "dynamic";
                out.append(QStringLiteral("  %1           %2     %3")
                    .arg(ip, -20).arg(mac, -20).arg(type));
            }
        }
    } else {
        out.append(QStringLiteral("  (ARP table not available)"));
    }
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = QStringLiteral("ARP table collected");
    r.durationMs = t.elapsed();
    return r;
}

// ═════════════════════════════════════════════════════════════════════════════
// Remaining G2/G3 stubs with native implementations
// ═════════════════════════════════════════════════════════════════════════════

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
    QFile fwd(QStringLiteral("/proc/sys/net/ipv4/ip_forward"));
    if (fwd.open(QIODevice::ReadOnly))
        out.append(QStringLiteral("  IP Forwarding: %1").arg(QString::fromLatin1(fwd.readAll().trimmed()) == "1" ? "Enabled" : "Disabled"));
#endif

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
    out.append(QStringLiteral("TCP/IP Settings:"));
    out.append(QString());

#ifdef _WIN32
    out.append(QStringLiteral("  (use netsh int tcp show global for details)"));
#else
    // Read /proc/sys/net/ipv4/tcp_* settings
    auto readSys = [&](const QString& path, const QString& label) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly))
            out.append(QStringLiteral("  %1: %2").arg(label, QString::fromLatin1(f.readAll().trimmed())));
    };
    readSys(QStringLiteral("/proc/sys/net/ipv4/tcp_congestion_control"), QStringLiteral("Congestion Control"));
    readSys(QStringLiteral("/proc/sys/net/ipv4/tcp_window_scaling"), QStringLiteral("Window Scaling"));
    readSys(QStringLiteral("/proc/sys/net/ipv4/tcp_timestamps"), QStringLiteral("Timestamps"));
    readSys(QStringLiteral("/proc/sys/net/ipv4/tcp_sack"), QStringLiteral("Selective ACK"));
    readSys(QStringLiteral("/proc/sys/net/ipv4/tcp_fastopen"), QStringLiteral("TCP Fast Open"));
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = QStringLiteral("TCP settings collected");
    r.durationMs = 0;
    return r;
}

DiagnosticResult defaultGateway(DiagId id) {
    // Extract gateway from routing table
    return routingTable(id); // Gateway info is in route print output
}

DiagnosticResult proxySettings(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Proxy Configuration:"));
    out.append(QString());

#ifdef _WIN32
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG cfg = {};
    if (WinHttpGetIEProxyConfigForCurrentUser(&cfg)) {
        if (cfg.lpszProxy) out.append(QStringLiteral("  HTTP Proxy: %1").arg(QString::fromWCharArray(cfg.lpszProxy)));
        if (cfg.lpszProxyBypass) out.append(QStringLiteral("  Bypass: %1").arg(QString::fromWCharArray(cfg.lpszProxyBypass)));
        GlobalFree(cfg.lpszProxy); GlobalFree(cfg.lpszProxyBypass);
    }
#else
    // Check environment variables
    const char* vars[] = {"HTTP_PROXY","HTTPS_PROXY","FTP_PROXY","NO_PROXY","http_proxy","https_proxy","no_proxy"};
    for (auto* v : vars) {
        const char* val = getenv(v);
        if (val && val[0])
            out.append(QStringLiteral("  %1=%2").arg(QString::fromLatin1(v), QString::fromLatin1(val)));
    }
    if (out.size() <= 3) out.append(QStringLiteral("  No proxy configured"));
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Info;
    r.summary = QStringLiteral("Proxy settings collected");
    r.durationMs = 0;
    return r;
}

// ── G3 ────────────────────────────────────────────────────────────────────

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
    out.append(QStringLiteral("DNS Server Configuration (resolv.conf)"));
    out.append(QString());

#ifdef _WIN32
    ULONG bufLen = 15000;
    QByteArray buf(bufLen, '\0');
    PIP_ADAPTER_ADDRESSES adapters = (PIP_ADAPTER_ADDRESSES)buf.data();
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_UNICAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_ANYCAST, nullptr, adapters, &bufLen) == NO_ERROR) {
        for (auto* a = adapters; a; a = a->Next) {
            out.append(QStringLiteral("  Adapter: %1").arg(QString::fromWCharArray(a->FriendlyName)));
            for (auto* dns = a->FirstDnsServerAddress; dns; dns = dns->Next) {
                char ip[64]; DWORD ipLen = sizeof(ip);
                WSAAddressToStringA(dns->Address.lpSockaddr, dns->Address.iSockaddrLength, nullptr, ip, &ipLen);
                QString ipStr = QString::fromLatin1(ip);
                out.append(QStringLiteral("    DNS Server: %1").arg(ipStr));
                if (!dnsList.contains(ipStr)) dnsList.append(ipStr);
            }
        }
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
                out.append(QStringLiteral("  Nameserver: %1").arg(ns));
                if (!dnsList.contains(ns)) dnsList.append(ns);
            }
            else if (line.startsWith("search "))
                out.append(QStringLiteral("  Search domains: %1").arg(line.mid(7)));
        }
    }
    // Also check systemd-resolved stub
    QFile stub(QStringLiteral("/run/systemd/resolve/resolv.conf"));
    if (stub.open(QIODevice::ReadOnly)) {
        out.append(QStringLiteral("  (systemd-resolved stub resolver active)"));
    }
#endif

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
    out.append(QStringLiteral("Windows IP Configuration"));
    out.append(QString());

#ifdef _WIN32
    out.append(QStringLiteral("DNS Client Cache (ipconfig /displaydns format)"));
    out.append(QStringLiteral("=============================================="));
    out.append(QString());
    out.append(QStringLiteral("(Use 'ipconfig /displaydns' for full cache contents)"));
    out.append(QStringLiteral("To flush: ipconfig /flushdns"));
#else
    // ── Try systemd-resolved cache (most common on modern Linux) ────────
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
                    // Unparsed line — show as-is
                    out.append(QStringLiteral("    %1").arg(trimmed));
                }
            }
        } else {
            out.append(QStringLiteral("    (cache is empty)"));
        }
    } else {
        // ── No systemd-resolved — check and show resolution setup ───────
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
                    out.append(QStringLiteral("    Namespace Servers . . . . : %1").arg(line.mid(11)));
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
        r.summary = QStringLiteral("Cache active · %1 cached DNS entries").arg(cacheEntries);
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
    out.append(QStringLiteral("DNS Pollution Check (resolving non-existent domains):"));
    out.append(QString());
    out.append(QStringLiteral("  Domain                                  Result"));
    out.append(QStringLiteral("  ------                                  ------"));

    // Test known-bad domains — if they resolve, DNS is being hijacked
    struct { const char* domain; } testCases[] = {
        {"thisdomainshouldnotexist12345.com"},
        {"nonexistent-test-domain-98765.org"},
        {"definitely-not-real-domain-42.net"},
    };

    bool hijacked = false;
    for (auto& tc : testCases) {
        struct addrinfo hints = {}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        // NOTE: getaddrinfo() is a blocking call with no timeout parameter.
        // On systems with unresponsive DNS, each lookup can block for the
        // system resolver's internal timeout (5-30 s). We use SIGALRM as a
        // 5-second emergency escape on Linux. On macOS/BSD, the resolver
        // typically has a shorter built-in timeout.
#ifdef __linux__
        struct sigaction sa_old, sa_new;
        sa_new.sa_handler = [](int){};
        sa_new.sa_flags = 0;
        sigemptyset(&sa_new.sa_mask);
        sigaction(SIGALRM, &sa_new, &sa_old);
        alarm(5);
#endif
        int rc = getaddrinfo(tc.domain, nullptr, &hints, &res);
#ifdef __linux__
        alarm(0);
        sigaction(SIGALRM, &sa_old, nullptr);
        // If SIGALRM fired, rc may be EAI_SYSTEM with errno=EINTR
        if (rc != 0 && errno == EINTR) {
            out.append(QStringLiteral("  %1           TIMEOUT").arg(tc.domain, -40));
            continue;
        }
#endif
        if (rc == 0 && res) {
            char ip[64];
            inet_ntop(AF_INET, &((struct sockaddr_in*)res->ai_addr)->sin_addr, ip, sizeof(ip));
            out.append(QStringLiteral("  %1           RESOLVED → %2 (POSSIBLE HIJACK)")
                .arg(tc.domain, -40).arg(ip));
            hijacked = true;
            freeaddrinfo(res);
        } else {
            out.append(QStringLiteral("  %1           NXDOMAIN (clean)")
                .arg(tc.domain, -40));
        }
    }

    out.append(QString());
    out.append(hijacked ? QStringLiteral("  [WARN] DNS appears to be hijacked") : QStringLiteral("  [OK] No DNS hijacking detected"));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = hijacked ? DiagStatus::Warning : DiagStatus::Pass;
    r.summary = hijacked ? QStringLiteral("DNS hijacking detected") : QStringLiteral("DNS clean");
    r.durationMs = t.elapsed();
    return r;
}

// internetConnectivity() removed — merged into speedTest() (Phase 0 connectivity check)

// ═════════════════════════════════════════════════════════════════════════════
// Speed Test — Speedtest.net protocol (Ookla-compatible)
// Mimics "speedtest-cli" output format
// =============================================================================
// Protocol:
//   1. GET /api/js/servers → JSON server list
//   2. TCP ping each candidate → pick lowest latency
//   3. GET {url}/download?size=N → measure download throughput
//   4. POST {url}/upload → measure upload throughput
// ═════════════════════════════════════════════════════════════════════════════

// ── Simple HTTP GET via raw socket (no Qt event loop needed) ────────────
static QByteArray httpGet(const QString& host, int port, const QString& path, int timeoutMs, int maxBytes) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return {};
    struct addrinfo hints = {}, *res;
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    QByteArray hostBytes = host.toUtf8();
    if (getaddrinfo(hostBytes.constData(), nullptr, &hints, &res) != 0) { close(sock); return {}; }

    struct sockaddr_in addr; memcpy(&addr, res->ai_addr, sizeof(addr));
    addr.sin_port = htons(port);
    freeaddrinfo(res);

#ifdef _WIN32
    u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
    ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
    struct timeval tv = {timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    if (select(sock + 1, nullptr, &fdset, nullptr, &tv) <= 0) { close(sock); return {}; }
    int err = 0; socklen_t len = sizeof(err);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
    if (err != 0) { close(sock); return {}; }

    // Send HTTP request (loop handles partial sends, EAGAIN-safe)
    QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: NetDiagnostic/1.0\r\nAccept: */*\r\nConnection: close\r\n\r\n")
        .arg(path, host).toUtf8();
    int sent = 0;
    while (sent < req.size()) {
        int n = ::send(sock, req.constData() + sent, req.size() - sent, 0);
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
    close(sock);
    return response;
}

// ── HTTP download with throughput measurement ───────────────────────────
struct SpeedResult { double mbps; int bytes; int durationMs; bool ok; };
static SpeedResult httpDownload(const QString& urlStr, int targetBytes, int timeoutMs) {
    SpeedResult r = {0, 0, 0, false};
    // Parse URL → host, port, path
    QString u = urlStr;
    if (!u.startsWith("http://")) return r;
    u = u.mid(7); // strip "http://"
    int slash = u.indexOf('/');
    QString hostPort = (slash > 0) ? u.left(slash) : u;
    QString path = (slash > 0) ? u.mid(slash) : "/";

    QString host = hostPort; int port = 80;
    int colon = hostPort.lastIndexOf(':');
    if (colon > 0) { host = hostPort.left(colon); port = hostPort.mid(colon + 1).toInt(); }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return r;
    struct addrinfo hints = {}, *res;
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    QByteArray hb = host.toUtf8();
    if (getaddrinfo(hb.constData(), nullptr, &hints, &res) != 0) { close(sock); return r; }
    struct sockaddr_in addr; memcpy(&addr, res->ai_addr, sizeof(addr));
    addr.sin_port = htons(port);
    freeaddrinfo(res);

#ifdef _WIN32
    u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
    QElapsedTimer t; t.start();
    ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
    struct timeval tv = {3, 0};
    if (select(sock + 1, nullptr, &fdset, nullptr, &tv) <= 0) { close(sock); return r; }
    int err = 0; socklen_t len = sizeof(err);
    getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
    if (err != 0) { close(sock); return r; }

    // Send HTTP GET (loop handles partial sends, EAGAIN-safe)
    QByteArray req = QStringLiteral("GET %1 HTTP/1.0\r\nHost: %2\r\nUser-Agent: NetDiagnostic/1.0\r\nConnection: close\r\n\r\n")
        .arg(path, host).toUtf8();
    int reqSent = 0;
    while (reqSent < req.size()) {
        int n = ::send(sock, req.constData() + reqSent, req.size() - reqSent, 0);
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

    // Read with timing — measure throughput (wall-clock guarded)
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
            int hdrEnd = body.indexOf("\r\n\r\n");
            if (hdrEnd >= 0) {
                body = body.mid(hdrEnd + 4);
                headersDone = true;
                startNs = t.nsecsElapsed(); // reset timer to body start
            }
        } else {
            body.append(buf, (int)n);
        }
    }
    close(sock);

    qint64 elapsedNs = t.nsecsElapsed() - startNs;
    if (elapsedNs <= 0) elapsedNs = 1;
    r.bytes = body.size();
    r.durationMs = (int)(elapsedNs / 1000000);
    if (r.bytes > 0 && r.durationMs > 0) {
        double bits = r.bytes * 8.0;
        double secs = r.durationMs / 1000.0;
        r.mbps = bits / secs / 1000000.0;
        r.ok = true;
    }
    return r;
}

// ── TCP ping (simple connect RTT) ───────────────────────────────────────
static int tcpPingMs(const QString& host, int port) {
    QElapsedTimer t; t.start();
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    struct addrinfo hints = {}, *res;
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    QByteArray hb = host.toUtf8();
    if (getaddrinfo(hb.constData(), nullptr, &hints, &res) != 0) { close(sock); return -1; }
    struct sockaddr_in addr; memcpy(&addr, res->ai_addr, sizeof(addr));
    addr.sin_port = htons(port);
    freeaddrinfo(res);
#ifdef _WIN32
    u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
    ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
    struct timeval tv = {2, 0};
    int sel = select(sock + 1, nullptr, &fdset, nullptr, &tv);
    int ms = t.elapsed();
    if (sel > 0) { int err = 0; socklen_t len = sizeof(err); getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len); if (err != 0) ms = -1; }
    else ms = -1;
    close(sock);
    return ms;
}

// ── HTTP latency via tiny file download (speedtest-cli style latency.txt) ──
// Measures real application-layer RTT: DNS + TCP connect + HTTP request/response
// Much better predictor of download throughput than raw TCP ping.
static int httpLatencyMs(const QString& urlStr, int timeoutMs) {
    QElapsedTimer t; t.start();
    QString u = urlStr;
    if (!u.startsWith("http://")) return -1;
    u = u.mid(7);
    int slash = u.indexOf('/');
    QString hostPort = (slash > 0) ? u.left(slash) : u;
    QString host = hostPort; int port = 80;
    int colon = hostPort.lastIndexOf(':');
    if (colon > 0) { host = hostPort.left(colon); port = hostPort.mid(colon + 1).toInt(); }

    // Download latency.txt from server root — speedtest-cli uses the root path
    // regardless of the download/upload URL structure
    QString latPath = QStringLiteral("/latency.txt");
    QByteArray resp = httpGet(host, port, latPath, timeoutMs, 256);
    if (resp.isEmpty()) return -1;

    // Parse HTTP response — extract body after \r\n\r\n
    int hdrEnd = resp.indexOf("\r\n\r\n");
    if (hdrEnd < 0) return -1;
    QByteArray body = resp.mid(hdrEnd + 4);
    // latency.txt should contain a small text like "test=...", we just need the time
    if (body.trimmed().isEmpty()) return -1;

    return t.elapsed();
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

    // ═════════════════════════════════════════════════════════════════════
    // Phase 0 — Quick connectivity check (TCP to well-known hosts)
    // ═════════════════════════════════════════════════════════════════════
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
        {"8.8.8.8", 53, "Google DNS"},
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

    // ═════════════════════════════════════════════════════════════════════
    // Phase 1 — Fetch server list from speedtest.net
    // ═════════════════════════════════════════════════════════════════════
    out.append(QStringLiteral("Retrieving speedtest.net server list..."));
    // Fast fail: if speedtest.net is blocked (China), don't wait 10s.
    // 3s timeout is enough for a reachable server to respond.
    QByteArray serversJson = httpGet("www.speedtest.net", 80,
        "/api/js/servers?engine=js&limit=15", 3000, 262144);

    struct SpeedServer { QString url; QString host; QString name; QString sponsor; QString country; int port; double lat; double lon; };
    QVector<SpeedServer> servers;

    // Parse JSON array manually (avoids QJsonDocument dependency issues)
    if (!serversJson.isEmpty()) {
        // Strip HTTP headers
        int hdrEnd = serversJson.indexOf("\r\n\r\n");
        if (hdrEnd >= 0) serversJson = serversJson.mid(hdrEnd + 4);

        QString json = QString::fromUtf8(serversJson);
        // Parse each { ... } object
        int pos = 0;
        while ((pos = json.indexOf("\"url\":\"", pos)) >= 0) {
            pos += 7;
            int urlEnd = json.indexOf('"', pos);
            if (urlEnd < 0) break;
            QString surl = json.mid(pos, urlEnd - pos).replace("\\/", "/");

            SpeedServer s; s.url = surl; s.port = 8080;
            // Extract host from URL
            if (surl.startsWith("http://")) {
                QString hp = surl.mid(7);
                int sl = hp.indexOf('/');
                QString hostPort = (sl > 0) ? hp.left(sl) : hp;
                int co = hostPort.lastIndexOf(':');
                if (co > 0) { s.host = hostPort.left(co); s.port = hostPort.mid(co + 1).toInt(); }
                else s.host = hostPort;
            }

            // name
            int np = json.indexOf("\"name\":\"", urlEnd);
            if (np >= 0) { np += 8; int ne = json.indexOf('"', np); if (ne > np) s.name = json.mid(np, ne - np); }

            // sponsor
            int sp = json.indexOf("\"sponsor\":\"", urlEnd);
            if (sp >= 0) { sp += 11; int se = json.indexOf('"', sp); if (se > sp) s.sponsor = json.mid(sp, se - sp); }

            // country
            int cp = json.indexOf("\"country\":\"", urlEnd);
            if (cp >= 0) { cp += 11; int ce = json.indexOf('"', cp); if (ce > cp) s.country = json.mid(cp, ce - cp); }

            // lat/lon
            int lp = json.indexOf("\"lat\":", urlEnd);
            if (lp >= 0) { lp += 6; int le = json.indexOf(',', lp); if (le < 0) le = json.indexOf('}', lp); if (le > lp) s.lat = json.mid(lp, le - lp).toDouble(); }
            int op = json.indexOf("\"lon\":", urlEnd);
            if (op >= 0) { op += 6; int oe = json.indexOf(',', op); if (oe < 0) oe = json.indexOf('}', op); if (oe > op) s.lon = json.mid(op, oe - op).toDouble(); }

            if (!s.host.isEmpty()) servers.append(s);
        }
    }

    // Fallback servers if API fails
    if (servers.isEmpty()) {
        struct { const char* host; const char* name; const char* sponsor; } fb[] = {
            // ── Chinese mainland speedtest servers (reachable without VPN) ──
            {"speedtest1.gd.chinamobile.com:8080", "Guangzhou", "China Mobile"},
            {"speedtest2.gz.chinamobile.com:8080", "Guangzhou 2", "China Mobile"},
            {"speedtest.bj.chinamobile.com:8080", "Beijing", "China Mobile"},
            {"speedtest.sz.supergaminator.com:8080", "Shenzhen", "SuperGaminator"},
            // Ookla server IDs — speedtest.net protocol via direct host
            {"speedtest1.online.sh.cn:8080", "Shanghai", "China Telecom"},
            {"speedtest2.fj.chinamobile.com:8080", "Fujian", "China Mobile"},
            {"speedtest.sc.chinamobile.com:8080", "Sichuan", "China Mobile"},
            // CDN-based fallback: download test file from Aliyun OSS
            {"dg5oj7x6qhvhr.cloudfront.net:80", "CloudFront", "AWS CDN"},
        };
        for (auto& f : fb) {
            SpeedServer s; s.url = QStringLiteral("http://%1").arg(f.host);
            QString hp = f.host; int co = hp.lastIndexOf(':');
            s.host = co > 0 ? hp.left(co) : hp;
            s.port = co > 0 ? hp.mid(co + 1).toInt() : 8080;
            s.name = f.name; s.sponsor = f.sponsor; s.country = "China";
            servers.append(s);
        }
        out.append(QStringLiteral("  (using pre-configured server list)"));
    }

    // ═════════════════════════════════════════════════════════════════════
    // Phase 2 — Select best server by HTTP latency (speedtest-cli style)
    // ═════════════════════════════════════════════════════════════════════
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

    struct RankedServer { SpeedServer* srv; int latency; };
    QVector<RankedServer> ranked;
    for (auto& s : servers) {
        int lat = httpLatencyMs(s.url, 5000);
        if (lat > 0)
            ranked.append({&s, lat});
    }

    if (ranked.isEmpty()) {
        out.append(QStringLiteral("  (no reachable servers)"));
        out.append(QString());
        r.rawOutput = out.join('\n'); r.details = r.rawOutput;
        r.status = hasConnectivity ? DiagStatus::Warning : DiagStatus::Fail;
        r.summary = hasConnectivity ? QStringLiteral("Connected · no speed test servers reachable")
                                    : QStringLiteral("No internet connectivity");
        r.durationMs = totalTimer.elapsed(); return r;
    }

    // Sort by HTTP latency ascending — fastest first
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

    SpeedServer* best = ranked[0].srv;
    int bestLatency = ranked[0].latency;

    out.append(QString());
    out.append(QStringLiteral("------------------------------------------------------------------"));
    out.append(QStringLiteral("  Selected: %1 (%2) — %3 ms")
        .arg(best->sponsor, best->name).arg(bestLatency));
    out.append(QString());

    // ═════════════════════════════════════════════════════════════════════
    // Phase 3 — Download test (with server fallback)
    // ═════════════════════════════════════════════════════════════════════
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
        // per size tier — a server that handles 250KB may choke on 25MB.
        bool ok = false;
        for (int si = 0; si < ranked.size(); si++) {
            // Try each server once before marking failure
            int idx = (dlServerIdx + si) % ranked.size();
            SpeedServer* srv = ranked[idx].srv;
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
            // Don't abort — try next size tier even if this one failed
        }
    }

    double dlSpeed = 0;
    if (!dlResults.isEmpty()) {
        std::sort(dlResults.begin(), dlResults.end());
        int count = qMin(5, (int)dlResults.size());
        double sum = 0;
        for (int i = dlResults.size() - count; i < dlResults.size(); i++) sum += dlResults[i];
        dlSpeed = sum / count;
    }

    out.append(QString());
    out.append(QStringLiteral("------------------------------------------------------------------"));
    out.append(QStringLiteral("  Download: %1 Mbit/s%2")
        .arg(dlSpeed, 0, 'f', 2)
        .arg(dlResults.size() >= 5 ? QStringLiteral("  (avg of top %1)").arg(qMin(5, (int)dlResults.size())) : QString()));

    // ═════════════════════════════════════════════════════════════════════
    // Phase 4 — Upload test
    // ═════════════════════════════════════════════════════════════════════
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
        struct addrinfo hints = {}, *res;
        hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
        QByteArray hb = best->host.toUtf8();
        char portStr[16]; snprintf(portStr, sizeof(portStr), "%d", best->port);
        if (getaddrinfo(hb.constData(), portStr, &hints, &res) != 0) { close(sock); continue; }
        struct sockaddr_in addr; memcpy(&addr, res->ai_addr, sizeof(addr));
        freeaddrinfo(res);

#ifdef _WIN32
        u_long mode = 1; ioctlsocket(sock, FIONBIO, &mode);
#else
        int flags = fcntl(sock, F_GETFL, 0); fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
        ::connect(sock, (struct sockaddr*)&addr, sizeof(addr));
        fd_set fdset; FD_ZERO(&fdset); FD_SET(sock, &fdset);
        struct timeval tv = {3, 0};
        if (select(sock + 1, nullptr, &fdset, nullptr, &tv) <= 0) { close(sock); continue; }
        int err = 0; socklen_t len = sizeof(err);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
        if (err != 0) { close(sock); continue; }

        // Generate random data
        QByteArray uploadData(dataSize, 'A');
        for (int i = 0; i < qMin(dataSize, 4096); i++) uploadData[i] = (char)('A' + (rand() % 26));

        // POST request headers
        QByteArray postHeaders = QStringLiteral(
            "POST /upload HTTP/1.0\r\nHost: %1\r\nContent-Type: application/octet-stream\r\nContent-Length: %2\r\nConnection: close\r\n\r\n")
            .arg(best->host).arg(dataSize).toUtf8();

        QElapsedTimer ulTimer; ulTimer.start();
        // Send POST headers (EAGAIN-safe: select() for writability on stall)
        int hdrSent = 0;
        while (hdrSent < postHeaders.size()) {
            int n = ::send(sock, postHeaders.constData() + hdrSent, postHeaders.size() - hdrSent, 0);
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
            int n = ::send(sock, dp + sent, chunk, 0);
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
        int ulMs = ulTimer.elapsed();
        close(sock);

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

    double ulSpeed = 0;
    if (!ulResults.isEmpty()) {
        std::sort(ulResults.begin(), ulResults.end());
        int count = qMin(5, (int)ulResults.size());
        double sum = 0;
        for (int i = ulResults.size() - count; i < ulResults.size(); i++) sum += ulResults[i];
        ulSpeed = sum / count;
    }

    out.append(QString());
    out.append(QStringLiteral("------------------------------------------------------------------"));
    out.append(QStringLiteral("  Upload: %1 Mbit/s%2")
        .arg(ulSpeed, 0, 'f', 2)
        .arg(ulResults.size() >= 5 ? QStringLiteral("  (avg of top %1)").arg(qMin(5, (int)ulResults.size())) : QString()));

    // ═════════════════════════════════════════════════════════════════════
    // Results
    // ═════════════════════════════════════════════════════════════════════
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
        r.summary = QStringLiteral("Connected · ↓%1 ↑%2 Mbit/s").arg(dlSpeed, 0, 'f', 1).arg(ulSpeed, 0, 'f', 1);
    } else {
        r.status = DiagStatus::Warning;
        r.summary = QStringLiteral("Connected · speed test incomplete");
    }
    r.durationMs = totalTimer.elapsed();
    return r;
}

} // namespace G1G2G3Native
