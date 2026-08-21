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
#include "Diagnostics/Model/GHelpers.h"   // readProcLines（procfs atEnd 陷阱共享）
#include "Diagnostics/View/V030TerminalFormat.h"   // childVal（v0.0.3 复刻层共用取值）

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

// 5WHY (复核 2026-08-21 三份漂移收敛): 本文件曾保留一份 ipToStr 本地副本
// （小端 hex → 点分十进制）——与 SystemDiagnostics::ipToStr（in_addr 内存
// 序等价，G1 routeGateways 同源消费同一文件）逐字同构；GHelpers.h 自包含
// 后可直接共享。删除本地副本，调用点统一 SystemDiagnostics::ipToStr。

// ── G2RoutingTable ─────────────────────────────────────────────────────────
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
    int routeCount = 0;

    // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): v0.0.3 全平台无条件骨架——
    // 空行 + 分隔线 + "Interface List" + 分隔线（零路由亦然；首版曾把
    // 头块整体藏在 Linux 分支且 gate 于 routeCount>0，Windows/macOS
    // 头部尽失）。
    out.append(QString());
    out.append(QStringLiteral("==========================================================================="));
    out.append(QStringLiteral("Interface List"));
    out.append(QStringLiteral("==========================================================================="));

#if defined(_WIN32)
    // v0.0.3 Windows 接口清单行（"  %1...%2 ......%3"）
    {
        ULONG bufLen = 15000;
        QByteArray buf(bufLen, '\0');
        PIP_ADAPTER_ADDRESSES adapters = (PIP_ADAPTER_ADDRESSES)buf.data();
        if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, adapters, &bufLen) != NO_ERROR) {
            buf.resize(bufLen);
            adapters = (PIP_ADAPTER_ADDRESSES)buf.data();
            GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, adapters, &bufLen);
        }
        for (auto* a = adapters; a; a = a->Next)
            out.append(QStringLiteral("  %1...%2 ......%3")
                .arg(a->Ipv6IfIndex, 4)
                .arg(macToStr(reinterpret_cast<const unsigned char*>(a->PhysicalAddress),
                              a->PhysicalAddressLength))
                .arg(QString::fromWCharArray(a->FriendlyName)));
    }

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
            // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): 补列子行（共享表
            // 重建所需；v0.0.3 Windows 网关直印 ip4ToStr，0.0.0.0 = on-link）。
            ResultProperty rp(ip4ToStr(dest), QStringLiteral("gw=%1 if=%2 metric=%3")
                              .arg(ip4ToStr(gw), ifName).arg(row.Metric));
            rp.children.append({QStringLiteral("netmask"), ip4ToStr(mask)});
            rp.children.append({QStringLiteral("gateway"), ip4ToStr(gw)});
            rp.children.append({QStringLiteral("interface"), ifName});
            rp.children.append({QStringLiteral("metric"), QString::number(row.Metric)});
            props.append(rp);
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
                    // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): 曾只解析
                    // DST/GATEWAY——共享表 Netmask/Interface 列缺数据。
                    // 补 RTAX_NETMASK/RTAX_IFP（v0.0.3 同源；无网关 "-"）。
                    QString dst, gw, mask, ifName;
                    auto* sa = (struct sockaddr*)(rtm + 1);
                    for (int i = 0; i < RTAX_MAX && sa->sa_len > 0; ++i) {
                        if (rtm->rtm_addrs & (1 << i)) {
                            if (i == RTAX_DST && sa->sa_family == AF_INET)
                                dst = ip4ToStr(((struct sockaddr_in*)sa)->sin_addr);
                            else if (i == RTAX_GATEWAY && sa->sa_family == AF_INET)
                                gw = ip4ToStr(((struct sockaddr_in*)sa)->sin_addr);
                            else if (i == RTAX_NETMASK && sa->sa_family == AF_INET)
                                mask = ip4ToStr(((struct sockaddr_in*)sa)->sin_addr);
                            else if (i == RTAX_IFP && sa->sa_family == AF_LINK) {
                                auto* sdl = (struct sockaddr_dl*)sa;
                                if (sdl->sdl_nlen > 0)
                                    ifName = QString::fromLatin1(sdl->sdl_data, sdl->sdl_nlen);
                            }
                            sa = (struct sockaddr*)((char*)sa + sa->sa_len);
                        }
                    }
                    if (dst.isEmpty()) { ptr += rtm->rtm_msglen; continue; }
                    const QString gwStr = gw.isEmpty() ? QStringLiteral("-") : gw;
                    ResultProperty rp(dst, QStringLiteral("gw=%1").arg(gwStr));
                    rp.children.append({QStringLiteral("netmask"), mask.isEmpty() ? QStringLiteral("-") : mask});
                    rp.children.append({QStringLiteral("gateway"), gwStr});
                    rp.children.append({QStringLiteral("interface"), ifName.isEmpty() ? QStringLiteral("-") : ifName});
                    rp.children.append({QStringLiteral("metric"), QString::number(rtm->rtm_index)});
                    props.append(rp);
                    ++routeCount;
                }
                ptr += rtm->rtm_msglen;
            }
        }
    }
