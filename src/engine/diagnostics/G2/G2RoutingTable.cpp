#include "engine/diagnostics/GHelpers.h"
namespace G1G2G3Native {
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
                QString::number(row.Metric)});
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
#elif defined(__APPLE__) && !defined(PLATFORM_IOS)
    // ── macOS: enumerate routing table via sysctl NET_RT_DUMP ──
    {
        int mib[] = { CTL_NET, PF_ROUTE, 0, 0, NET_RT_DUMP, 0 };
        size_t needed = 0;
        if (sysctl(mib, 6, nullptr, &needed, nullptr, 0) == 0 && needed > 0) {
            QByteArray rtbuf((int)needed + 4096, '\0');
            if (sysctl(mib, 6, rtbuf.data(), &needed, nullptr, 0) == 0) {
                char* ptr = rtbuf.data(); char* end = ptr + needed;
                while (ptr + (int)sizeof(struct rt_msghdr) <= end) {
                    auto* rtm = (struct rt_msghdr*)ptr;
                    if (rtm->rtm_version != RTM_VERSION || rtm->rtm_msglen < sizeof(struct rt_msghdr))
                        break;
                    // Parse sockaddrs: [dst, gw, netmask, ...]
                    struct sockaddr* sa = (struct sockaddr*)(rtm + 1);
                    QString dst = QStringLiteral("-"), gw = QStringLiteral("-"),
                            mask = QStringLiteral("-"), ifName = QStringLiteral("-");
                    for (int i = 0; i < RTAX_MAX && sa->sa_len > 0; i++) {
                        if (rtm->rtm_addrs & (1 << i)) {
                            if (i == RTAX_DST && sa->sa_family == AF_INET)
                                dst = ip4ToStr(((struct sockaddr_in*)sa)->sin_addr);
                            else if (i == RTAX_GATEWAY && sa->sa_family == AF_INET)
                                gw = ip4ToStr(((struct sockaddr_in*)sa)->sin_addr);
                            else if (i == RTAX_NETMASK && sa->sa_family == AF_INET)
                                mask = ip4ToStr(((struct sockaddr_in*)sa)->sin_addr);
                            else if (i == RTAX_IFP && sa->sa_family == AF_LINK) {
                                auto* sdl = (struct sockaddr_dl*)sa;
                                ifName = QString::fromLatin1(sdl->sdl_data, sdl->sdl_nlen);
                            }
                            sa = (struct sockaddr*)((char*)sa + sa->sa_len);
                        }
                    }
                    if (!dst.isEmpty() && dst != QStringLiteral("-"))
                        routeRows.append({dst, mask, gw, ifName.left(9), QString::number(rtm->rtm_index)});
                    ptr += rtm->rtm_msglen;
                    if (rtm->rtm_msglen == 0) break; // safety
                }
            }
        }
        if (!routeRows.isEmpty())
            out.append(DiagnosticFormatter::formatTable(kRouteCols, routeRows));
    }
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
}
