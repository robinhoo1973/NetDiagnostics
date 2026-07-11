#include "Diagnostics/Model/GHelpers.h"
namespace G1G2G3Native {
DiagnosticResult arpTable(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.status = DiagStatus::Info;  // 5WHY: cppcheck found uninitialized r.status
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

    out.append(QString());

#if defined(_WIN32)
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
    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status   = out.size() > 3 ? DiagStatus::Pass : DiagStatus::Warning;
    r.summary  = out.size() > 3
        ? QStringLiteral("ARP table collected via GetIpNetTable2")
        : QStringLiteral("No ARP entries found");
#else
    // Common ARP table columns (shared by Linux and macOS)
    static const QVector<DiagnosticFormatter::ColSpec> kArpCols = {
        {"Internet Address",  24, false},  // IP identifier
        {"Physical Address",  23, false},  // MAC identifier
        {"Type",               0, false},  // text
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
#else
#if defined(__APPLE__) && !defined(PLATFORM_IOS)
    // 鈹€鈹€ macOS: ARP cache via sysctl NET_RT_FLAGS RTF_LLINFO 鈹€鈹€
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
                    struct sockaddr* sa = reinterpret_cast<struct sockaddr*>(rtm + 1);
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
                            sa = reinterpret_cast<struct sockaddr*>((char*)sa + sa->sa_len);
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
#if defined(PLATFORM_IOS)
    r.status = DiagStatus::Skipped;
    r.summary = QStringLiteral("Unavailable on iOS (no public ARP API)");
#else
#if defined(__APPLE__)
    r.status = arpRows.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass;
    r.summary = arpRows.isEmpty()
        ? QStringLiteral("No ARP entries found")
        : QStringLiteral("ARP table collected via sysctl");
#else
    r.status = DiagStatus::Pass;
    r.summary = QStringLiteral("ARP table collected");
#endif
#endif
#endif  // close converted #elif
#endif  // close converted #elif
    r.durationMs = t.elapsed();
    return r;
}

// 闂佸磭鍎ら崝蹇涘疾閺屻儱鐓涢柟鑸妽濞呮粓鏌嶉悜妯哄闁哄懏鐓￠崺锟犲箛閵婏附鐝抽梺宕囧劋閸斿繘寮查弻銉ョ厸闁硅埇鍔嶅▍婊堟煃閻戞ê濮€闁哄懏鐓￠崺锟犲箛閵婏附鐝抽梺宕囧劋閸斿繘寮查弻銉ョ厸闁硅埇鍔嶅▍婊堟煃閻戞ê濮€闁哄懏鐓￠崺锟犲箛閵婏附鐝抽梺宕囧劋閸斿繘寮查弻銉ョ厸闁硅埇鍔嶅▍婊堟煃閻戞ê濮€闁哄懏鐓￠崺锟犲箛閵婏附鐝抽梺宕囧劋閸斿繘寮查弻銉ョ厸闁硅埇鍔嶅▍婊堟煃閻戞ê濮€闁哄懏鐓￠崺锟犲箛閵婏附鐝抽梺宕囧劋閸斿繘寮查弻銉ョ厸闁硅埇鍔嶅▍婊堟煃閻戞ê濮€闁哄懏鐓￠崺锟犲箛閵婏附鐝抽梺宕囧劋閸斿繘寮查弻銉ョ厸闁硅埇鍔嶅▍婊堟煃閻戞ê濮€闁哄懏鐓￠崺锟犲箛閵婏附鐝抽梺宕囧劋閸斿繘寮查弻銉ョ厸闁硅埇鍔嶅▍婊堟煃閻戞ê濮€闁哄懏鐓￠崺锟犲箛閵婏附鐝抽梺宕囧劋閸斿繘寮查弻銉ョ厸闁硅埇鍔嶅▍婊堟煃閻戞ê濮€闁哄懏鐓￠崺锟犲箛閵婏附鐝抽梺宕囧劋閸斿繘寮查弻銉ョ厸闁硅埇鍔嶅▍婊堟煃?
// Remaining G2/G3 stubs with native implementations
// 闂佸磭鍎ら崝蹇涘疾閺屻儱鐓涢柟鑸妽濞呮粓鏌嶉悜妯哄闁哄懏鐓￠崺锟犲箛閵婏附鐝抽梺宕囧劋閸斿繘寮查弻銉ョ厸闁硅埇鍔嶅▍婊堟煃閻戞ê濮€闁哄懏鐓￠崺锟犲箛閵婏附鐝抽梺宕囧劋閸斿繘寮查弻銉ョ厸闁硅埇鍔嶅▍婊堟煃閻戞ê濮€闁哄懏鐓￠崺锟犲箛閵婏附鐝抽梺宕囧劋閸斿繘寮查弻銉ョ厸闁硅埇鍔嶅▍婊堟煃閻戞ê濮€闁哄懏鐓￠崺锟犲箛閵婏附鐝抽梺宕囧劋閸斿繘寮查弻銉ョ厸闁硅埇鍔嶅▍婊堟煃閻戞ê濮€闁哄懏鐓￠崺锟犲箛閵婏附鐝抽梺宕囧劋閸斿繘寮查弻銉ョ厸闁硅埇鍔嶅▍婊堟煃閻戞ê濮€闁哄懏鐓￠崺锟犲箛閵婏附鐝抽梺宕囧劋閸斿繘寮查弻銉ョ厸闁硅埇鍔嶅▍婊堟煃閻戞ê濮€闁哄懏鐓￠崺锟犲箛閵婏附鐝抽梺宕囧劋閸斿繘寮查弻銉ョ厸闁硅埇鍔嶅▍婊堟煃閻戞ê濮€闁哄懏鐓￠崺锟犲箛閵婏附鐝抽梺宕囧劋閸斿繘寮查弻銉ョ厸闁硅埇鍔嶅▍婊堟煃閻戞ê濮€闁哄懏鐓￠崺锟犲箛閵婏附鐝抽梺宕囧劋閸斿繘寮查弻銉ョ厸闁硅埇鍔嶅▍婊堟煃?

}