#else // Linux / Android / iOS
    // 5WHY (复核 2026-08-21 procfs atEnd 陷阱): 曾 readLine+while(!atEnd())
    // ——/proc size 恒 0，表头后零行风险同 G2 IPv6 网关事故。共享
    // SystemDiagnostics::readProcLines（readLineInto 驱动）。
    // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): 曾 "  dest → gw (if)" 单行——
    // v0.0.3 为 route print 风格（"IPv4 Route Table" + 分隔线 + "Active
    // Routes:" + 列对齐表 [Network Destination/Netmask/Gateway/Interface/
    // Metric]）。补 metric（十进制，G2 网关同规则）与结构化子属性。
    for (const QString& raw : SystemDiagnostics::readProcLines(QStringLiteral("/proc/net/route"), 1)) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        const QString line = raw.trimmed();
        if (line.isEmpty()) continue;
        const QStringList cols = line.split(QLatin1Char('\t'));
        if (cols.size() >= 8) {
            const QString ifName = cols[0];
            bool ok = false;
            const uint32_t dest   = cols[1].toUInt(&ok, 16);
            const uint32_t gw     = cols[2].toUInt(&ok, 16);
            const uint32_t mask   = cols[7].toUInt(&ok, 16);
            const uint32_t metric = cols[6].toUInt(&ok, 10);
            const QString gwStr = gw ? SystemDiagnostics::ipToStr(gw) : QStringLiteral("On-link");
            ResultProperty rp(SystemDiagnostics::ipToStr(dest),
                QStringLiteral("netmask=%1 gw=%2 if=%3").arg(SystemDiagnostics::ipToStr(mask), gwStr, ifName));
            rp.children.append({QStringLiteral("netmask"), SystemDiagnostics::ipToStr(mask)});
            rp.children.append({QStringLiteral("gateway"), gwStr});
            rp.children.append({QStringLiteral("interface"), ifName});
            rp.children.append({QStringLiteral("metric"), QString::number(metric)});
            props.append(rp);
            ++routeCount;
        }
    }
#endif
#endif

    // v0.0.3 第二段（各平台共用）：分隔线 → 空行 → "IPv4 Route Table" →
    // 分隔线 → "Active Routes:" → [表，零行时省略] → 尾部分隔线
    out.append(QStringLiteral("==========================================================================="));
    out.append(QString());
    out.append(QStringLiteral("IPv4 Route Table"));
    out.append(QStringLiteral("==========================================================================="));
    out.append(QStringLiteral("Active Routes:"));
#if defined(PLATFORM_IOS)
    out.append(QStringLiteral("  [iOS] Routing table: unavailable (restricted by Apple)"));
