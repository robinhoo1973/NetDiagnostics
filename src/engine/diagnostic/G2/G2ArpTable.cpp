#include "engine/diagnostic/GHelpers.h"
namespace G1G2G3Native {
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
    // Common ARP table columns (shared by Linux and macOS)
    static const QVector<DiagnosticFormatter::ColSpec> kArpCols = {
        {"Internet Address",  24, true},
        {"Physical Address",  23, true},
        {"Type",               0, false},
    };
    QList<QStringList> arpRows;
    // Linux: parse /proc/net/arp
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID) && !defined(__APPLE__)
    QFile arpFile(QStringLiteral("/proc/net/arp"));
    if (arpFile.open(QIODevice::ReadOnly)) {
        QTextStream ts(&arpFile);
        QString header = ts.readLine(); // skip header
        out.append(QStringLiteral("Interface: (all)"));

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
#elif defined(__APPLE__) && !defined(PLATFORM_IOS)
    // ── macOS: ARP cache via sysctl NET_RT_FLAGS RTF_LLINFO ──
    {
        int mib[] = { CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_LLINFO };
        size_t needed = 0;
        if (sysctl(mib, 6, nullptr, &needed, nullptr, 0) == 0 && needed > 0) {
            QByteArray arpBuf((int)needed + 4096, '\0');
            if (sysctl(mib, 6, arpBuf.data(), &needed, nullptr, 0) == 0) {
                char* ptr = arpBuf.data(); char* end = ptr + needed;
                while (ptr + (int)sizeof(struct rt_msghdr) <= end) {
                    auto* rtm = (struct rt_msghdr*)ptr;
                    if (rtm->rtm_version != RTM_VERSION || rtm->rtm_msglen < sizeof(struct rt_msghdr))
                        break;
                    struct sockaddr* sa = (struct sockaddr*)(rtm + 1);
                    QString ip, mac;
                    for (int i = 0; i < RTAX_MAX && sa->sa_len > 0; i++) {
                        if (rtm->rtm_addrs & (1 << i)) {
                            if (i == RTAX_DST && sa->sa_family == AF_INET)
                                ip = ip4ToStr(((struct sockaddr_in*)sa)->sin_addr);
                            else if (i == RTAX_GATEWAY && sa->sa_family == AF_LINK) {
                                auto* sdl = (struct sockaddr_dl*)sa;
                                if (sdl->sdl_alen == 6)
                                    mac = macToStr((const unsigned char*)LLADDR(sdl));
                            }
                            sa = (struct sockaddr*)((char*)sa + sa->sa_len);
                        }
                    }
                    if (!ip.isEmpty() && !mac.isEmpty())
                        arpRows.append({ip, mac, QStringLiteral("dynamic")});
                    ptr += rtm->rtm_msglen;
                    if (rtm->rtm_msglen == 0) break;
                }
            }
        }
        if (!arpRows.isEmpty())
            out.append(QStringLiteral("  ") + DiagnosticFormatter::formatTable(kArpCols, arpRows).join(QStringLiteral("\n  ")));
        else
            out.append(QStringLiteral("  (no ARP entries found)"));
    }
#else  // PLATFORM_IOS
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
#ifdef PLATFORM_IOS
    r.status = DiagStatus::Skipped;
    r.summary = QStringLiteral("Unavailable on iOS (no public ARP API)");
#elif defined(__APPLE__)
    r.status = arpRows.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass;
    r.summary = arpRows.isEmpty()
        ? QStringLiteral("No ARP entries found")
        : QStringLiteral("ARP table collected via sysctl");
#else
    r.status = DiagStatus::Pass;
    r.summary = QStringLiteral("ARP table collected");
#endif
#endif
    r.durationMs = t.elapsed();
    return r;
}

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// Remaining G2/G3 stubs with native implementations
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?

}
