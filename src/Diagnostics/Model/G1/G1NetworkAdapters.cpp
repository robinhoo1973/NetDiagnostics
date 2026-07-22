#include "Diagnostics/Model/GBase.h"
#include "Diagnostics/Model/GHelpers.h"
namespace SystemDiagnostics {
DiagnosticResult networkAdapters(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

    out.append(QString());
    out.append(QStringLiteral("Network Adapters (ifconfig -s style)"));
    out.append(QString());

    QList<QStringList> netRows;

#if defined(_WIN32)
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
        netRows.append({QString::fromWCharArray(a->FriendlyName), ifType,
            QStringLiteral("UP"), QStringLiteral("-"), QStringLiteral("-")});
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
#if defined(PLATFORM_IOS)
    // iOS: use getifaddrs + SystemConfiguration framework
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

    // WiFi SSID (needs com.apple.developer.networking.wifi-info entitlement)
    QString wifiSSID = iosCopyWiFiSSID();

    freeifaddrs(ifa);

    // Build output table from ifMap
    static const QVector<DiagnosticFormatter::ColSpec> kNetCols = {
        {"Iface",       12, false},
        {"Type",        12, false},
        {"Status",      10, false},
        {"IPv4 Address",18, false},
        {"SSID",        20, false},
    };
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

    // Cellular info section
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
#if !defined(__APPLE__) && !defined(PLATFORM_ANDROID)
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

    // ifconfig -s style table with MAC/IPv4
    static const QVector<DiagnosticFormatter::ColSpec> kNetCols = {
        {"Iface",       12, false},
        {"MTU",          4, true},
        {"Status",      10, false},
        {"MAC Address", 20, false},
        {"IPv4 Address", 0, false},
    };

    for (auto it = ifMap.begin(); it != ifMap.end(); ++it) {
        const IfInfo& info = it.value();
        bool isLoopback = (info.flags & IFF_LOOPBACK);

        // Read MTU and operstate (Linux: /sys; macOS: ioctl)
        QString mtu = QStringLiteral("-"), state = QStringLiteral("DOWN");
#if defined(__APPLE__)
        int tmpSock = socket(AF_INET, SOCK_DGRAM, 0);
        if (tmpSock >= 0) {
            struct ifreq ifr;
            memset(&ifr, 0, sizeof(ifr));
            strncpy(ifr.ifr_name, info.name.toUtf8().constData(), IFNAMSIZ-1);
            if (ioctl(tmpSock, SIOCGIFMTU, &ifr) == 0)
                mtu = QString::number(ifr.ifr_mtu);
            if (ioctl(tmpSock, SIOCGIFFLAGS, &ifr) == 0)
                state = (ifr.ifr_flags & IFF_UP) ? QStringLiteral("UP") : QStringLiteral("DOWN");
            close(tmpSock);
        }
#else
#if !defined(PLATFORM_ANDROID)
        QFile mtuFile(QStringLiteral("/sys/class/net/%1/mtu").arg(info.name));
        if (mtuFile.open(QIODevice::ReadOnly)) mtu = QString::fromLatin1(mtuFile.readAll().trimmed());
        QFile stateFile(QStringLiteral("/sys/class/net/%1/operstate").arg(info.name));
        if (stateFile.open(QIODevice::ReadOnly)) state = QString::fromLatin1(stateFile.readAll().trimmed()).toUpper();
#endif
        if (isLoopback) state = QStringLiteral("UP");
#endif // __APPLE__

        // 5WHY: netRows.append() was inside the #else block (Linux only).
        // macOS correctly reads MTU/state via ioctl in the __APPLE__ block
        // above, but the row was never appended — resulting in an empty
        // "No network adapters found" table on macOS.
        QString mac = info.mac.isEmpty() ? QStringLiteral("-") : info.mac;
        QString ip4 = info.ips.isEmpty() ? (isLoopback ? QStringLiteral("127.0.0.1") : QStringLiteral("-")) : info.ips.join(',');

        netRows.append({info.name, mtu, state, mac, ip4});
    }
    out.append(DiagnosticFormatter::formatTable(kNetCols, netRows));
#endif // PLATFORM_IOS
#endif // _WIN32

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = (netRows.size() > 0)
        ? QStringLiteral("%1 network adapter%2 enumerated").arg(netRows.size()).arg(netRows.size() > 1 ? "s" : "")
        : QStringLiteral("No network adapters found");
    r.durationMs = t.elapsed();
    return r;
}


// G1  - Active Connections (netstat -an format)

}
