#include "engine/diagnostics/GBase.h"
#include "engine/diagnostics/GHelpers.h"
namespace G1G2G3Native {
DiagnosticResult ipConfiguration(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

#if defined(_WIN32)
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
            // 5WHY: dnsFirst flag had no header guard (header was removed);
            // the flag set was dead code. cppcheck: duplicateConditionalAssign.
            for (auto* dns = a->FirstDnsServerAddress; dns; dns = dns->Next) {
                char ip[64]; DWORD ipLen = sizeof(ip);
                WSAAddressToStringA(dns->Address.lpSockaddr, dns->Address.iSockaddrLength, nullptr, ip, &ipLen);
                out.append(QStringLiteral("   DNS Servers . . . . . . . . . . . : %1").arg(QString::fromLatin1(ip)));
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
    // IP forwarding status (Linux only — /proc/sys not available on macOS/iOS)
#if !defined(__APPLE__) && !defined(PLATFORM_ANDROID)
    QFile ipForward(QStringLiteral("/proc/sys/net/ipv4/ip_forward"));
    bool routingEnabled = false;
    if (ipForward.open(QIODevice::ReadOnly))
        routingEnabled = ipForward.readAll().trimmed() == "1";
    out.append(QStringLiteral("   IP Routing Enabled. . . . . . . . : %1").arg(routingEnabled ? "Yes" : "No"));
#else
    out.append(QStringLiteral("   IP Routing Enabled. . . . . . . . : Unknown"));
#endif
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
#if !defined(__APPLE__) && !defined(PLATFORM_ANDROID)
            if (p->ifa_addr->sa_family == AF_PACKET) {
                auto* sll = (struct sockaddr_ll*)p->ifa_addr;
                unsigned char mac[6]; memcpy(mac, sll->sll_addr, 6);
                info.mac = macToStr(mac);
            }
#endif
        }
        freeifaddrs(ifa);

        // Build gateway map from /proc/net/route (Linux only)
        // 5WHY: Round 8 removed this block as "dead code" but the routes vector
        // IS consumed by the default-gateway lookup loop below (line 238).
        // mask member is unused but dest/gw/ifName are all needed.
        struct RouteEntry { QString ifName; uint32_t dest; uint32_t gw; };
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
                if (cols.size() >= 8) {
                    RouteEntry re;
                    re.ifName = cols[0];
                    bool ok1, ok2;
                    re.dest = cols[1].toUInt(&ok1, 16);
                    re.gw   = cols[2].toUInt(&ok2, 16);
                    if (ok1 && ok2) routes.append(re);
                }
            }
        }
#endif
        for (auto it = ifMap.begin(); it != ifMap.end(); ++it) {
            const auto& info = it.value();
            bool isLoopback = (info.flags & IFF_LOOPBACK);
            QString ifName = info.name;

            // Determine adapter type
            QString adapterLabel;
            if (isLoopback)
                adapterLabel = QStringLiteral("Unknown adapter %1:").arg(ifName);
#if defined(PLATFORM_IOS) || defined(__APPLE__)
            else if (ifName.startsWith("en"))
                adapterLabel = QStringLiteral("Wireless LAN adapter %1:").arg(ifName);
#if defined(PLATFORM_IOS)
            else if (ifName.startsWith("pdp_ip"))
                adapterLabel = QStringLiteral("Cellular adapter %1:").arg(ifName);
#endif
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
                // 5WHY: same dead flag pattern as Windows DNS block above.
                for (const auto& dns : dnsServers) {
                    out.append(QStringLiteral("   DNS Servers . . . . . . . . . . . : %1").arg(dns));
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
}
