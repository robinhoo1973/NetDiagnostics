// =============================================================================
// G2/Adapters.cpp — G2 Connectivity & Security adapters (real implementations)
//
// Ported from the archived G2*.cpp behavioral code into the new adapter
// structure (RunContext signature, per-test contract).  Platform branches:
//   Windows: IP Helper API + registry + PowerShell（防火墙/连接配置）
//   Linux:   /proc/net/{route,arp} + /proc/sys/net
//   macOS:   sysctl NET_RT_DUMP / RTF_LLINFO
// Platforms per NEW-1: NetworkProfile/DefaultGateway/RoutingTable=All;
// TcpSettings/ArpTable/ProxySettings=Desktop|Android（Android 走归档 JNI 路径待迁移）
// =============================================================================
#if defined(_WIN32)
// 必须最先定义（Qt 头会先拉入 windows.h）：
// 1) WINAPI_FAMILY=DESKTOP_APP——mingw 的 winapifamily.h 默认 WINAPI_FAMILY_APP，
//    会使 netioapi.h 的 WINAPI_PARTITION_DESKTOP 块（GetIpForwardTable2 等）被整体排除；
// 2) _WIN32_WINNT=Vista+——IP Helper 声明的最低版本要求。
#if !defined(WINAPI_FAMILY)
#define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP
#endif
#if !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0601
#endif
#endif

#include "Common/Services/PlatformAdapter.h"
#include "Common/Model/DiagnosticMeta.h"
#include "Common/Model/DiagNames.h"

#if defined(PLATFORM_ANDROID)
#include "Diagnostics/Model/G5/Platform/Android/NetworkDiagnostics.h"
#endif
#if defined(PLATFORM_IOS)
#include "Diagnostics/Model/G1/Platform/IOS/GatewayDhcpRouting.h"
#endif

#include <QNetworkInterface>
#include <QHostAddress>
#include <QProcess>
#include <QFile>
#include <QTextStream>
#include <QHostInfo>
#include <QElapsedTimer>
#include <QRegularExpression>

#if defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
// 经典 MinGW 陷阱：netioapi.h 必须先于 iphlpapi.h——若 __IPHLPAPI_H__ 已定义，
// netioapi.h 走 #ifdef 分支而跳过 iprtrmib.h 的包含 → MIB_IPFORWARD_TABLE2 等类型缺失。
#include <netioapi.h>
#include <iphlpapi.h>
#include <winreg.h>
#endif
#if defined(__APPLE__) && !defined(PLATFORM_IOS)
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <arpa/inet.h>
#endif

using namespace PlatformFlag;

