#include "Diagnostics/Model/GHelpers.h"
namespace G1G2G3Native {
DiagnosticResult defaultGateway(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

    out.append(QString());
    out.append(QStringLiteral("Default Gateway:"));
    out.append(QString());

    QString defaultGw = QStringLiteral("Not found");
#if defined(_WIN32)
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
    // 5WHY: GetIpForwardTable2 (Vista+) may fail on older Windows or with
    // restricted privileges.  GetBestRoute is available on XP+ and uses a
    // simpler API — it returns the best route to a given destination.
    // Use 223.5.5.5 (AliDNS / Alibaba Cloud) — globally accessible, not
    // blocked by GFW, and always routed via the default gateway.
    if (defaultGw == QStringLiteral("Not found")) {
        MIB_IPFORWARDROW bestRoute = {};
        bestRoute.dwForwardDest = inet_addr("223.5.5.5");
        bestRoute.dwForwardProto = MIB_IPPROTO_NETMGMT;
        if (GetBestRoute(0, 0, &bestRoute) == NO_ERROR && bestRoute.dwForwardNextHop != 0) {
            struct in_addr gw; gw.S_un.S_addr = bestRoute.dwForwardNextHop;
            defaultGw = ip4ToStr(gw);
            out.append(QStringLiteral("  Default Gateway (via GetBestRoute): %1").arg(defaultGw));
        }
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
#else
#if defined(__APPLE__) && !defined(PLATFORM_IOS)
    // 鈹€鈹€ macOS: get default gateway via PF_ROUTE routing socket 鈹€鈹€
    int routeSock = socket(PF_ROUTE, SOCK_RAW, 0);
    if (routeSock >= 0) {
        struct { struct rt_msghdr h; struct sockaddr_in d; } msg;
        memset(&msg, 0, sizeof(msg));
        msg.h.rtm_msglen = sizeof(msg);
        msg.h.rtm_version = RTM_VERSION;
        msg.h.rtm_type = RTM_GET;
        msg.h.rtm_flags = RTF_GATEWAY;
        msg.h.rtm_addrs = RTA_DST;
        msg.d.sin_len = sizeof(struct sockaddr_in);
        msg.d.sin_family = AF_INET;
        if (write(routeSock, &msg, msg.h.rtm_msglen) >= 0) {
            char resp[512];
            ssize_t n = read(routeSock, resp, sizeof(resp));
            if (n > (ssize_t)sizeof(struct rt_msghdr)) {
                auto* rh = (struct rt_msghdr*)resp;
                auto* sa = reinterpret_cast<struct sockaddr*>(rh + 1);
                for (int i = 0; i < RTAX_MAX; i++) {
                    if (rh->rtm_addrs & (1 << i)) {
                        if (i == RTAX_GATEWAY && sa->sa_family == AF_INET) {
                            defaultGw = ip4ToStr(((struct sockaddr_in*)sa)->sin_addr);
                            out.append(QStringLiteral("  Default Gateway . . . . . . . . . : %1").arg(defaultGw));
                            break;
                        }
                        if (sa->sa_len > 0)
                            sa = reinterpret_cast<struct sockaddr*>((char*)sa + sa->sa_len);
                        else break;
                    }
                }
            }
        }
        close(routeSock);
    }
#else  // PLATFORM_IOS
#endif
#endif
    if (defaultGw == QStringLiteral("Not found"))
        out.append(QStringLiteral("  No default gateway configured"));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
#if defined(__APPLE__) && !defined(PLATFORM_IOS)
    r.status = (defaultGw != QStringLiteral("Not found")) ? DiagStatus::Pass : DiagStatus::Warning;
    r.summary = (defaultGw != QStringLiteral("Not found"))
        ? QStringLiteral("Default gateway: %1").arg(defaultGw)
        : QStringLiteral("No default gateway (routing socket query returned empty)");
#else
    r.status = (defaultGw != QStringLiteral("Not found")) ? DiagStatus::Pass : DiagStatus::Warning;
    r.summary = (defaultGw != QStringLiteral("Not found"))
        ? QStringLiteral("Default gateway: %1").arg(defaultGw)
        : QStringLiteral("No default gateway");
#endif
#endif  // close converted #elif
    r.durationMs = t.elapsed();
    return r;
}

}