#endif
    if (routeCount > 0) {
        static const QVector<DiagnosticFormatter::ColSpec> kRouteCols = {
            {"Network Destination", 22, false},
            {"Netmask",             16, false},
            {"Gateway",             16, false},
            {"Interface",           10, false},
            {"Metric",               6, true},
        };
        QList<QStringList> routeRows;
        for (const auto& p : props) {
            routeRows.append(QStringList()
                << p.label
                << SystemDiagnostics::childVal(p, QStringLiteral("netmask"))
                << SystemDiagnostics::childVal(p, QStringLiteral("gateway"))
                << SystemDiagnostics::childVal(p, QStringLiteral("interface")).left(9)   // v0.0.3 ifName.left(9)
                << SystemDiagnostics::childVal(p, QStringLiteral("metric")));
        }
        out.append(DiagnosticFormatter::formatTable(kRouteCols, routeRows));
    }
    out.append(QStringLiteral("==========================================================================="));

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
    // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): v0.0.3 out 首行恒空行
    // （各平台）——首版 Linux 分支直接从 "Interface: (all)" 起。
    out.append(QString());
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
    // 5WHY (复核 2026-08-21 procfs atEnd + 逐行正则): 曾 readLine+while
    // (!atEnd()) 且每行编译一次 QRegularExpression——共享 readProcLines
    // 读取 + 函数局部 static 一次编译（同 probeDefaultGateway 修正）。
    // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): v0.0.3 为 "Interface: (all)"
    // + 列对齐表 [Internet Address/Physical Address/Type]（两空格缩进），
    // Type 由 flags 列 0x2 判 static/dynamic。曾 "  ip  mac" 无类型列、
    // 且滤掉 00:00:00:00:00:00 行（v0.0.3 全行入表）与文件不可读时
    // 无 "  (ARP table not available)" 恒文。
    const bool arpReadable = [&]() {
        QFile f(QStringLiteral("/proc/net/arp"));
        return f.open(QIODevice::ReadOnly);
    }();
    if (!arpReadable) {
        out.append(QStringLiteral("  (ARP table not available)"));
    } else {
        static const QRegularExpression kWsRe(QStringLiteral("\\s+"));
        for (const QString& raw : SystemDiagnostics::readProcLines(QStringLiteral("/proc/net/arp"), 1)) {
            if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
            const QString line = raw.trimmed();
            if (line.isEmpty()) continue;
            const QStringList cols = line.split(kWsRe);
            // v0.0.3: cols.size() >= 5、不滤零 MAC 行（含 incomplete 条目）
            if (cols.size() >= 5) {
                const QString type = (cols.size() >= 3 && cols[2] == QLatin1String("0x2"))
                    ? QStringLiteral("static") : QStringLiteral("dynamic");
                ResultProperty ap(cols[0], cols[3]);
                ap.children.append({QStringLiteral("type"), type});
                props.append(ap);
                ++entryCount;
            }
        }
        out.append(QStringLiteral("Interface: (all)"));
        static const QVector<DiagnosticFormatter::ColSpec> kArpCols = {
            {"Internet Address",  24, false},
            {"Physical Address",  23, false},
            {"Type",               0, false},
        };
        QList<QStringList> arpRows;
        for (const auto& p : props)
            arpRows.append(QStringList() << p.label << p.value
                << SystemDiagnostics::childVal(p, QStringLiteral("type")));
        // v0.0.3 样式：表整体两空格缩进（逐行前缀）；零行时 formatTable
        // 仍出表头/分隔线（v0.0.3 同）
        out.append(QStringLiteral("  ") + DiagnosticFormatter::formatTable(kArpCols, arpRows)
            .join(QStringLiteral("\n  ")));
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
    // 5WHY (复核 2026-08-20 procfs atEnd 陷阱): /proc size 恒 0，atEnd()
    // 首行缓冲前恒真——"readLine 表头 + while(!atEnd())" 换无表头文件即
    // 零行（v6 循环即因此从未上报）。共享 SystemDiagnostics::readProcLines
    // 驱动（曾 v4/v6 各写一份 readLineInto 惯用法）。
    for (const QString& raw : SystemDiagnostics::readProcLines(QStringLiteral("/proc/net/route"), 1)) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        const QString t = raw.trimmed();
        if (t.isEmpty()) continue;
        const QStringList cols = t.split(QLatin1Char('\t'));
        if (cols.size() >= 8) {
            bool ok = false;
            const uint32_t dest = cols[1].toUInt(&ok, 16);
            const uint32_t gw   = cols[2].toUInt(&ok, 16);
            if (dest == 0 && gw != 0) {
                // 5WHY (2026-08-20 用户诉求 "Default Gateway 无属性卡"):
                // 曾丢失 metric（cols[6]）且只查 IPv4——路由选择依赖
                // metric，双栈网络默认走 IPv6 时属性卡为空。补 metric
                // 与 IPv6 默认路由（/proc/net/ipv6_route）。
                // 5WHY (复核 2026-08-20 进制): /proc/net/route 仅
                // Destination/Gateway/Mask 为十六进制——Metric 列是
                // 十进制（600 = 600，非 0x600=1536）。曾以基 16 解析
                // 虚报 metric。
                // 5WHY (复核 2026-08-20 重复换算): ipToStr(gw) 曾在两行
                // 各算一次——局部一次复用（共享 SystemDiagnostics::ipToStr）。
                // 5WHY (复核 2026-08-21 死守卫): 外层已判 >=8，内层 >=7
                // 恒真——直接读取。
                const uint32_t metric = cols[6].toUInt(nullptr, 10);
                const QString gwStr = SystemDiagnostics::ipToStr(gw);
                props.append({gwStr,
                    QStringLiteral("via %1, metric %2").arg(cols[0]).arg(metric)});
                found.append(QStringLiteral("%1 (via %2, metric %3)")
                    .arg(gwStr, cols[0]).arg(metric));
            }
        }
    }
    static const QRegularExpression kWsRe(QStringLiteral("\\s+"));
    for (const QString& raw : SystemDiagnostics::readProcLines(QStringLiteral("/proc/net/ipv6_route"))) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        const QString line = raw.trimmed();
        if (line.isEmpty()) continue;
        // 5WHY (复核 2026-08-20 正则提升): 曾逐行构造 QRegularExpression
        // （PCRE2 编译每行一次）——函数局部 static 一次编译复用。
        const QStringList cols = line.split(kWsRe);
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
    out.append(QString());
    out.append(QStringLiteral("Network Profile Information:"));
    // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): v0.0.3 头行后空行——
    // 首版只补了头前空行，Hostname 行紧贴头行。
    out.append(QString());

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
    // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): 头行曾 "TCP Settings:"——
    // v0.0.3 为 "\nTCP/IP Settings (table mode):\n"。
    out.append(QString());
    out.append(QStringLiteral("TCP/IP Settings (table mode):"));
    out.append(QString());

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
    // 属性卡：9 项 sysctl（与首版一致，含 KeepAlive 系列/DefaultTTL 等）
    const struct { const char* path; const char* label; } kTcpSys[] = {
        { "/proc/sys/net/ipv4/tcp_keepalive_time",  "KeepAliveTime" },
        { "/proc/sys/net/ipv4/tcp_keepalive_intvl", "KeepAliveInterval" },
        { "/proc/sys/net/ipv4/ip_default_ttl",      "DefaultTTL" },
        { "/proc/sys/net/ipv4/tcp_congestion_control", "Congestion Control" },
        { "/proc/sys/net/ipv4/tcp_window_scaling",  "Window Scaling" },
        { "/proc/sys/net/ipv4/tcp_timestamps",      "Timestamps" },
        { "/proc/sys/net/ipv4/tcp_sack",            "Selective ACK" },
        { "/proc/sys/net/ipv4/tcp_fastopen",        "TCP Fast Open" },
        { "/proc/sys/net/ipv4/tcp_max_syn_backlog", "Max SYN Backlog" },
    };
    for (const auto& e : kTcpSys) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        QFile file(QLatin1String(e.path));
        if (file.open(QIODevice::ReadOnly))
            props.append({QLatin1String(e.label), QString::fromLatin1(file.readAll().trimmed())});
    }
    // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): v0.0.3 Linux 终端为
    // formatTable {Setting 20/Value 0} 五行 readSys（文件不可读 → "-"，
    // 恒五行）。首版曾仿 Windows 注册表标签集输出 "  Label: val" 九行
    // ——行式/表式与行集均非 v0.0.3（"ms" 单位属 v0.0.3 Windows 分支，
    // Linux readSys 无单位）。
    const struct { const char* path; const char* label; } kTcpTable[] = {
        { "/proc/sys/net/ipv4/tcp_congestion_control", "Congestion Control" },
        { "/proc/sys/net/ipv4/tcp_window_scaling",      "Window Scaling" },
        { "/proc/sys/net/ipv4/tcp_timestamps",          "Timestamps" },
        { "/proc/sys/net/ipv4/tcp_sack",                "Selective ACK" },
        { "/proc/sys/net/ipv4/tcp_fastopen",            "TCP Fast Open" },
    };
    static const QVector<DiagnosticFormatter::ColSpec> kTcpCols = {
        {"Setting", 20, false},
        {"Value",    0, false},
    };
    QList<QStringList> tcpRows;
    for (const auto& e : kTcpTable) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        QFile file(QLatin1String(e.path));
        const QString val = file.open(QIODevice::ReadOnly)
            ? QString::fromLatin1(file.readAll().trimmed()) : QStringLiteral("-");
        tcpRows.append({ QLatin1String(e.label), val });
    }
    out.append(DiagnosticFormatter::formatTable(kTcpCols, tcpRows));
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
    // Unix 环境变量——v0.0.3 七变量与顺序（HTTP_PROXY/HTTPS_PROXY/FTP_PROXY/
    // NO_PROXY 大写在前；首版曾缺 FTP_PROXY 且顺序相反，表格行序不符）。
    const QStringList vars = { QStringLiteral("HTTP_PROXY"), QStringLiteral("HTTPS_PROXY"),
                               QStringLiteral("FTP_PROXY"), QStringLiteral("NO_PROXY"),
                               QStringLiteral("http_proxy"), QStringLiteral("https_proxy"),
                               QStringLiteral("no_proxy") };
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

    // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): v0.0.3 为 "Proxy Configuration
    // (table mode):" 头 + 列对齐表 [Variable 16/Value 0]；未配置时表下
    // 印 "  No proxy configured"（恒有终端文本，满足无条件转储诉求）。
    out.clear();
    out.append(QString());
    out.append(QStringLiteral("Proxy Configuration (table mode):"));
    out.append(QString());
    if (!anyProxy) {
        out.append(QStringLiteral("  No proxy configured"));
        // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): v0.0.3 恒 Info +
        // "Proxy Settings Collected"（无论是否配置）——曾 Pass/"Proxy
        // configured"（状态徽章与摘要卡与 v0.0.3 不符）。
        return makeResult(id, DiagStatus::Info, QStringLiteral("Proxy Settings Collected"), {}, out.join('\n'));
    }
    static const QVector<DiagnosticFormatter::ColSpec> kProxyCols = {
        {"Variable", 16, false},
        {"Value",     0, false},
    };
    QList<QStringList> proxyRows;
    for (const auto& p : props)
        proxyRows.append({ p.label, p.value });
    out.append(DiagnosticFormatter::formatTable(kProxyCols, proxyRows));
    return makeResult(id, DiagStatus::Info, QStringLiteral("Proxy Settings Collected"), props, out.join('\n'));
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