namespace g2 {

// ── Helpers ────────────────────────────────────────────────────────────────
static DiagnosticResult makeResult(DiagId id, DiagStatus status,
                                   const QString& summary,
                                   const QVector<ResultProperty>& props,
                                   const QString& details) {
    DiagnosticResult r;
    r.id = id; r.displayName = diagDisplayName(id); r.group = diagGroup(id);
    r.status = status; r.summary = summary; r.properties = props;
    r.details = details; r.rawOutput = details;
    r.timestamp = QDateTime::currentDateTime();
    return r;
}

static QString macToStr(const unsigned char* mac, int len = 6) {
    QStringList parts;
    for (int i = 0; i < len; ++i)
        parts << QStringLiteral("%1").arg(mac[i], 2, 16, QLatin1Char('0')).toUpper();
    return parts.join(QLatin1Char('-'));
}

// /proc/net/route 格式：目标/掩码/网关以小端十六进制存储 → 转点分十进制
static QString ipToStr(uint32_t littleEndianHex) {
    const quint32 b0 = littleEndianHex & 0xff;
    const quint32 b1 = (littleEndianHex >> 8) & 0xff;
    const quint32 b2 = (littleEndianHex >> 16) & 0xff;
    const quint32 b3 = (littleEndianHex >> 24) & 0xff;
    return QStringLiteral("%1.%2.%3.%4").arg(b0).arg(b1).arg(b2).arg(b3);
}

#if defined(_WIN32)
static QString ip4ToStr(const in_addr& a) { return QString::fromLatin1(inet_ntoa(a)); }
#endif
#if defined(__APPLE__) && !defined(PLATFORM_IOS)
static QString ip4ToStr(const struct in_addr& a) { return QString::fromLatin1(inet_ntoa(a)); }
#endif

// ── G2RoutingTable ─────────────────────────────────────────────────────────
static DiagnosticResult probeRoutingTable(DiagId id, const QString&, RunContext& ctx) {
    QVector<ResultProperty> props;
    QStringList out;
    out.append(QStringLiteral("Active Routes:"));
    int routeCount = 0;

#if defined(_WIN32)
    PMIB_IPFORWARD_TABLE2 ft = nullptr;
    if (GetIpForwardTable2(AF_INET, &ft) == NO_ERROR && ft) {
        for (ULONG i = 0; i < ft->NumEntries; ++i) {
            if (ctx.cancelled.load()) { HeapFree(GetProcessHeap(), 0, ft); return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled")); }
            const auto& row = ft->Table[i];
            const auto& dest = row.DestinationPrefix.Prefix.Ipv4.sin_addr;
            const auto& gw   = row.NextHop.Ipv4.sin_addr;
            const int prefixLen = row.DestinationPrefix.PrefixLength;
            const uint32_t maskVal = (prefixLen == 0) ? 0 : (~0u << (32 - prefixLen));
            in_addr mask; mask.S_un.S_addr = htonl(maskVal);
            MIB_IFROW ifRow; ZeroMemory(&ifRow, sizeof(ifRow));
            ifRow.dwIndex = row.InterfaceIndex;
            QString ifName = QString::number(row.InterfaceIndex);
            if (GetIfEntry(&ifRow) == NO_ERROR) ifName = QString::fromWCharArray(ifRow.wszName);
            props.append({ip4ToStr(dest), QStringLiteral("gw=%1 if=%2 metric=%3")
                          .arg(ip4ToStr(gw), ifName).arg(row.Metric)});
            out.append(QStringLiteral("  %1 → %2 (%3)").arg(ip4ToStr(dest), ip4ToStr(gw), ifName));
            ++routeCount;
        }
        HeapFree(GetProcessHeap(), 0, ft);
    }
#else
#if defined(__APPLE__) && !defined(PLATFORM_IOS)
    int mib[] = { CTL_NET, PF_ROUTE, 0, 0, NET_RT_DUMP, 0 };
    size_t needed = 0;
    if (sysctl(mib, 6, nullptr, &needed, nullptr, 0) == 0 && needed > 0) {
        QByteArray rtbuf((int)needed + 4096, '\0');
        if (sysctl(mib, 6, rtbuf.data(), &needed, nullptr, 0) == 0) {
            char* ptr = rtbuf.data(); const char* end = ptr + needed;
            while (ptr + (int)sizeof(struct rt_msghdr) <= end) {
                if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
                auto* rtm = (struct rt_msghdr*)ptr;
                if (rtm->rtm_version != RTM_VERSION || rtm->rtm_msglen == 0) break;
                if (rtm->rtm_type == RTM_GET && rtm->rtm_addrs & RTA_DST) {
                    auto* sa = (struct sockaddr*)(rtm + 1);
                    if (sa->sa_family == AF_INET) {
                        auto* dst = &((struct sockaddr_in*)sa)->sin_addr;
                        auto* gwsa = (struct sockaddr*)((char*)sa + sa->sa_len);
                        QString gw = (rtm->rtm_addrs & RTA_GATEWAY && gwsa->sa_family == AF_INET)
                                   ? ip4ToStr(((struct sockaddr_in*)gwsa)->sin_addr) : QStringLiteral("On-link");
                        props.append({ip4ToStr(*dst), QStringLiteral("gw=%1").arg(gw)});
                        out.append(QStringLiteral("  %1 → %2").arg(ip4ToStr(*dst), gw));
                        ++routeCount;
                    }
                }
                ptr += rtm->rtm_msglen;
            }
        }
    }
#else // Linux / Android / iOS
    QFile routeFile(QStringLiteral("/proc/net/route"));
    if (routeFile.open(QIODevice::ReadOnly)) {
        QTextStream ts(&routeFile);
        ts.readLine(); // header
        while (!ts.atEnd()) {
            if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
            const QString line = ts.readLine().trimmed();
            if (line.isEmpty()) continue;
            const QStringList cols = line.split(QLatin1Char('\t'));
            if (cols.size() >= 8) {
                const QString ifName = cols[0];
                bool ok = false;
                const uint32_t dest   = cols[1].toUInt(&ok, 16);
                const uint32_t gw     = cols[2].toUInt(&ok, 16);
                const uint32_t mask   = cols[7].toUInt(&ok, 16);
                const QString gwStr = gw ? ipToStr(gw) : QStringLiteral("On-link");
                props.append({ipToStr(dest), QStringLiteral("netmask=%1 gw=%2 if=%3").arg(ipToStr(mask), gwStr, ifName)});
                out.append(QStringLiteral("  %1 → %2 (%3)").arg(ipToStr(dest), gwStr, ifName));
                ++routeCount;
            }
        }
    }
#endif
#endif

    if (routeCount == 0)
        return makeResult(id, DiagStatus::Warning, QStringLiteral("No route entries found"), props, out.join('\n'));
    DiagnosticResult r = makeResult(id, DiagStatus::Pass,
        QStringLiteral("%1 route(s)").arg(routeCount), props, out.join('\n'));
    r.data[QStringLiteral("routeCount")] = routeCount;
    return r;
}

// ── G2ArpTable ─────────────────────────────────────────────────────────────
static DiagnosticResult probeArpTable(DiagId id, const QString&, RunContext& ctx) {
    QVector<ResultProperty> props;
    QStringList out;
    int entryCount = 0;

#if defined(_WIN32)
    PMIB_IPNET_TABLE2 table = nullptr;
    if (GetIpNetTable2(AF_INET, &table) == NO_ERROR && table) {
        for (ULONG i = 0; i < table->NumEntries; ++i) {
            if (ctx.cancelled.load()) { HeapFree(GetProcessHeap(), 0, table); return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled")); }
            const auto& row = table->Table[i];
            const auto ip = ip4ToStr(row.Address.Ipv4.sin_addr);
            const QString mac = macToStr(reinterpret_cast<const unsigned char*>(&row.PhysicalAddress), row.PhysicalAddressLength);
            props.append({ip, mac});
            out.append(QStringLiteral("  %1  %2").arg(ip, -16).arg(mac));
            ++entryCount;
        }
        HeapFree(GetProcessHeap(), 0, table);
    }
#else
#if defined(__APPLE__) && !defined(PLATFORM_IOS)
    int mib[] = { CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_LLINFO };
    size_t needed = 0;
    if (sysctl(mib, 6, nullptr, &needed, nullptr, 0) == 0 && needed > 0) {
        QByteArray buf((int)needed + 4096, '\0');
        if (sysctl(mib, 6, buf.data(), &needed, nullptr, 0) == 0) {
            char* ptr = buf.data(); const char* end = ptr + needed;
            while (ptr + (int)sizeof(struct rt_msghdr) <= end) {
                if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
                auto* rtm = (struct rt_msghdr*)ptr;
                if (rtm->rtm_version != RTM_VERSION || rtm->rtm_msglen < sizeof(struct rt_msghdr)) break;
                auto* sa = (struct sockaddr*)(rtm + 1);
                QString ip, mac;
                for (int i = 0; i < RTAX_MAX && sa->sa_len > 0; ++i) {
                    if (rtm->rtm_addrs & (1 << i)) {
                        if (i == RTAX_DST && sa->sa_family == AF_INET)
                            ip = ip4ToStr(((struct sockaddr_in*)sa)->sin_addr);
                        else if (i == RTAX_GATEWAY && sa->sa_family == AF_LINK) {
                            auto* sdl = (struct sockaddr_dl*)sa;
                            if (sdl->sdl_alen == 6) mac = macToStr(reinterpret_cast<const unsigned char*>(LLADDR(sdl)));
                        }
                        sa = (struct sockaddr*)((char*)sa + sa->sa_len);
                    }
                }
                if (!ip.isEmpty() && !mac.isEmpty()) {
                    props.append({ip, mac});
                    out.append(QStringLiteral("  %1  %2").arg(ip, -16).arg(mac));
                    ++entryCount;
                }
                ptr += rtm->rtm_msglen;
            }
        }
    }
#else // Linux / Android
    QFile arpFile(QStringLiteral("/proc/net/arp"));
    if (arpFile.open(QIODevice::ReadOnly)) {
        QTextStream ts(&arpFile);
        ts.readLine(); // header
        while (!ts.atEnd()) {
            if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
            const QString line = ts.readLine().trimmed();
            if (line.isEmpty()) continue;
            const QStringList cols = line.split(QRegularExpression(QStringLiteral("\\s+")));
            if (cols.size() >= 4 && cols[3] != QStringLiteral("00:00:00:00:00:00")) {
                props.append({cols[0], cols[3]});
                out.append(QStringLiteral("  %1  %2").arg(cols[0], -16).arg(cols[3]));
                ++entryCount;
            }
        }
    }
#endif
#endif

    if (entryCount == 0)
        return makeResult(id, DiagStatus::Warning, QStringLiteral("No ARP entries found"), props, out.join('\n'));
    DiagnosticResult r = makeResult(id, DiagStatus::Pass,
        QStringLiteral("%1 ARP entr(y/ies)").arg(entryCount), props, out.join('\n'));
    r.data[QStringLiteral("entryCount")] = entryCount;
    return r;
}

// ── G2DefaultGateway ───────────────────────────────────────────────────────
static DiagnosticResult probeDefaultGateway(DiagId id, const QString&, RunContext& ctx) {
    // 默认路由 = 目标 0.0.0.0/0 的网关；复用路由表逻辑。
    QVector<ResultProperty> props;
    QStringList found;

#if defined(_WIN32)
    PMIB_IPFORWARD_TABLE2 ft = nullptr;
    if (GetIpForwardTable2(AF_INET, &ft) == NO_ERROR && ft) {
        for (ULONG i = 0; i < ft->NumEntries; ++i) {
            if (ctx.cancelled.load()) { HeapFree(GetProcessHeap(), 0, ft); return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled")); }
            const auto& row = ft->Table[i];
            if (row.DestinationPrefix.PrefixLength != 0) continue;   // default route
            if (row.DestinationPrefix.Prefix.Ipv4.sin_addr.S_un.S_addr != 0) continue;
            MIB_IFROW ifRow; ZeroMemory(&ifRow, sizeof(ifRow));
            ifRow.dwIndex = row.InterfaceIndex;
            QString ifName = QString::number(row.InterfaceIndex);
            if (GetIfEntry(&ifRow) == NO_ERROR) ifName = QString::fromWCharArray(ifRow.wszName);
            const QString gw = ip4ToStr(row.NextHop.Ipv4.sin_addr);
            props.append({gw, QStringLiteral("interface=%1").arg(ifName)});
            found.append(QStringLiteral("%1 via %2").arg(gw, ifName));
        }
        HeapFree(GetProcessHeap(), 0, ft);
    }
#else
#if defined(__APPLE__) && !defined(PLATFORM_IOS)
    // M3：macOS 无 /proc/net/route——PF_ROUTE sysctl NET_RT_DUMP 查默认路由
    // （0.0.0.0/0 + RTF_GATEWAY），与归档 G2DefaultGateway 的 RTM_GET 实现同源。
    int mib[] = { CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_DUMP, 0 };
    size_t needed = 0;
    if (sysctl(mib, 6, nullptr, &needed, nullptr, 0) == 0 && needed > 0) {
        QByteArray buf((int)needed + 4096, '\0');
        if (sysctl(mib, 6, buf.data(), &needed, nullptr, 0) == 0) {
            char* ptr = buf.data(); const char* end = ptr + needed;
            while (ptr + (int)sizeof(struct rt_msghdr) <= end) {
                if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
                auto* rtm = (struct rt_msghdr*)ptr;
                if (rtm->rtm_version != RTM_VERSION || rtm->rtm_msglen < sizeof(struct rt_msghdr)) break;
                const bool isGateway = (rtm->rtm_flags & RTF_GATEWAY) != 0;
                QString destIp, gwIp;
                auto* sa = (struct sockaddr*)(rtm + 1);
                for (int i = 0; i < RTAX_MAX; ++i) {
                    if (!(rtm->rtm_addrs & (1 << i))) continue;
                    if (sa->sa_len <= 0) break;
                    if (i == RTAX_DST && sa->sa_family == AF_INET)
                        destIp = ip4ToStr(((struct sockaddr_in*)sa)->sin_addr);
                    else if (i == RTAX_GATEWAY && sa->sa_family == AF_INET)
                        gwIp = ip4ToStr(((struct sockaddr_in*)sa)->sin_addr);
                    sa = (struct sockaddr*)((char*)sa + sa->sa_len);
                }
                if (isGateway && destIp == QLatin1String("0.0.0.0") && !gwIp.isEmpty()) {
                    props.append({gwIp, QStringLiteral("interface index=%1").arg(rtm->rtm_index)});
                    found.append(QStringLiteral("%1 via ifindex %2").arg(gwIp).arg(rtm->rtm_index));
                }
                ptr += rtm->rtm_msglen;
            }
        }
    }
#else
    // Linux / Android
    QFile routeFile(QStringLiteral("/proc/net/route"));
    if (routeFile.open(QIODevice::ReadOnly)) {
        QTextStream ts(&routeFile);
        ts.readLine();
        while (!ts.atEnd()) {
            if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
            const QString line = ts.readLine().trimmed();
            if (line.isEmpty()) continue;
            const QStringList cols = line.split(QLatin1Char('\t'));
            if (cols.size() >= 8) {
                bool ok = false;
                const uint32_t dest = cols[1].toUInt(&ok, 16);
                const uint32_t gw   = cols[2].toUInt(&ok, 16);
                if (dest == 0 && gw != 0) {
                    // 5WHY (2026-08-20 用户诉求 "Default Gateway 无属性卡"):
                    // 曾丢失 metric（cols[6]）且只查 IPv4——路由选择依赖
                    // metric，双栈网络默认走 IPv6 时属性卡为空。补 metric
                    // 与 IPv6 默认路由（/proc/net/ipv6_route）。
                    const uint32_t metric = cols.size() >= 7 ? cols[6].toUInt(nullptr, 16) : 0;
                    props.append({ipToStr(gw),
                        QStringLiteral("via %1, metric %2").arg(cols[0]).arg(metric)});
                    found.append(QStringLiteral("%1 (via %2, metric %3)")
                        .arg(ipToStr(gw), cols[0]).arg(metric));
                }
            }
        }
    }
    QFile v6File(QStringLiteral("/proc/net/ipv6_route"));
    if (v6File.open(QIODevice::ReadOnly)) {
        QTextStream ts(&v6File);
        while (!ts.atEnd()) {
            if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
            const QString line = ts.readLine().trimmed();
            if (line.isEmpty()) continue;
            const QStringList cols = line.split(QRegularExpression(QStringLiteral("\\s+")));
            if (cols.size() >= 10) {
                bool ok = false;
                const int prefix = cols[1].toInt(&ok, 16);
                if (!ok || prefix != 0) continue;   // 仅默认路由
                const QByteArray gwBytes = QByteArray::fromHex(cols[4].toLatin1());
                if (gwBytes.size() != 16) continue;
                bool allZero = true;
                for (const char b : gwBytes) { if (b != 0) { allZero = false; break; } }
                if (allZero) continue;
                QHostAddress gw6;
                gw6.setAddress(reinterpret_cast<const quint8*>(gwBytes.constData()));
                const quint32 metric = cols[5].toUInt(nullptr, 16);
                props.append({gw6.toString(),
                    QStringLiteral("via %1, metric %2 (IPv6)").arg(cols[9]).arg(metric)});
                found.append(QStringLiteral("%1 (via %2, metric %3, IPv6)")
                    .arg(gw6.toString(), cols[9]).arg(metric));
            }
        }
    }
#endif
#endif

    // ipconfig 风格终端转储（v0.0.3 对等：Default Gateway . . . : 行）
    QStringList out;
    out.append(QStringLiteral("Default Gateway:"));
    if (found.isEmpty()) {
        out.append(QStringLiteral("  No default gateway configured"));
        DiagnosticResult r = makeResult(id, DiagStatus::Warning,
            QStringLiteral("No default gateway found"), props, out.join(QLatin1Char('\n')));
        r.narrative = QStringLiteral("No default route (0.0.0.0/0 with a gateway) was found — "
            "the device can only reach its local subnet, not external networks.");
        return r;
    }
    for (const auto& f : found)
        out.append(QStringLiteral("  Default Gateway . . . . . . . . . : %1").arg(f));
    DiagnosticResult r = makeResult(id, DiagStatus::Pass,
        QStringLiteral("%1 default gateway(s)").arg(found.size()), props,
        out.join(QLatin1Char('\n')));
    r.narrative = QStringLiteral("%1 default gateway(s) found: %2. "
        "Outbound packets to external networks are routed via the listed gateway/interface pair(s).")
        .arg(found.size()).arg(found.join(QStringLiteral("; ")));
    return r;
}

// ── G2NetworkProfile ───────────────────────────────────────────────────────
static DiagnosticResult probeNetworkProfile(DiagId id, const QString&, RunContext& ctx) {
    QVector<ResultProperty> props;
    QStringList out;
    out.append(QStringLiteral("Network Profile Information:"));

    const QString hostname = QHostInfo::localHostName();
    props.append({QStringLiteral("hostname"), hostname});
    out.append(QStringLiteral("  Hostname: %1").arg(hostname));

#if defined(_WIN32)
    // IP 转发（注册表）
    {
        HKEY hKey = nullptr;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
                0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD val = 0, sz = sizeof(val);
            if (RegQueryValueExA(hKey, "IPEnableRouter", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) {
                const QString fwd = val ? QStringLiteral("Enabled") : QStringLiteral("Disabled");
                props.append({QStringLiteral("ipForwarding"), fwd});
                out.append(QStringLiteral("  IP Forwarding: %1").arg(fwd));
            }
            RegCloseKey(hKey);
        }
    }
    // 防火墙配置文件状态（PowerShell，5s 超时）
    {
        QProcess ps;
        ps.start(QStringLiteral("powershell"), QStringList()
            << QStringLiteral("-NoProfile") << QStringLiteral("-Command")
            << QStringLiteral("Get-NetFirewallProfile | Select Name,Enabled | Format-List"));
        if (ps.waitForFinished(5000)) {
            // PowerShell 标准输出按控制台代码页（中文机 GBK）编码——用本地编码解码
            const QString fwOut = QString::fromLocal8Bit(ps.readAllStandardOutput()).trimmed();
            if (!fwOut.isEmpty()) {
                for (const auto& line : fwOut.split('\n')) {
                    const QString t = line.trimmed();
                    if (!t.isEmpty()) out.append(QStringLiteral("    ") + t);
                }
            }
        } else {
            ps.kill();
            ps.waitForFinished(2000);   // R5-1: 超时后必须回收，否则析构时进程仍在运行
        }
    }
#else
    QFile fwd(QStringLiteral("/proc/sys/net/ipv4/ip_forward"));
    if (fwd.open(QIODevice::ReadOnly)) {
        const QString fw = QString::fromLatin1(fwd.readAll().trimmed()) == QLatin1String("1")
                         ? QStringLiteral("Enabled") : QStringLiteral("Disabled");
        props.append({QStringLiteral("ipForwarding"), fw});
        out.append(QStringLiteral("  IP Forwarding: %1").arg(fw));
    }
#if defined(__APPLE__) && !defined(PLATFORM_IOS)
    int f = 0; size_t fs = sizeof(f);
    if (sysctlbyname("net.inet.ip.forwarding", &f, &fs, nullptr, 0) == 0) {
        props.append({QStringLiteral("ipForwarding"), f ? QStringLiteral("Enabled") : QStringLiteral("Disabled")});
        out.append(QStringLiteral("  IP Forwarding: %1").arg(f ? QStringLiteral("Enabled") : QStringLiteral("Disabled")));
    }
#endif
#endif

    // 活动接口列表
    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const auto& i : ifaces) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        if (!i.flags().testFlag(QNetworkInterface::IsUp)) continue;
        props.append({QStringLiteral("interface"), QStringLiteral("%1 (%2)").arg(i.name(), i.hardwareAddress())});
    }

    return makeResult(id, DiagStatus::Pass, QStringLiteral("Network Profile Collected"), props, out.join('\n'));
}

// ── G2TcpSettings ──────────────────────────────────────────────────────────
static DiagnosticResult probeTcpSettings(DiagId id, const QString&, RunContext& ctx) {
    QVector<ResultProperty> props;
    QStringList out;
    out.append(QStringLiteral("TCP Settings:"));

#if defined(_WIN32)
    const QStringList keys = { QStringLiteral("MaxUserPort"), QStringLiteral("TcpTimedWaitDelay"),
                               QStringLiteral("DefaultTTL"), QStringLiteral("KeepAliveTime") };
    HKEY hKey = nullptr;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
            0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        for (const QString& k : keys) {
            if (ctx.cancelled.load()) { RegCloseKey(hKey); return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled")); }
            DWORD val = 0, sz = sizeof(val);
            if (RegQueryValueExA(hKey, k.toLatin1().constData(), nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS) {
                props.append({k, QString::number(val)});
                out.append(QStringLiteral("  %1 = %2").arg(k).arg(val));
            }
        }
        RegCloseKey(hKey);
    }
#else // Linux / Android
    const QStringList files = { QStringLiteral("/proc/sys/net/ipv4/tcp_fin_timeout"),
                                QStringLiteral("/proc/sys/net/ipv4/tcp_keepalive_time"),
                                QStringLiteral("/proc/sys/net/ipv4/tcp_max_syn_backlog"),
                                QStringLiteral("/proc/sys/net/ipv4/ip_default_ttl") };
    for (const QString& f : files) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        QFile file(f);
        if (file.open(QIODevice::ReadOnly)) {
            const QString val = QString::fromLatin1(file.readAll().trimmed());
            const QString key = f.section(QLatin1Char('/'), -1);
            props.append({key, val});
            out.append(QStringLiteral("  %1 = %2").arg(key, val));
        }
    }
#endif

    if (props.isEmpty())
        return makeResult(id, DiagStatus::Warning, QStringLiteral("TCP settings not accessible"), {}, out.join('\n'));
    return makeResult(id, DiagStatus::Pass,
        QStringLiteral("%1 TCP parameter(s)").arg(props.size()), props, out.join('\n'));
}

// ── G2ProxySettings ────────────────────────────────────────────────────────
static DiagnosticResult probeProxySettings(DiagId id, const QString&, RunContext& ctx) {
    QVector<ResultProperty> props;
    QStringList out;
    bool anyProxy = false;

#if defined(_WIN32)
    // WinINet 注册表（当前用户 Internet Settings）
    {
        HKEY hKey = nullptr;
        if (RegOpenKeyExA(HKEY_CURRENT_USER,
                "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
                0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD enabled = 0, sz = sizeof(enabled);
            if (RegQueryValueExA(hKey, "ProxyEnable", nullptr, nullptr, (LPBYTE)&enabled, &sz) == ERROR_SUCCESS && enabled) {
                char server[512] = {}; DWORD ssz = sizeof(server);
                if (RegQueryValueExA(hKey, "ProxyServer", nullptr, nullptr, (LPBYTE)server, &ssz) == ERROR_SUCCESS) {
                    const QString srv = QString::fromLocal8Bit(server);
                    anyProxy = true;
                    props.append({QStringLiteral("proxyServer"), srv});
                    out.append(QStringLiteral("  Proxy: %1 (enabled)").arg(srv));
                }
            }
            RegCloseKey(hKey);
        }
    }
#else
    // Unix 环境变量（http_proxy/https_proxy/no_proxy）
    const QStringList vars = { QStringLiteral("http_proxy"), QStringLiteral("https_proxy"),
                               QStringLiteral("HTTP_PROXY"), QStringLiteral("HTTPS_PROXY"),
                               QStringLiteral("no_proxy"), QStringLiteral("NO_PROXY") };
    for (const QString& v : vars) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        const QString val = qEnvironmentVariable(v.toLatin1().constData());
        if (!val.isEmpty()) {
            anyProxy = true;
            props.append({v, val});
            out.append(QStringLiteral("  %1=%2").arg(v, val));
        }
    }
#endif

    if (!anyProxy)
        return makeResult(id, DiagStatus::Info, QStringLiteral("No proxy configured"), {}, {});
    return makeResult(id, DiagStatus::Pass, QStringLiteral("Proxy configured"), props, out.join('\n'));
}

} // namespace g2

// ── Registration ───────────────────────────────────────────────────────────
// Called by registerAllAdapters() from main() (DIAG-2/A1 explicit init).
void registerG2Adapters() {
    using g2::probeRoutingTable; using g2::probeArpTable;
    using g2::probeNetworkProfile; using g2::probeTcpSettings;
    using g2::probeDefaultGateway; using g2::probeProxySettings;

#if defined(PLATFORM_ANDROID)
    // Android：6 项均有 JNI 深探测（NetworkDiagnostics.cpp）
    AdapterRegistry::registerAdapters(DiagId::G2NetworkProfile, {
        { PF_Android, "Android", {}, [](DiagId i, const QString&, RunContext&) { return androidNetworkProfileDiag(i); } },
    });
    AdapterRegistry::registerAdapters(DiagId::G2TcpSettings, {
        { PF_Android, "Android", {}, [](DiagId i, const QString&, RunContext&) { return androidTcpSettingsDiag(i); } },
    });
    AdapterRegistry::registerAdapters(DiagId::G2DefaultGateway, {
        { PF_Android, "Android", {}, [](DiagId i, const QString&, RunContext&) { return androidGatewayDiag(i); } },
    });
    AdapterRegistry::registerAdapters(DiagId::G2RoutingTable, {
        { PF_Android, "Android", {}, [](DiagId i, const QString&, RunContext&) { return androidRoutingTableDiag(i); } },
    });
    AdapterRegistry::registerAdapters(DiagId::G2ArpTable, {
        { PF_Android, "Android", {}, [](DiagId i, const QString&, RunContext&) { return androidArpTableDiag(i); } },
    });
    AdapterRegistry::registerAdapters(DiagId::G2ProxySettings, {
        { PF_Android, "Android", {}, [](DiagId i, const QString&, RunContext&) { return androidProxyDiag(i); } },
    });
    return;
#else
#if defined(PLATFORM_IOS)
    // iOS：网关/路由经系统 API（GatewayDhcpRouting）
    AdapterRegistry::registerAdapters(DiagId::G2DefaultGateway, {
        { PF_IOS, "iOS", {}, [](DiagId i, const QString&, RunContext&) { return iosDefaultGatewayDiag(i); } },
    });
    AdapterRegistry::registerAdapters(DiagId::G2RoutingTable, {
        { PF_IOS, "iOS", {}, [](DiagId i, const QString&, RunContext&) { return iosRoutingTableDiag(i); } },
    });
#endif
#endif

    AdapterRegistry::registerAdapters(DiagId::G2NetworkProfile, {
        { PF_Desktop, "Desktop", {}, probeNetworkProfile },
#if !defined(PLATFORM_ANDROID)
        { PF_IOS,     "iOS",     {}, probeNetworkProfile },
#endif
    });
    AdapterRegistry::registerAdapters(DiagId::G2TcpSettings, {
        { PF_Desktop, "Desktop", {}, probeTcpSettings },
    });
    AdapterRegistry::registerAdapters(DiagId::G2DefaultGateway, {
        { PF_Desktop, "Desktop", {}, probeDefaultGateway },
    });
    AdapterRegistry::registerAdapters(DiagId::G2RoutingTable, {
        { PF_Desktop, "Desktop", {}, probeRoutingTable },
    });
    AdapterRegistry::registerAdapters(DiagId::G2ArpTable, {
        { PF_Desktop, "Desktop", {}, probeArpTable },
    });
    AdapterRegistry::registerAdapters(DiagId::G2ProxySettings, {
        { PF_Desktop, "Desktop", {}, probeProxySettings },
    });
}
