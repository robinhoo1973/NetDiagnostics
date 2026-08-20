// =============================================================================
// G1/Adapters.cpp — G1 System & Adapters adapters + probe implementations
//
// Registered via registerG1Adapters() (called by registerAllAdapters() from
// main(), DIAG-2/A1).  Platforms per NEW-1 code-verified matrix:
//   All: NetworkAdapters/Wifi/Dhcp/IpConfig/CellularInfo
//   Desktop|Android: NicAdvanced/Wired/ActiveConnections
// =============================================================================
#if defined(_WIN32)
// 必须先于任何 Qt/Windows 头（Qt 头会先拉 windows.h）：
// WINAPI_FAMILY=DESKTOP_APP 使 IP Helper 桌面分区可见；_WIN32_WINNT 最低版本。
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

#include <QNetworkInterface>
#include <QNetworkAddressEntry>
#include <QHostAddress>
#include <QHostInfo>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QProcess>
#include <QDir>
#include <QDateTime>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

#include <cstring>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
// MinGW 陷阱：netioapi.h 必须先于 iphlpapi.h（否则 __IPHLPAPI_H__ 已定义，
// netioapi.h 跳过 iprtrmib.h → MIB_IF_TABLE2 等类型缺失）。
#include <netioapi.h>
#include <iphlpapi.h>   // GetAdaptersInfo / GetExtendedTcpTable
#include <tcpmib.h>     // MIB_TCP_STATE_ESTAB / MIB_TCPTABLE_OWNER_PID
#include <wlanapi.h>    // WlanOpenHandle/WlanQueryInterface（WiFi 深探测）
// MinGW wlanapi.h 缺 WPA3 常量（0x8，Windows 10+ WLAN 报告 SAE）；MSVC/新版 SDK 自带。
// 5WHY：直接裸用 DOT11_AUTH_ALGO_WPA3 使 MinGW 工具链编译失败——需 SDK 兼容回退。
// 5WHY (复核 2026-08-18): 初版用 _MSC_VER 判工具链——plain clang 以 MSVC SDK
// 编译时不定义 _MSC_VER 却已有枚举，回退宏遮蔽 SDK 枚举（原 bug 换工具链重现）；
// 且外层单守卫内 #define 四个符号——只缺部分变体的工具链被整体跳过仍编译失败。
// 改为 __MINGW32__（真正的受影响工具链）+ 逐符号独立守卫。
#if defined(__MINGW32__)
#if !defined(DOT11_AUTH_ALGO_WPA3)
#define DOT11_AUTH_ALGO_WPA3 0x00000008
#endif
#if !defined(DOT11_AUTH_ALGO_WPA3_ENT_192)
#define DOT11_AUTH_ALGO_WPA3_ENT_192 0x00000009
#endif
#if !defined(DOT11_AUTH_ALGO_OWE)
#define DOT11_AUTH_ALGO_OWE          0x0000000A
#endif
#if !defined(DOT11_AUTH_ALGO_WPA3_ENT)
#define DOT11_AUTH_ALGO_WPA3_ENT     0x0000000B
#endif
#endif
#endif

#if defined(__APPLE__)
#include <ifaddrs.h>
// 5WHY (复核 2026-08-19 Apple CI 失败): gethostname 在 Apple 平台需
// <unistd.h>（POSIX 声明）——曾仅在 Linux 分支包含，macOS/iOS 构建
// 'use of undeclared identifier'（本地 Linux 构建含该头、CI 才暴露）。
#include <unistd.h>
#if !defined(PLATFORM_IOS)
#include "Common/Platform/Apple/macOS/WifiHelper.h"   // CoreWLAN SSID/BSSID
#endif
#endif
#if defined(__linux__)
#include <ifaddrs.h>
// 5WHY (复核 2026-08-20 Android 编译): gethostname 的 POSIX 声明在
// <unistd.h>——曾在 !PLATFORM_ANDROID 门内（Bionic 与桌面 Linux 均有此
// 头），Android 构建 'undeclared identifier'。移出平台门供 Android 共用。
#include <unistd.h>
#if !defined(PLATFORM_ANDROID)
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <linux/wireless.h>   // iwreq / SIOCGIWESSID / SIOCGIWAP / SIOCGIWFREQ（Bionic 无此头）
#endif
#endif

#include "Diagnostics/View/DiagnosticFormatter.h"

using namespace PlatformFlag;

namespace g1 {

// ── Helpers ────────────────────────────────────────────────────────────────
static QString mac6ToStr(const unsigned char* mac) {
    return QStringLiteral("%1:%2:%3:%4:%5:%6")
        .arg(mac[0], 2, 16, QLatin1Char('0')).arg(mac[1], 2, 16, QLatin1Char('0'))
        .arg(mac[2], 2, 16, QLatin1Char('0')).arg(mac[3], 2, 16, QLatin1Char('0'))
        .arg(mac[4], 2, 16, QLatin1Char('0')).arg(mac[5], 2, 16, QLatin1Char('0'))
        .toUpper();
}

static DiagnosticResult makeResult(DiagId id, DiagStatus status,
                                   const QString& summary,
                                   const QVector<ResultProperty>& props,
                                   const QString& details) {
    DiagnosticResult r;
    r.id = id;
    r.displayName = diagDisplayName(id);
    r.group = diagGroup(id);
    r.status = status;
    r.summary = summary;
    r.properties = props;
    // 5WHY (复核 2026-08-19 v0.0.3 对等): 8 个 G1 探针全部传空 details——
    // 详情页终端区块（details||rawOutput 门控）对 G1 恒隐藏，而 v0.0.3
    // 各探针都输出格式化多行转储。空 details 时由 props 生成转储文本，
    // 终端区块恢复呈现（子属性缩进，与剪贴板格式一致）。
    if (details.isEmpty() && !props.isEmpty()) {
        QStringList lines;
        for (const ResultProperty& p : props) {
            lines.append(QStringLiteral("%1: %2").arg(p.label, p.value));
            for (const auto& c : p.children)
                lines.append(QStringLiteral("  %1: %2").arg(c.label, c.value));
        }
        r.details = lines.join(QLatin1Char('\n'));
        r.rawOutput = r.details;
        // 5WHY (2026-08-19 用户诉求 "不单独列出 During 区块"): 该转储与
        // 属性卡逐字重复——详情页曾并列呈现两份相同数据。标记为属性派生
        // 转储：UI 经 resultFor 关闭终端区块（showTerminal=false），数据
        // 以结构化属性卡呈现；剪贴板仍含完整转储（details 保留）。
        r.data[QStringLiteral("propsDump")] = true;
    } else {
        r.details = details;
        r.rawOutput = details;
    }
    r.timestamp = QDateTime::currentDateTime();
    return r;
}

static QVector<QNetworkInterface> runningInterfaces() {
    QVector<QNetworkInterface> out;
    const auto all = QNetworkInterface::allInterfaces();
    for (const auto& i : all)
        // R4-2：IsUp && IsRunning 即可（含 loopback/隧道），不再要求 CanBroadcast
        if (i.flags().testFlag(QNetworkInterface::IsUp)
            && i.flags().testFlag(QNetworkInterface::IsRunning))
            out.append(i);
    return out;
}

// ── 现代 Linux 无线接口判据（5WHY 2026-08-20 用户诉求 "WiFi 无数据"）──
// 旧判据只认 /sys/class/net/<if>/wireless（wext 目录）——内核 6.6+ 的
// mac80211 不再创建该目录（本机 wlan0 即无此目录），过滤恒空 → 探针
// Skipped、详情页零数据。现代判据按可靠性排序：
//   1. phy80211 符号链接（mac80211 始终创建，无线网卡专属）
//   2. uevent DEVTYPE=wlan
//   3. 旧 wext 目录（旧内核兼容）
//   4. /proc/net/wireless 行成员（最后手段）
#if defined(__linux__)
static bool isWirelessInterface(const QString& ifName) {
    if (QFileInfo::exists(QStringLiteral("/sys/class/net/%1/phy80211").arg(ifName)))
        return true;
    QFile uevent(QStringLiteral("/sys/class/net/%1/uevent").arg(ifName));
    if (uevent.open(QIODevice::ReadOnly)
        && QString::fromLatin1(uevent.readAll()).contains(QLatin1String("DEVTYPE=wlan")))
        return true;
    if (QFile::exists(QStringLiteral("/sys/class/net/%1/wireless").arg(ifName)))
        return true;
    QFile pw(QStringLiteral("/proc/net/wireless"));
    if (pw.open(QIODevice::ReadOnly)) {
        QTextStream ts(&pw);
        ts.readLine(); ts.readLine();   // 表头两行
        while (!ts.atEnd()) {
            const QString line = ts.readLine().trimmed();
            if (line.startsWith(ifName + QLatin1Char(':'))) return true;
        }
    }
    return false;
}
#endif

// 子网掩码 → CIDR 前缀长度（业界惯例 CIDR 呈现：192.168.20.150/24）
static int netmaskPrefixLen(const QHostAddress& mask) {
    if (mask.protocol() == QAbstractSocket::IPv4Protocol) {
        const quint32 m = mask.toIPv4Address();
        if (m == 0) return 0;
        int trailing = 0;
        quint32 v = m;
        while ((v & 1u) == 0) { ++trailing; v >>= 1; }
        return 32 - trailing;
    }
    return -1;
}

// ── G1NetworkAdapters ────────────────────────────────────────────────────
static DiagnosticResult probeNetworkAdapters(DiagId id, const QString&, RunContext& ctx) {
    QVector<ResultProperty> props;
    const auto ifaces = runningInterfaces();
    for (const auto& i : ifaces) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        ResultProperty p(i.name(), i.hardwareAddress().isEmpty()
                         ? QStringLiteral("no MAC") : i.hardwareAddress());
        QString type = (i.flags().testFlag(QNetworkInterface::IsLoopBack))
                     ? QStringLiteral("loopback")
                     : QStringLiteral("ethernet");
        p.children.append({QStringLiteral("type"), type});
        // 5WHY (2026-08-20 用户诉求 "属性卡一团混乱"): 地址子行曾一律标
        // "address"——v4/v6 混排无法区分协议。按协议标注 IPv4/IPv6，
        // 与 IP Configuration 卡一致（业界惯例：CIDR 或协议标注地址）。
        for (const auto& e : i.addressEntries())
            p.children.append({e.ip().protocol() == QAbstractSocket::IPv6Protocol
                    ? QStringLiteral("IPv6") : QStringLiteral("IPv4"), e.ip().toString()});
        props.append(p);
    }
    if (props.isEmpty())
        return makeResult(id, DiagStatus::Info, QStringLiteral("No active adapters found"), {}, {});
    QStringList parts;
    for (const auto& p : props)
        parts.append(QStringLiteral("%1 (%2)").arg(p.label, p.value));
    DiagnosticResult r = makeResult(id, DiagStatus::Pass,
        QStringLiteral("%1 active adapter(s)").arg(props.size()), props, {});
    r.narrative = QStringLiteral("Detected %1 active network adapter(s): %2. "
        "Type and address entries are listed per adapter in the property cards below.")
        .arg(props.size()).arg(parts.join(QStringLiteral(", ")));
    return r;
}

// ── G1NicAdvanced ─────────────────────────────────────────────────────────
static QString formatLinkSpeed(quint64 bps) {
    if (bps == 0) return QStringLiteral("unknown");
    if (bps >= 1000000000ULL)
        return QStringLiteral("%1 Gbps").arg(bps / 1.0e9, 0, 'f', 1);
    return QStringLiteral("%1 Mbps").arg(bps / 1.0e6, 0, 'f', 0);
}

static DiagnosticResult probeNicAdvanced(DiagId id, const QString&, RunContext& ctx) {
    QVector<ResultProperty> props;
    QStringList knownSpeeds;
    int speedKnown = 0;

#if defined(_WIN32)
    // MIB_IF_ROW2（GetIfTable2）：Alias=友好名、TransmitLinkSpeed=发送速率 bps、
    // MediaDuplexState=双工。释放用 HeapFree（FreeMibTable 在本 mingw 不可用）。
    PMIB_IF_TABLE2 table = nullptr;
    if (GetIfTable2(&table) == NO_ERROR && table) {
        for (ULONG i = 0; i < table->NumEntries; ++i) {
            if (ctx.cancelled.load()) { HeapFree(GetProcessHeap(), 0, table);
                return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled")); }
            const auto& row = table->Table[i];
            if (row.OperStatus == IfOperStatusNotPresent) continue;
            if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            const QString ifName = QString::fromWCharArray(row.Alias);
            const QString speed = formatLinkSpeed(row.TransmitLinkSpeed);
            ResultProperty p(ifName, speed);
            if (row.TransmitLinkSpeed > 0) { knownSpeeds.append(ifName + QLatin1Char('=') + speed); ++speedKnown; }
            p.children.append({QStringLiteral("MTU"), QString::number(row.Mtu)});
            p.children.append({QStringLiteral("type"), QString::number(row.Type)});
            p.children.append({QStringLiteral("status"),
                row.OperStatus == IfOperStatusUp ? QStringLiteral("up")
                : row.OperStatus == IfOperStatusDown ? QStringLiteral("down")
                : QStringLiteral("other")});
            // 双工：MinGW MIB_IF_ROW2 无 MediaDuplexState 字段；按归档启发式——
            // ≥1Gbps 链路标准为全双工，其余标 unknown（真实 OID 查询需内核句柄）。
            p.children.append({QStringLiteral("duplex"),
                row.TransmitLinkSpeed >= 1000000000ULL ? QStringLiteral("full")
                                                       : QStringLiteral("unknown")});
            QStringList macParts;
            for (ULONG b = 0; b < row.PhysicalAddressLength; ++b)
                macParts << QStringLiteral("%1").arg(row.PhysicalAddress[b], 2, 16, QLatin1Char('0')).toUpper();
            p.children.append({QStringLiteral("MAC"),
                macParts.isEmpty() ? QStringLiteral("N/A") : macParts.join(QLatin1Char('-'))});
            props.append(p);
        }
        HeapFree(GetProcessHeap(), 0, table);
    }
#else
#if defined(__linux__) || defined(__ANDROID__)
    // sysfs：驱动上报的真实协商速率/双工（文件不存在 = 驱动不暴露）。
    for (const auto& i : runningInterfaces()) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        if (i.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
        const QString base = QStringLiteral("/sys/class/net/%1/").arg(i.name());
        quint64 bps = 0;
        QFile sf(base + QStringLiteral("speed"));
        if (sf.open(QIODevice::ReadOnly)) {
            bool ok = false;
            const quint64 mbps = QString::fromLatin1(sf.readAll().trimmed()).toULongLong(&ok);
            if (ok && mbps > 0) bps = mbps * 1000000ULL;
        }
        QString duplex;
        QFile df(base + QStringLiteral("duplex"));
        if (df.open(QIODevice::ReadOnly))
            duplex = QString::fromLatin1(df.readAll().trimmed());
        const QString speed = formatLinkSpeed(bps);
        ResultProperty p(i.name(), speed);
        if (bps > 0) { knownSpeeds.append(i.name() + QLatin1Char('=') + speed); ++speedKnown; }
        p.children.append({QStringLiteral("MTU"), QString::number(i.maximumTransmissionUnit())});
        p.children.append({QStringLiteral("duplex"), duplex.isEmpty() ? QStringLiteral("unknown") : duplex});
        p.children.append({QStringLiteral("MAC"), i.hardwareAddress()});
        props.append(p);
    }
#else
#if defined(__APPLE__) && !defined(PLATFORM_IOS)
    // macOS：ifconfig media 行（如 "media: autoselect (1000baseT <full-duplex>)"）。
    static const QRegularExpression kMedia(QStringLiteral("(\\d+)\\s*(G|M)?base"));
    for (const auto& i : runningInterfaces()) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        if (i.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
        quint64 bps = 0;
        QString duplex;
        QProcess proc;
        proc.start(QStringLiteral("ifconfig"), QStringList() << i.name());
        if (proc.waitForFinished(3000)) {
            const QString out = QString::fromLocal8Bit(proc.readAllStandardOutput());
            const auto m = kMedia.match(out);
            if (m.hasMatch()) {
                const quint64 value = m.captured(1).toULongLong();
                bps = (m.captured(2) == QLatin1String("G")) ? value * 1000000000ULL : value * 1000000ULL;
            }
            if (out.contains(QLatin1String("full-duplex"))) duplex = QStringLiteral("full");
            else if (out.contains(QLatin1String("half-duplex"))) duplex = QStringLiteral("half");
        } else {
            proc.kill();
            proc.waitForFinished(2000);   // R5-1: 超时后必须回收
        }
        const QString speed = formatLinkSpeed(bps);
        ResultProperty p(i.name(), speed);
        if (bps > 0) { knownSpeeds.append(i.name() + QLatin1Char('=') + speed); ++speedKnown; }
        p.children.append({QStringLiteral("MTU"), QString::number(i.maximumTransmissionUnit())});
        p.children.append({QStringLiteral("duplex"), duplex.isEmpty() ? QStringLiteral("unknown") : duplex});
        p.children.append({QStringLiteral("MAC"), i.hardwareAddress()});
        props.append(p);
    }
#else
    // iOS 等：系统不向第三方应用暴露链路速率——只报 MTU/MAC（诚实）。
    for (const auto& i : runningInterfaces()) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        ResultProperty p(i.name(), QStringLiteral("unknown (system-managed)"));
        p.children.append({QStringLiteral("MTU"), QString::number(i.maximumTransmissionUnit())});
        props.append(p);
    }
#endif
#endif
#endif

    if (props.isEmpty())
        return makeResult(id, DiagStatus::Info, QStringLiteral("No NIC driver details available"), {}, {});
    if (speedKnown > 0) {
        // 摘要限长：大量接口（WFP 过滤层等）时截断为 3 条 + 计数。
        QStringList shown = knownSpeeds;
        QString suffix;
        if (shown.size() > 3) {
            suffix = QStringLiteral(" (+%1 more)").arg(shown.size() - 3);
            shown = shown.mid(0, 3);
        }
        DiagnosticResult r = makeResult(id, DiagStatus::Pass,
            QStringLiteral("%1 NIC(s), link speed: %2%3").arg(props.size()).arg(shown.join(QStringLiteral(", ")), suffix),
            props, {});
        r.narrative = QStringLiteral("Enumerated %1 NIC(s); link speed reported for %2: %3%4. "
            "Speed/duplex/MTU/MAC are listed per adapter below.")
            .arg(props.size()).arg(speedKnown).arg(shown.join(QStringLiteral(", ")), suffix);
        return r;
    }
    DiagnosticResult r = makeResult(id, DiagStatus::Info,
        QStringLiteral("%1 NIC(s) enumerated (driver does not expose link speed)").arg(props.size()),
        props, {});
    r.narrative = QStringLiteral("Enumerated %1 NIC(s), but the driver does not expose negotiated link speed. "
        "MTU/MAC/duplex are still listed per adapter below.").arg(props.size());
    return r;
}

// ── G1WifiDiagnostics ─────────────────────────────────────────────────────
static DiagnosticResult probeWifi(DiagId id, const QString&, RunContext& ctx) {
    QVector<ResultProperty> props;
    QStringList ssids;
#if defined(_WIN32)
    // Windows：WLAN API 深探测（SSID/BSSID/信道/RSSI/认证算法）
    HANDLE hClient = nullptr;
    DWORD negotiatedVer = 0;
    if (WlanOpenHandle(2, nullptr, &negotiatedVer, &hClient) == ERROR_SUCCESS) {
        PWLAN_INTERFACE_INFO_LIST ifList = nullptr;
        if (WlanEnumInterfaces(hClient, nullptr, &ifList) == ERROR_SUCCESS) {
            for (DWORD i = 0; i < ifList->dwNumberOfItems; i++) {
                if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
                auto& wi = ifList->InterfaceInfo[i];
                ResultProperty p(QString::fromWCharArray(wi.strInterfaceDescription),
                    wi.isState == wlan_interface_state_connected ? QStringLiteral("connected") : QStringLiteral("disconnected"));
                QString ssid = QStringLiteral("-"), bssid = QStringLiteral("-");
                QString channel = QStringLiteral("-"), signal = QStringLiteral("-");
                if (wi.isState == wlan_interface_state_connected) {
                    DWORD dataSize = 0;
                    PWLAN_CONNECTION_ATTRIBUTES pConn = nullptr;
                    if (WlanQueryInterface(hClient, &wi.InterfaceGuid, wlan_intf_opcode_current_connection,
                                          nullptr, &dataSize, (PVOID*)&pConn, nullptr) == ERROR_SUCCESS && pConn) {
                        ssid = QString::fromUtf8((const char*)pConn->wlanAssociationAttributes.dot11Ssid.ucSSID,
                                                 pConn->wlanAssociationAttributes.dot11Ssid.uSSIDLength);
                        bssid = mac6ToStr((const unsigned char*)pConn->wlanAssociationAttributes.dot11Bssid);
                        ULONG ch = 0; DWORD chSize = sizeof(ch);
                        if (WlanQueryInterface(hClient, &wi.InterfaceGuid, wlan_intf_opcode_channel_number,
                                              nullptr, &chSize, (PVOID*)&ch, nullptr) == ERROR_SUCCESS && ch > 0)
                            channel = QString::number(ch);
                        QString auth = QStringLiteral("Unknown");
                        switch (pConn->wlanSecurityAttributes.dot11AuthAlgorithm) {
                            case DOT11_AUTH_ALGO_80211_OPEN:      auth = QStringLiteral("Open"); break;
                            case DOT11_AUTH_ALGO_80211_SHARED_KEY: auth = QStringLiteral("Shared"); break;
                            case DOT11_AUTH_ALGO_WPA:             auth = QStringLiteral("WPA"); break;
                            case DOT11_AUTH_ALGO_WPA_PSK:         auth = QStringLiteral("WPA-PSK"); break;
                            case DOT11_AUTH_ALGO_WPA3:            auth = QStringLiteral("WPA3"); break;
                            // 5WHY (复核 2026-08-18): WPA3-Enterprise/OWE 网络此前
                            // 落入 default 报 "Unknown"——本 switch 就在写 WPA3 支持，
                            // 补齐三个变体。
                            case DOT11_AUTH_ALGO_WPA3_ENT_192:    auth = QStringLiteral("WPA3-Enterprise 192-bit"); break;
                            case DOT11_AUTH_ALGO_OWE:             auth = QStringLiteral("OWE"); break;
                            case DOT11_AUTH_ALGO_WPA3_ENT:        auth = QStringLiteral("WPA3-Enterprise"); break;
                            case DOT11_AUTH_ALGO_RSNA:            auth = QStringLiteral("WPA2"); break;
                            case DOT11_AUTH_ALGO_RSNA_PSK:        auth = QStringLiteral("WPA2-PSK"); break;
                            default: break;
                        }
                        p.children.append({QStringLiteral("security"), auth});
                        WlanFreeMemory(pConn); pConn = nullptr;
                    }
                    LONG rssi = 0; dataSize = sizeof(rssi);
                    if (WlanQueryInterface(hClient, &wi.InterfaceGuid, wlan_intf_opcode_rssi,
                                          nullptr, &dataSize, (PVOID*)&rssi, nullptr) == ERROR_SUCCESS) {
                        const int pct = (rssi >= -50) ? 100 : (rssi <= -100) ? 0 : 2 * (rssi + 100);
                        signal = QStringLiteral("%1% (%2 dBm)").arg(pct).arg(rssi);
                    }
                }
                p.children.append({QStringLiteral("SSID"), ssid});
                p.children.append({QStringLiteral("BSSID"), bssid});
                p.children.append({QStringLiteral("channel"), channel});
                p.children.append({QStringLiteral("signal"), signal});
                props.append(p);
                if (ssid != QLatin1String("-")) ssids.append(ssid);
            }
            WlanFreeMemory(ifList);
        }
        WlanCloseHandle(hClient, nullptr);
    }
#else
    // Linux：wireless extensions ioctl（SSID/BSSID/频段）+ sysfs bitrate + /proc 信号；
    // macOS：CoreWLAN 专用 .mm 助手。
    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) == 0) {
        QSet<QString> seen;
        for (auto* q = ifa; q; q = q->ifa_next) {
            if (ctx.cancelled.load()) { freeifaddrs(ifa); return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled")); }
            const QString ifName = QString::fromLatin1(q->ifa_name);
            if (seen.contains(ifName)) continue;
#if defined(__APPLE__)
            if (!ifName.startsWith(QLatin1String("en"))) continue;   // macOS WiFi = en*
#else
            // 5WHY (2026-08-20 用户诉求 "WiFi 信息无数据"): 见
            // isWirelessInterface——内核 6.6+ 无 wext wireless 目录，
            // 旧判据把真实 wlan0 过滤掉 → 恒 Skipped。
            if (!isWirelessInterface(ifName)) continue;
#endif
            seen.insert(ifName);
            QString ssid = QStringLiteral("-"), bssid = QStringLiteral("-");
            QString channel = QStringLiteral("-"), signal = QStringLiteral("-"), bitrate = QStringLiteral("-");
#if defined(__APPLE__) && !defined(PLATFORM_IOS)
            const QString s = macosWifiSsid();
            if (!s.isEmpty()) ssid = s;
            const QString b = macosWifiBssid();
            if (!b.isEmpty()) bssid = b;
#else
#if defined(__linux__) && !defined(PLATFORM_ANDROID)
            int sock = ::socket(AF_INET, SOCK_DGRAM, 0);
            if (sock >= 0) {
                struct iwreq wrq;
                std::memset(&wrq, 0, sizeof(wrq));
                strncpy(wrq.ifr_name, ifName.toUtf8().constData(), IFNAMSIZ - 1);
                char essid[IW_ESSID_MAX_SIZE + 1] = {};
                wrq.u.essid.pointer = essid;
                wrq.u.essid.length = IW_ESSID_MAX_SIZE + 1;
                wrq.u.essid.flags = 0;
                if (::ioctl(sock, SIOCGIWESSID, &wrq) == 0 && wrq.u.essid.length > 0)
                    ssid = QString::fromUtf8(essid, wrq.u.essid.length);
                if (::ioctl(sock, SIOCGIWAP, &wrq) == 0 && wrq.u.ap_addr.sa_family == ARPHRD_ETHER)
                    bssid = mac6ToStr((const unsigned char*)wrq.u.ap_addr.sa_data);
                if (::ioctl(sock, SIOCGIWFREQ, &wrq) == 0) {
                    const double freq = wrq.u.freq.m / 1e9;
                    // 8-18：chained .arg() 中混入 %1 与 %.3f——QString::arg 的
                    // 占位符解析会把 %.3f 误当 %3 消费导致 "Argument missing"。
                    // 拆成两次独立格式化消除歧义。
                    channel = QStringLiteral("%1 (%2 GHz)")
                        .arg(static_cast<int>((freq - 2.412) / 0.005 + 1))
                        .arg(QString::number(freq, 'f', 3));
                }
                ::close(sock);
            }
            QFile wfile(QStringLiteral("/proc/net/wireless"));
            if (wfile.open(QIODevice::ReadOnly)) {
                QTextStream ts(&wfile);
                ts.readLine(); ts.readLine();
                while (!ts.atEnd()) {
                    const QString line = ts.readLine().trimmed();
                    if (line.startsWith(ifName + QLatin1Char(':'))) {
                        const QStringList cols = line.split(QRegularExpression(QStringLiteral("\\s+")));
                        // 列：face status link level noise ...
                        // cols[0]="wlan0:" cols[1]=status cols[2]=link cols[3]=level cols[4]=noise
                        // 5WHY (2026-08-20 用户诉求 "WiFi 信息无数据"): 曾取
                        // cols[4]——那是噪声（本机 -256），信号电平在
                        // cols[3]（本机 -20 dBm）。v0.0.3 取 cols[3]，移植
                        // 时列索引偏移一位。恢复电平列。
                        if (cols.size() >= 4) {
                            QString sig = cols[3];
                            sig.remove(QLatin1Char('.'));
                            signal = sig + QStringLiteral(" dBm");
                        }
                        break;
                    }
                }
            }
            QFile rateFile(QStringLiteral("/sys/class/net/%1/wireless/bitrate").arg(ifName));
            if (rateFile.open(QIODevice::ReadOnly))
                bitrate = QString::fromLatin1(rateFile.readAll().trimmed());
            // 5WHY (2026-08-20 现代内核): wext 目录随内核 6.6+ 移除，
            // bitrate 文件不再存在——`iw dev <if> link`（nl80211）仍暴露
            // tx bitrate；有 iw 时作回退源，无则诚实保留 "-"。
            if (bitrate == QLatin1String("-")) {
                const QString iw = QStandardPaths::findExecutable(QStringLiteral("iw"));
                if (!iw.isEmpty()) {
                    QProcess iproc;
                    iproc.start(iw, QStringList() << QStringLiteral("dev") << ifName << QStringLiteral("link"));
                    if (iproc.waitForFinished(3000)) {
                        const QStringList iwl = QString::fromLocal8Bit(iproc.readAllStandardOutput()).split(QLatin1Char('\n'));
                        for (const QString& il : iwl) {
                            const QString t = il.trimmed();
                            if (t.startsWith(QLatin1String("tx bitrate:")))
                                bitrate = t.section(QLatin1Char(':'), 1).trimmed();
                        }
                    } else {
                        iproc.kill();
                        iproc.waitForFinished(2000);   // R5-1
                    }
                }
            }
#endif
#endif
            ResultProperty p(ifName, ssid == QLatin1String("-") ? QLatin1String("-") : ssid);
            p.children.append({QStringLiteral("SSID"), ssid});
            p.children.append({QStringLiteral("BSSID"), bssid});
            p.children.append({QStringLiteral("channel"), channel});
            p.children.append({QStringLiteral("signal"), signal});
            if (bitrate != QLatin1String("-")) p.children.append({QStringLiteral("bitrate"), bitrate});
            props.append(p);
            if (ssid != QLatin1String("-")) ssids.append(ssid);
        }
        freeifaddrs(ifa);
    }
#endif
    if (props.isEmpty())
        return makeResult(id, DiagStatus::Skipped, QStringLiteral("No WiFi interface present"), {}, {});
    DiagnosticResult r = makeResult(id, DiagStatus::Pass,
        ssids.isEmpty() ? QStringLiteral("WiFi interface present")
                        : QStringLiteral("WiFi: %1").arg(ssids.join(QStringLiteral(", "))),
        props, {});
    r.data[QStringLiteral("ssids")] = ssids;
    r.narrative = ssids.isEmpty()
        ? QStringLiteral("A wireless interface is present but not associated with any network (SSID: -). "
            "Check that Wi-Fi is enabled and connected to a network.")
        : QStringLiteral("Wi-Fi is connected to: %1. SSID/BSSID/channel/signal are listed per interface below.")
            .arg(ssids.join(QStringLiteral(", ")));
    return r;
}

// ── G1WiredDiagnostics ────────────────────────────────────────────────────
static DiagnosticResult probeWired(DiagId id, const QString&, RunContext& ctx) {
    QVector<ResultProperty> props;
    const auto ifaces = runningInterfaces();
    for (const auto& i : ifaces) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        if (i.name().contains(QLatin1String("eth"), Qt::CaseInsensitive)
            || i.name().contains(QLatin1String("en"), Qt::CaseInsensitive)
            || i.name().contains(QLatin1String("ethernet"), Qt::CaseInsensitive)) {
            ResultProperty p(i.name(), QString::number(i.maximumTransmissionUnit()));
            for (const auto& e : i.addressEntries())
                p.children.append({QStringLiteral("address"), e.ip().toString()});
#if defined(__linux__)
            // H5/Linux：sysfs 上报真实协商速率/双工/状态
            QFile sf(QStringLiteral("/sys/class/net/%1/speed").arg(i.name()));
            if (sf.open(QIODevice::ReadOnly)) {
                const QString mbps = QString::fromLatin1(sf.readAll().trimmed());
                if (mbps.toInt() > 0) p.children.append({QStringLiteral("link speed"), mbps + QStringLiteral(" Mbps")});
            }
            QFile df(QStringLiteral("/sys/class/net/%1/duplex").arg(i.name()));
            if (df.open(QIODevice::ReadOnly)) {
                const QString d = QString::fromLatin1(df.readAll().trimmed());
                if (!d.isEmpty() && d != QLatin1String("unknown"))
                    p.children.append({QStringLiteral("duplex"), d});
            }
            QFile of(QStringLiteral("/sys/class/net/%1/operstate").arg(i.name()));
            if (of.open(QIODevice::ReadOnly))
                p.children.append({QStringLiteral("state"), QString::fromLatin1(of.readAll().trimmed())});
#endif
            props.append(p);
        }
    }
    if (props.isEmpty())
        return makeResult(id, DiagStatus::Skipped, QStringLiteral("No wired interface present"), {}, {});
    DiagnosticResult r = makeResult(id, DiagStatus::Pass, QStringLiteral("Wired interface present"), props, {});
    r.narrative = QStringLiteral("Detected %1 wired interface(s). MTU/link speed/duplex/state are listed per interface below.")
        .arg(props.size());
    return r;
}

// ── G1DhcpStatus ──────────────────────────────────────────────────────────
static DiagnosticResult probeDhcp(DiagId id, const QString&, RunContext& ctx) {
    QVector<ResultProperty> props;
    QStringList leases;
    int leaseCount = 0;

#if defined(_WIN32)
    // 真实租约：GetAdaptersInfo（旧 API，MinGW 可用）带 DhcpEnabled、
    // DhcpServer、LeaseObtained/LeaseExpires——正是租约数据源。
    ULONG bufLen = 0;
    GetAdaptersInfo(nullptr, &bufLen);
    if (bufLen > 0) {
        QByteArray buf((int)bufLen, '\0');
        auto* info = (PIP_ADAPTER_INFO)buf.data();
        if (GetAdaptersInfo(info, &bufLen) == NO_ERROR) {
            for (auto* a = info; a; a = a->Next) {
                if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
                const QString ifName = QString::fromLocal8Bit(a->Description).isEmpty()
                    ? QString::fromLocal8Bit(a->AdapterName)
                    : QString::fromLocal8Bit(a->Description);
                const bool dhcp = a->DhcpEnabled != 0;
                const QString ipStr = QString::fromLocal8Bit(a->IpAddressList.IpAddress.String);
                const QString serverStr = QString::fromLocal8Bit(a->DhcpServer.IpAddress.String);
                ResultProperty p(ifName, ipStr.isEmpty() ? QStringLiteral("(no IP)") : ipStr);
                p.children.append({QStringLiteral("DHCP"), dhcp ? QStringLiteral("Yes") : QStringLiteral("No")});
                if (dhcp) {
                    if (!serverStr.isEmpty() && serverStr != QLatin1String("0.0.0.0"))
                        p.children.append({QStringLiteral("server"), serverStr});
                    if (a->LeaseObtained > 0)
                        p.children.append({QStringLiteral("lease obtained"),
                            QDateTime::fromSecsSinceEpoch((qint64)a->LeaseObtained).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))});
                    if (a->LeaseExpires > 0)
                        p.children.append({QStringLiteral("lease expires"),
                            QDateTime::fromSecsSinceEpoch((qint64)a->LeaseExpires).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))});
                    if (!ipStr.isEmpty() && ipStr != QLatin1String("0.0.0.0")) {
                        ++leaseCount;
                        leases.append(QStringLiteral("%1=%2").arg(ifName, ipStr));
                    }
                }
                props.append(p);
            }
        }
    }
#else
#if defined(PLATFORM_IOS) && defined(__APPLE__)
    ResultProperty p(QStringLiteral("iOS DHCP"), QStringLiteral("system-managed"));
    p.children.append({QStringLiteral("DHCP"), QStringLiteral("Yes")});
    p.children.append({QStringLiteral("lease"), QStringLiteral("not exposed to third-party apps")});
    props.append(p);
#else
#if defined(__APPLE__)
    // macOS：ipconfig getpacket <iface>（系统 DHCP 客户端真实报文数据）。
    for (const auto& i : runningInterfaces()) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        if (i.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
        QProcess proc;
        proc.start(QStringLiteral("ipconfig"), QStringList() << QStringLiteral("getpacket") << i.name());
        if (!proc.waitForFinished(3000)) {
            proc.kill();
            proc.waitForFinished(2000);   // R5-1
            continue;
        }
        const QString out = QString::fromLocal8Bit(proc.readAllStandardOutput());
        if (out.trimmed().isEmpty()) continue;
        QString server, leaseTime, expire;
        for (const auto& line : out.split(QLatin1Char('\n'))) {
            const QString t = line.trimmed();
            if (t.startsWith(QLatin1String("server_identifier"))) server = t.section(QStringLiteral(":"), 1).trimmed();
            else if (t.startsWith(QLatin1String("lease_time"))) leaseTime = t.section(QStringLiteral(":"), 1).trimmed();
            else if (t.startsWith(QLatin1String("expire"))) expire = t.section(QStringLiteral(":"), 1).trimmed();
        }
        ResultProperty p(i.name(), server.isEmpty() ? QStringLiteral("(DHCP packet) — no server id") : server);
        p.children.append({QStringLiteral("DHCP"), QStringLiteral("Yes")});
        if (!server.isEmpty()) p.children.append({QStringLiteral("server"), server});
        if (!leaseTime.isEmpty()) p.children.append({QStringLiteral("lease time"), leaseTime});
        if (!expire.isEmpty()) p.children.append({QStringLiteral("expire"), expire});
        props.append(p);
        ++leaseCount;
        leases.append(i.name() + QLatin1Char('=') + server);
    }
#else
    // Linux/Android：systemd-networkd → dhclient → NetworkManager 租约文件（真实解析）。
    auto addLeaseFile = [&](const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return;
        // 接口名：优先文件内容 `interface "eth0";`（dhclient 格式），
        // 否则由文件名推导：dhclient.<if>.leases → <if>。
        QString fileName = QFileInfo(path).fileName();
        if (fileName.startsWith(QLatin1String("dhclient."))) fileName = fileName.mid(9);
        if (fileName.endsWith(QLatin1String(".leases"))) fileName.chop(8);
        else if (fileName.endsWith(QLatin1String(".lease"))) fileName.chop(6);
        QString ifName = fileName;
        // 5WHY (2026-08-20): systemd-networkd 租约文件名是接口索引（如
        // "4"）而非接口名——索引经 QNetworkInterface 映射回接口名，
        // 否则属性卡以数字索引当接口名（数据错标）。
        bool numericIdx = false;
        const int ifIdx = fileName.toInt(&numericIdx);
        if (numericIdx) {
            const QNetworkInterface ni = QNetworkInterface::interfaceFromIndex(ifIdx);
            if (ni.isValid()) ifName = ni.name();
        }
        QString ipStr, serverStr, expire;
        QTextStream ts(&f);
        while (!ts.atEnd()) {
            // 5WHY (复核 2026-08-19 取消语义): 租约文件解析微秒级——取消窗口
            // 可忽略，保留早停（部分租约数据落盘，与 v0.0.3 行为一致）。
            if (ctx.cancelled.load()) return;
            const QString line = ts.readLine().trimmed();
            if (line.startsWith(QLatin1String("ADDRESS="))) ipStr = line.mid(8);
            else if (line.startsWith(QLatin1String("SERVER_ADDRESS="))) serverStr = line.mid(15);
            else if (line.startsWith(QLatin1String("interface "))) {
                QString in = line.mid(10).trimmed();
                in.remove(QLatin1Char('"')).remove(QLatin1Char('\'')).remove(QLatin1Char(';'));
                if (!in.isEmpty()) ifName = in;
            }
            else if (line.startsWith(QLatin1String("fixed-address "))) ipStr = line.mid(14).remove(QLatin1Char(' ')).remove(QLatin1Char(';'));
            else if (line.contains(QLatin1String("dhcp-server-identifier")))
                serverStr = line.section(QLatin1Char(' '), -1).remove(QLatin1Char(';'));
            else if (line.startsWith(QLatin1String("expire ")))
                expire = line.section(QLatin1Char(' '), 2, 3).remove(QLatin1Char(';'));
        }
        if (ipStr.isEmpty()) return;
        ResultProperty p(ifName, ipStr);
        p.children.append({QStringLiteral("DHCP"), QStringLiteral("Yes")});
        if (!serverStr.isEmpty()) p.children.append({QStringLiteral("server"), serverStr});
        if (!expire.isEmpty()) p.children.append({QStringLiteral("expire"), expire});
        props.append(p);
        ++leaseCount;
        leases.append(ifName + QLatin1Char('=') + ipStr);
    };
    // systemd-networkd
    const QDir sd(QStringLiteral("/run/systemd/netif/leases"));
    if (sd.exists())
        for (const auto& fi : sd.entryInfoList(QDir::Files)) addLeaseFile(fi.absoluteFilePath());
    // dhclient
    const QDir dc(QStringLiteral("/var/lib/dhcp"));
    if (dc.exists())
        for (const auto& fi : dc.entryInfoList({QStringLiteral("dhclient*.leases")}, QDir::Files)) addLeaseFile(fi.absoluteFilePath());
    // NetworkManager
    const QDir nm(QStringLiteral("/var/lib/NetworkManager"));
    if (nm.exists())
        for (const auto& fi : nm.entryInfoList({QStringLiteral("*.lease")}, QDir::Files)) addLeaseFile(fi.absoluteFilePath());
#if defined(__linux__) && !defined(PLATFORM_ANDROID)
    // 5WHY (2026-08-20 用户诉求 "DHCP 信息无数据"): 本机（Radxa ARM,
    // Debian trixie）wlan0 由 NetworkManager 管理——真实租约只存
    // /var/lib/NetworkManager/internal-*（root 0700，应用用户不可读），
    // 上方三个文件源全部落空 → 详情页零数据。v0.0.3 曾以 /proc/net/route
    // 网关回退至少给出 "Likely" 行，移植时丢失。恢复两条非特权数据路径：
    //   1. NetworkManager D-Bus（nmcli -t -m multiline device show）——
    //      免 root 读 DHCP4.OPTION（dhcp_server_identifier/lease time/
    //      ip_address/routers/subnet_mask/host name），桌面 Linux 标准。
    //   2. /proc/net/route 网关回退（v0.0.3 语义：有网关的接口 DHCP 高概率）。
    if (leaseCount == 0 && !ctx.cancelled.load()) {
        const QString nmcli = QStandardPaths::findExecutable(QStringLiteral("nmcli"));
        if (!nmcli.isEmpty()) {
            QProcess proc;
            proc.start(nmcli, QStringList() << QStringLiteral("-t")
                << QStringLiteral("-m") << QStringLiteral("multiline")
                << QStringLiteral("-f")
                << QStringLiteral("GENERAL.DEVICE,GENERAL.TYPE,IP4.ADDRESS,IP4.GATEWAY,DHCP4.OPTION")
                << QStringLiteral("device") << QStringLiteral("show"));
            if (proc.waitForFinished(4000)) {
                const QString text = QString::fromLocal8Bit(proc.readAllStandardOutput());
                QString dev, devType, ipAddr, gateway, server, leaseTime, optIp, subnetMask, hostName;
                auto flushDevice = [&]() {
                    if (dev.isEmpty()) return;
                    const bool dhcpManaged = !server.isEmpty() || !leaseTime.isEmpty() || !optIp.isEmpty();
                    const QString effIp = ipAddr.isEmpty() ? optIp : ipAddr;
                    const QString key = dev;
                    const QString type = devType;
                    dev.clear(); devType.clear();
                    // 回环/隧道设备无 DHCP 语义——过滤，避免 lo/tailscale
                    // 以 "DHCP: No" 行混入租约卡（数据噪声）。
                    if (type == QLatin1String("loopback") || type == QLatin1String("tun")
                        || type == QLatin1String("wireguard")) {
                        ipAddr.clear(); gateway.clear(); server.clear(); leaseTime.clear();
                        optIp.clear(); subnetMask.clear(); hostName.clear();
                        return;
                    }
                    if (!dhcpManaged && effIp.isEmpty()) {
                        ipAddr.clear(); gateway.clear(); server.clear(); leaseTime.clear();
                        optIp.clear(); subnetMask.clear(); hostName.clear();
                        return;   // 无 DHCP 且无地址的设备：无信息可呈现
                    }
                    ResultProperty p(key, effIp.isEmpty() ? QStringLiteral("(DHCP)") : effIp);
                    p.children.append({QStringLiteral("DHCP"), dhcpManaged ? QStringLiteral("Yes") : QStringLiteral("No")});
                    if (!server.isEmpty()) p.children.append({QStringLiteral("server"), server});
                    if (!leaseTime.isEmpty()) {
                        bool ltOk = false;
                        const qulonglong lt = leaseTime.toULongLong(&ltOk);
                        // NM 的无限租约 = 4294967295（UINT32_MAX）
                        p.children.append({QStringLiteral("lease time"),
                            (ltOk && lt >= 4294967295ULL) ? QStringLiteral("infinite") : leaseTime + QStringLiteral(" s")});
                    }
                    if (!gateway.isEmpty()) p.children.append({QStringLiteral("gateway"), gateway});
                    if (!subnetMask.isEmpty()) p.children.append({QStringLiteral("subnet mask"), subnetMask});
                    if (!hostName.isEmpty()) p.children.append({QStringLiteral("host name"), hostName});
                    props.append(p);
                    if (dhcpManaged && !effIp.isEmpty()) {
                        ++leaseCount;
                        leases.append(key + QLatin1Char('=') + effIp);
                    }
                    ipAddr.clear(); gateway.clear(); server.clear(); leaseTime.clear();
                    optIp.clear(); subnetMask.clear(); hostName.clear();
                };
                for (const QString& line : text.split(QLatin1Char('\n'))) {
                    if (ctx.cancelled.load()) break;
                    const QString t = line.trimmed();
                    if (t.isEmpty()) continue;
                    if (t.startsWith(QLatin1String("GENERAL.DEVICE:"))) {
                        flushDevice();
                        dev = t.section(QLatin1Char(':'), 1).trimmed();
                    } else if (t.startsWith(QLatin1String("GENERAL.TYPE:"))) {
                        devType = t.section(QLatin1Char(':'), 1).trimmed();
                    } else if (t.startsWith(QLatin1String("IP4.ADDRESS"))) {
                        ipAddr = t.section(QLatin1Char(':'), 1).trimmed();
                    } else if (t.startsWith(QLatin1String("IP4.GATEWAY:"))) {
                        gateway = t.section(QLatin1Char(':'), 1).trimmed();
                    } else if (t.startsWith(QLatin1String("DHCP4.OPTION"))) {
                        // "dhcp_server_identifier = 192.168.20.1"
                        const QString body = t.section(QLatin1Char(':'), 1).trimmed();
                        const QString name = body.section(QLatin1Char('='), 0, 0).trimmed();
                        const QString value = body.section(QLatin1Char('='), 1).trimmed();
                        if (name == QLatin1String("dhcp_server_identifier")) server = value;
                        else if (name == QLatin1String("dhcp_lease_time")) leaseTime = value;
                        else if (name == QLatin1String("ip_address")) optIp = value;
                        else if (name == QLatin1String("routers") && gateway.isEmpty()) gateway = value;
                        else if (name == QLatin1String("subnet_mask")) subnetMask = value;
                        else if (name == QLatin1String("host_name")) hostName = value;
                    }
                }
                flushDevice();
            } else {
                proc.kill();
                proc.waitForFinished(2000);   // R5-1
            }
        }
    }
    if (leaseCount == 0 && !ctx.cancelled.load()) {
        // v0.0.3 回退语义恢复：租约文件与 NM 均不可得时，用路由表网关
        // 推断 DHCP（"Likely"——诚实标记不确定而非断言）。
        QFile rf(QStringLiteral("/proc/net/route"));
        if (rf.open(QIODevice::ReadOnly)) {
            QTextStream ts(&rf);
            ts.readLine();
            while (!ts.atEnd()) {
                if (ctx.cancelled.load()) break;
                const QString line = ts.readLine().trimmed();
                if (line.isEmpty()) continue;
                const QStringList cols = line.split(QLatin1Char('\t'));
                if (cols.size() >= 3) {
                    bool ok = false;
                    const uint32_t gw = cols[2].toUInt(&ok, 16);
                    if (ok && gw != 0) {
                        const QString gwStr = QStringLiteral("%1.%2.%3.%4")
                            .arg(int(gw & 0xFF)).arg(int((gw >> 8) & 0xFF))
                            .arg(int((gw >> 16) & 0xFF)).arg(int((gw >> 24) & 0xFF));
                        ResultProperty p(cols[0], QStringLiteral("DHCP likely"));
                        p.children.append({QStringLiteral("DHCP"), QStringLiteral("Likely")});
                        p.children.append({QStringLiteral("gateway"), gwStr});
                        props.append(p);
                    }
                }
            }
        }
    }
#endif
#endif
#endif
#endif
    // 5WHY (复核 2026-08-20 取消语义): 租约解析循环内取消仅早停（部分数据
    // 落盘）——曾直接落 Pass。与 ActiveConnections/Netskope 同规则：解析后
    // 统一复查，取消整项计 Cancelled。
    if (ctx.cancelled.load())
        return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));

    if (leaseCount > 0) {
        DiagnosticResult r = makeResult(id, DiagStatus::Pass,
            QStringLiteral("%1 DHCP lease(s): %2").arg(leaseCount).arg(leases.join(QStringLiteral(", "))),
            props, {});
        r.data[QStringLiteral("leaseCount")] = leaseCount;
        r.data[QStringLiteral("leases")] = leases;
        r.narrative = QStringLiteral("Found %1 active DHCP lease(s). "
            "DHCP server and lease timestamps are listed per interface below, "
            "sourced from lease files or NetworkManager (D-Bus).").arg(leaseCount);
        return r;
    }
    DiagnosticResult r = makeResult(id, DiagStatus::Info,
        props.isEmpty() ? QStringLiteral("No DHCP information found")
                        : QStringLiteral("No DHCP lease found (static IP or managed externally)"),
        props, {});
    r.data[QStringLiteral("leaseCount")] = 0;
    r.narrative = props.isEmpty()
        ? QStringLiteral("No DHCP leases or DHCP-derived configuration were found on this device. "
            "The interface may use a static IP, or the DHCP client does not expose lease data "
            "(lease files are unreadable and NetworkManager is not installed).")
        : QStringLiteral("No active DHCP lease was found, but configuration reported by NetworkManager "
            "is listed below — the interface most likely uses a static IP configuration.");
    return r;
}

// ── G1IpConfiguration ─────────────────────────────────────────────────────
static DiagnosticResult probeIpConfig(DiagId id, const QString&, RunContext& ctx) {
    QVector<ResultProperty> props;
    // L7/Linux：默认网关（/proc/net/route）与 DNS（resolv.conf）补入属性
#if defined(__linux__) || defined(__ANDROID__)
    QStringList gateways;
    QFile rf(QStringLiteral("/proc/net/route"));
    if (rf.open(QIODevice::ReadOnly)) {
        QTextStream ts(&rf);
        ts.readLine();
        while (!ts.atEnd()) {
            const QString line = ts.readLine().trimmed();
            const QStringList cols = line.split(QLatin1Char('\t'));
            if (cols.size() >= 8) {
                bool ok = false;
                const uint32_t dest = cols[1].toUInt(&ok, 16);
                const uint32_t gw   = cols[2].toUInt(&ok, 16);
                if (dest == 0 && gw != 0) {
                    QHostAddress a(QStringLiteral("%1.%2.%3.%4")
                        .arg(int(gw & 0xFF)).arg(int((gw >> 8) & 0xFF))
                        .arg(int((gw >> 16) & 0xFF)).arg(int((gw >> 24) & 0xFF)));
                    gateways.append(a.toString());
                }
            }
        }
    }
    QStringList dnsServers;
    QFile df(QStringLiteral("/etc/resolv.conf"));
    if (df.open(QIODevice::ReadOnly)) {
        QTextStream ts(&df);
        while (!ts.atEnd()) {
            const QString line = ts.readLine().trimmed();
            if (line.startsWith(QLatin1String("nameserver")))
                dnsServers.append(line.section(QRegularExpression(QStringLiteral("\\s+")), 1));
        }
    }
#endif
    const auto ifaces = runningInterfaces();
    int addressCount = 0;
    int interfaceCount = 0;
    // 5WHY (2026-08-20 用户诉求 "IP 配置属性卡一团混乱"): 曾以「每个地址
    // 条目 = 一个顶层属性」平铺——双栈接口（v4+v6+链路本地）在 Grouped
    // 布局下渲染出 N 个同名组标题（同一接口重复 N 次组），MAC/网关/DNS
    // 挂在任意首条目上，无层级可读。业界惯例（MyNet 等）：一张卡 = 一个
    // 适配器，卡内行 = CIDR/网关/DNS/MAC。重构为「每接口一个组」：
    // 组标题 = 接口名（主值 = 首个地址），子行 = MAC/各地址(CIDR)/网关/DNS。
    for (const auto& i : ifaces) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        const auto entries = i.addressEntries();
        if (entries.isEmpty()) continue;   // 无地址接口（空 down 口）不入卡
        const QString firstIp = entries.first().ip().toString();
        ResultProperty p(i.name(), firstIp);
        if (!i.hardwareAddress().isEmpty()
            && i.hardwareAddress() != QLatin1String("00:00:00:00:00:00"))
            p.children.append({QStringLiteral("MAC"), i.hardwareAddress()});
        for (const auto& e : entries) {
            const bool isV6 = e.ip().protocol() == QAbstractSocket::IPv6Protocol;
            QString value = e.ip().toString();
            const int prefix = netmaskPrefixLen(e.netmask());   // IPv6/空掩码 → -1
            if (prefix >= 0) value += QStringLiteral("/%1").arg(prefix);
            p.children.append({isV6 ? QStringLiteral("IPv6") : QStringLiteral("IPv4"), value});
            ++addressCount;
        }
#if defined(__linux__) || defined(__ANDROID__)
        if (!gateways.isEmpty())
            p.children.append({QStringLiteral("gateway"), gateways.join(QStringLiteral(", "))});
        if (!dnsServers.isEmpty())
            p.children.append({QStringLiteral("dns"), dnsServers.join(QStringLiteral(", "))});
#endif
        props.append(p);
        ++interfaceCount;
    }
    // 5WHY (复核 2026-08-19 v0.0.3 对等): 主机名曾随 ipconfig 转储呈现
    // （G1IpConfiguration.cpp: Host Name）——现丢失，前置一条补回。
    // 5WHY (复核 2026-08-19 探针线程安全): QHostInfo::localHostName() 在
    // 工作线程上阻塞反查 DNS（Qt 文档明示可阻塞）——改用纯 libc
    // gethostname()（即时、无线程风险）。
    // （IPv6 已随 addressEntries 全族覆盖；DHCP Enabled/DNS 后缀无便携
    // 探测，记录为已知缺口。）
    // 5WHY (复核 2026-08-20 分支可达性): 主机名前置曾位于空检查之前——
    // 空枚举时 props 因主机名行恒非空，Info "No IP configuration found"
    // 分支不可达（空栈误报 Pass）。先判空，再前置主机名。
    if (props.isEmpty())
        return makeResult(id, DiagStatus::Info, QStringLiteral("No IP configuration found"), {}, {});
    char hostBuf[256] = {};
    gethostname(hostBuf, sizeof(hostBuf) - 1);
    // 5WHY (复核 2026-08-20 空行守卫): gethostname 失败（受限命名空间/
    // Android）时缓冲为空——曾仍前置 "Host Name: " 空值行。非空才前置。
    if (hostBuf[0])
        props.prepend({QStringLiteral("Host Name"), QString::fromLocal8Bit(hostBuf)});
    DiagnosticResult r = makeResult(id, DiagStatus::Pass,
        QStringLiteral("IP configuration: %1 interface(s), %2 address(es)")
            .arg(interfaceCount).arg(addressCount), props, {});
    r.data[QStringLiteral("addressCount")] = addressCount;
    r.narrative = QStringLiteral("Host %1 has %2 configured interface(s) with %3 address entr(ies). "
        "Each adapter is one group below: MAC, IPv4/IPv6 addresses (CIDR), plus default gateway "
        "and DNS servers on Linux.")
        .arg(hostBuf[0] ? QString::fromLocal8Bit(hostBuf) : QStringLiteral("(unknown)"))
        .arg(interfaceCount).arg(addressCount);
    return r;
}

// ── G1ActiveConnections ───────────────────────────────────────────────────
static DiagnosticResult probeActiveConnections(DiagId id, const QString&, RunContext& ctx) {
    int count = 0;
    // 5WHY (复核 2026-08-19 语义修正): 全状态枚举后 count 含 LISTEN/
    // TIME_WAIT 等——"established" 摘要若沿用 count 会虚报（Windows/macOS
    // 仍只计 ESTABLISHED）。单独累计 ESTABLISHED；tcpCount 保持总数语义。
    int established = 0;
    QVector<ResultProperty> props;
#if defined(__linux__) || defined(__ANDROID__)
    // 5WHY (复核 2026-08-19 v0.0.3 对等): v0.0.3 以 Proto/Local/Foreign/State
    // 表格呈现全部连接——曾只抓 ESTABLISHED 的本地端点 hex 串。恢复全状态
    // + 可读端点（hex→点分十进制）+ 状态名 + tcp6。
    static const char* kStateNames[12] = {"", "ESTABLISHED", "SYN_SENT",
        "SYN_RECV", "FIN_WAIT1", "FIN_WAIT2", "TIME_WAIT", "CLOSE",
        "CLOSE_WAIT", "LAST_ACK", "LISTEN", "CLOSING"};
    const auto parseSockets = [&](const QString& path, const char* proto) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
        QTextStream in(&f);
        in.readLine(); // header
        while (!in.atEnd()) {
            if (ctx.cancelled.load()) return;
            const QString line = in.readLine();
            if (line.simplified().isEmpty()) continue;
            const auto fields = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (fields.size() < 4) continue;
            const auto decodeEp = [](const QString& ep) {
                // "0100007F:1F90"（IP 小端 hex + 端口 hex）→ "127.0.0.1:8080"
                const auto parts = ep.split(QLatin1Char(':'));
                if (parts.size() != 2) return ep;
                // 5WHY (复核 2026-08-20 数据伪造): 曾共用 ok 标志——tcp6 的
                // 32 位 hex IP 溢出 toUInt（ip=0, ok=false），随后端口解析
                // 成功覆盖 ok=true → 溢出未检出、端点被渲染为 "0.0.0.0:port"
                // （伪造数据而非回退原文）。IP/端口独立标志，任一侧失败
                // 即回退原文 hex。
                bool ipOk = false;
                const uint32_t ip = parts[0].toUInt(&ipOk, 16);
                if (!ipOk) return ep;
                // 5WHY (复核 2026-08-20 截断): toUInt 32 位结果隐式窄化进
                // uint16_t 会静默回绕（"1F90A"→0xF90A）——伪造端口。先取
                // 32 位并范围校验，越界回退原文。
                bool portOk = false;
                const uint32_t portRaw = parts[1].toUInt(&portOk, 16);
                if (!portOk || portRaw > 0xFFFF) return ep;
                const quint16 port = static_cast<quint16>(portRaw);
                return QStringLiteral("%1.%2.%3.%4:%5")
                    .arg(int(ip & 0xFF)).arg(int((ip >> 8) & 0xFF))
                    .arg(int((ip >> 16) & 0xFF)).arg(int((ip >> 24) & 0xFF))
                    .arg(port);
            };
            bool ok = false;
            const int state = fields[3].toInt(&ok, 16);
            const QString stateName = (ok && state >= 1 && state <= 11)
                ? QString::fromLatin1(kStateNames[state]) : QStringLiteral("UNKNOWN");
            props.append({QStringLiteral("connection"),
                QStringLiteral("%1 %2 -> %3 [%4]")
                    .arg(QLatin1String(proto), decodeEp(fields[1]),
                         decodeEp(fields[2]), stateName)});
            ++count;
            if (state == 0x01) ++established;
        }
    };
    parseSockets(QStringLiteral("/proc/net/tcp"), "tcp");
    parseSockets(QStringLiteral("/proc/net/tcp6"), "tcp6");
    // 5WHY (复核 2026-08-19 取消语义): 解析循环内取消仅早停（部分数据落盘
    // 是 v0.0.3 行为）——解析后统一复查：取消整项计 Cancelled，不落 Pass/Info。
    if (ctx.cancelled.load())
        return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
#else
#if defined(_WIN32)
    // GetExtendedTcpTable 真实枚举（含 PID）；仅统计 ESTABLISHED。
    DWORD bufLen = 0;
    GetExtendedTcpTable(nullptr, &bufLen, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    if (bufLen > 0) {
        QByteArray buf((int)bufLen, '\0');
        auto* table = (PMIB_TCPTABLE_OWNER_PID)buf.data();
        if (GetExtendedTcpTable(table, &bufLen, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
            for (DWORD i = 0; i < table->dwNumEntries; ++i) {
                if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
                const MIB_TCPROW_OWNER_PID& row = table->table[i];
                if (row.dwState != MIB_TCP_STATE_ESTAB) continue;
                in_addr la, ra;
                la.S_un.S_addr = row.dwLocalAddr;   // 网络字节序 → inet_ntoa 直接可用
                ra.S_un.S_addr = row.dwRemoteAddr;
                props.append({QStringLiteral("connection"),
                    QStringLiteral("%1:%2 -> %3:%4 (pid %5)")
                        .arg(QString::fromLatin1(inet_ntoa(la)))
                        .arg(ntohs((u_short)row.dwLocalPort))
                        .arg(QString::fromLatin1(inet_ntoa(ra)))
                        .arg(ntohs((u_short)row.dwRemotePort))
                        .arg(row.dwOwningPid)});
                ++count;
            }
        }
    }
#else
#if defined(__APPLE__) && !defined(PLATFORM_IOS)
    // macOS：netstat -an -p tcp 解析（无 /proc；pcblist64 解析复杂度高且不稳定）。
    QProcess proc;
    proc.start(QStringLiteral("netstat"), QStringList() << QStringLiteral("-an") << QStringLiteral("-p") << QStringLiteral("tcp"));
    if (proc.waitForFinished(3000)) {
        const QString out = QString::fromLocal8Bit(proc.readAllStandardOutput());
        for (const auto& line : out.split(QLatin1Char('\n'))) {
            if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
            if (!line.contains(QLatin1String("ESTABLISHED"))) continue;
            const QStringList f = line.simplified().split(QLatin1Char(' '));
            if (f.size() >= 4) {   // tcp4  local.addr.port  remote.addr.port  ESTABLISHED
                props.append({QStringLiteral("connection"),
                    QStringLiteral("%1 -> %2").arg(f[1], f[2])});
                ++count;
            }
        }
    } else {
        proc.kill();
        proc.waitForFinished(2000);   // R5-1
    }
#else
    // iOS：沙箱不暴露连接表（netstat 不存在）——诚实说明。
    props.append({QStringLiteral("iOS"), QStringLiteral("connection table not exposed to third-party apps")});
#endif
#endif
#endif
#if !defined(__linux__) && !defined(__ANDROID__)
    established = count;   // Windows/macOS 仅枚举 ESTABLISHED（Linux 在解析内累计）
#endif
    DiagnosticResult r = makeResult(id, count > 0 ? DiagStatus::Pass : DiagStatus::Info,
        QStringLiteral("%1 TCP connection(s), %2 established").arg(count).arg(established), props, {});
    r.data[QStringLiteral("tcpCount")] = count;
    r.data[QStringLiteral("establishedCount")] = established;
    r.narrative = QStringLiteral("%1 TCP connection(s) enumerated, of which %2 established. "
        "Local/remote endpoints are listed in the property card below.").arg(count).arg(established);
    return r;
}

// ── G1CellularInfo ────────────────────────────────────────────────────────
static DiagnosticResult probeCellular(DiagId id, const QString&, RunContext& ctx) {
    QVector<ResultProperty> props;
    QStringList out;
    out.append(QStringLiteral("Cellular Information"));
    out.append(QString());
    bool found = false;
    QStringList foundNames;

#if defined(__linux__) && !defined(PLATFORM_ANDROID)
    // 5WHY (2026-08-20 用户诉求 "Cellular 信息不完全"): v0.0.3 仅在 iOS
    // 输出 Raw 数据（Carrier/Radio Access/IP/Gateway/Signal/SIM 卡槽），
    // 桌面移植版只剩接口名+MAC——Raw 数据丢失。桌面 Linux 的标准蜂窝
    // 管理栈是 ModemManager（mmcli，免 root 读运营商/制式/信号/IMEI），
    // 有则完整转储；无则诚实地只报接口枚举（下方）。
    const QString mmcli = QStandardPaths::findExecutable(QStringLiteral("mmcli"));
    if (!mmcli.isEmpty()) {
        QProcess listProc;
        listProc.start(mmcli, QStringList() << QStringLiteral("-L"));
        if (listProc.waitForFinished(3000)) {
            const QString listOut = QString::fromLocal8Bit(listProc.readAllStandardOutput());
            static const QRegularExpression kModemRe(QStringLiteral("Modem/(\\d+)"));
            auto it = kModemRe.globalMatch(listOut);
            while (it.hasNext()) {
                if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
                const QRegularExpressionMatch m = it.next();
                const QString num = m.captured(1);
                QProcess detailProc;
                detailProc.start(mmcli, QStringList() << QStringLiteral("-m") << num);
                if (!detailProc.waitForFinished(3000)) {
                    detailProc.kill();
                    detailProc.waitForFinished(2000);   // R5-1
                    continue;
                }
                const QString text = QString::fromLocal8Bit(detailProc.readAllStandardOutput());
                QString carrier, rat, signal, mccmnc, state, imei, device;
                for (const QString& rawLine : text.split(QLatin1Char('\n'))) {
                    const QString t = rawLine.trimmed();
                    const int colon = t.indexOf(QLatin1Char(':'));
                    if (colon < 0) continue;
                    const QString key = t.left(colon).trimmed();
                    const QString value = t.mid(colon + 1).trimmed();
                    if (key == QLatin1String("operator name")) carrier = value;
                    else if (key == QLatin1String("access tech")) rat = value;
                    else if (key == QLatin1String("signal quality")) signal = value;
                    else if (key == QLatin1String("operator code")) mccmnc = value;
                    else if (key == QLatin1String("state")) state = value;
                    else if (key == QLatin1String("imei")) imei = value;
                    else if (key == QLatin1String("device")) device = value;
                }
                if (carrier.isEmpty() && rat.isEmpty() && state.isEmpty()) continue;
                ResultProperty p(QStringLiteral("modem %1").arg(num),
                    state.isEmpty() ? QStringLiteral("present") : state);
                if (!carrier.isEmpty()) p.children.append({QStringLiteral("carrier"), carrier});
                if (!rat.isEmpty()) p.children.append({QStringLiteral("radio access"), rat});
                if (!signal.isEmpty()) p.children.append({QStringLiteral("signal"), signal});
                if (!mccmnc.isEmpty()) p.children.append({QStringLiteral("MCC/MNC"), mccmnc});
                if (!imei.isEmpty()) p.children.append({QStringLiteral("IMEI"), imei});
                if (!device.isEmpty()) p.children.append({QStringLiteral("device"), device});
                props.append(p);
                found = true;
                foundNames.append(carrier.isEmpty() ? QStringLiteral("modem %1").arg(num) : carrier);
                // Raw 数据转储（历史版本样式）
                out.append(QStringLiteral("  Modem %1:").arg(num));
                for (const QString& rawLine : text.split(QLatin1Char('\n'))) {
                    const QString t = rawLine.trimmed();
                    if (!t.isEmpty()) out.append(QStringLiteral("    %1").arg(t));
                }
                out.append(QString());
            }
        } else {
            listProc.kill();
            listProc.waitForFinished(2000);   // R5-1
        }
    }
#endif

    // 接口枚举（各平台）：wwan/cellular/rmnet/pdp 命名的接口 + 地址
    const auto ifaces = runningInterfaces();
    for (const auto& i : ifaces) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        if (i.name().contains(QLatin1String("wwan"), Qt::CaseInsensitive)
            || i.name().contains(QLatin1String("cellular"), Qt::CaseInsensitive)
            || i.name().contains(QLatin1String("rmnet"), Qt::CaseInsensitive)
            || i.name().contains(QLatin1String("pdp"), Qt::CaseInsensitive)) {
            ResultProperty p(i.name(), i.hardwareAddress().isEmpty()
                ? QStringLiteral("(no MAC)") : i.hardwareAddress());
            for (const auto& e : i.addressEntries())
                p.children.append({e.ip().protocol() == QAbstractSocket::IPv6Protocol
                    ? QStringLiteral("IPv6") : QStringLiteral("IPv4"), e.ip().toString()});
            props.append(p);
            found = true;
            foundNames.append(i.name());
            out.append(QStringLiteral("  %1  MAC %2").arg(i.name(), i.hardwareAddress()));
        }
    }

    if (!found)
        return makeResult(id, DiagStatus::Skipped, QStringLiteral("No cellular modem present"), {}, {});

    // details 显式给 Raw 转储——终端区块呈现原始数据（用户诉求
    // "尽可能展示 Raw 数据"）；属性卡呈现结构化字段，两者并存。
    DiagnosticResult r = makeResult(id, DiagStatus::Pass,
        QStringLiteral("Cellular modem detected: %1").arg(foundNames.join(QStringLiteral(", "))),
        props, out.join(QLatin1Char('\n')));
    r.narrative = QStringLiteral("Cellular modem(s) detected: %1. "
        "Carrier, radio access technology, signal and MCC/MNC are listed in the property cards; "
        "the raw ModemManager/interface dump is in the terminal section.")
        .arg(foundNames.join(QStringLiteral(", ")));
    return r;
}

} // namespace g1

// ── 平台深探针（iOS GatewayDhcpRouting / Android JNI）───────────────────
#if defined(PLATFORM_IOS)
#include "Diagnostics/Model/G1/Platform/IOS/GatewayDhcpRouting.h"
namespace {
DiagnosticResult iosWifiProbe(DiagId id, const QString&, RunContext&) {
    const QVariantMap info = iosWiFiInfo();
    if (info.isEmpty())
        return DiagnosticResult::skipped(id, QStringLiteral("No WiFi interface present"));
    QVector<ResultProperty> props;
    auto add = [&props](const char* label, const QString& v) {
        if (!v.isEmpty()) props.append({QLatin1String(label), v});
    };
    add("SSID", info.value(QStringLiteral("ssid")).toString());
    add("BSSID", info.value(QStringLiteral("bssid")).toString());
    add("signal dBm", info.value(QStringLiteral("rssi")).toString());
    add("channel", info.value(QStringLiteral("channel")).toString());
    add("security", info.value(QStringLiteral("security")).toString());
    // 5WHY (2026-08-20 用户诉求 "WiFi 信息无数据"): 曾手搭结果、无
    // narrative——摘要卡缺失且 details 为空（终端区块隐藏）。统一走
    // g1::makeResult（属性派生转储 + 摘要叙述同源）。
    DiagnosticResult r = g1::makeResult(id, DiagStatus::Pass,
        QStringLiteral("WiFi: %1").arg(info.value(QStringLiteral("ssid")).toString()),
        props, {});
    r.narrative = QStringLiteral("Wi-Fi is connected to %1 (BSSID %2). "
        "Signal, channel and security are listed in the property cards below.")
        .arg(info.value(QStringLiteral("ssid")).toString(),
             info.value(QStringLiteral("bssid")).toString());
    return r;
}

DiagnosticResult iosCellularProbe(DiagId id) {
    const QVariantMap info = iosCellularInfo();
    // 5WHY (复核 2026-08-19 v0.0.3 对等): v0.0.3 的 "No cellular service"
    // 是 Info 态（G1CellularInfo.cpp）——曾返回 skipped 丢失该呈现。
    // iOS 设备恒有调制解调器：空图即"无服务"而非"无硬件"。
    if (info.isEmpty()) {
        // 5WHY (复核 2026-08-19 simplify): 曾手搭 DiagnosticResult（10 行
        // 样板）——makeResult 的空 details 分支自动生成同文本转储并打
        // propsDump 标记（终端区块对属性派生转储让位），一行等价且随
        // 归一化契约演进（时间戳/转储规则不再旁路）。
        DiagnosticResult nr = g1::makeResult(id, DiagStatus::Info,
            QStringLiteral("No cellular service"),
            {{QStringLiteral("cellular"), QStringLiteral("No cellular service available")}}, {});
        nr.narrative = QStringLiteral("No cellular service is currently available on this device "
            "(no SIM registered with a usable data plan, or the modem is offline).");
        return nr;
    }
    QVector<ResultProperty> props;
    auto add = [&props](const char* label, const QString& v) {
        if (!v.isEmpty()) props.append({QLatin1String(label), v});
    };
    add("carrier", info.value(QStringLiteral("carrierName")).toString());
    add("radio access", info.value(QStringLiteral("radioAccess")).toString());
    add("MCC", info.value(QStringLiteral("mcc")).toString());
    add("MNC", info.value(QStringLiteral("mnc")).toString());
    add("signal", info.value(QStringLiteral("signalStrength")).toString());
    // 5WHY (复核 2026-08-19 v0.0.3 对等): 数据 IP/网关/多卡槽位（SIM-by-SIM）
    // 曾随表呈现——ObjC 侧若提供相应键即呈现（防御性读取；键缺失不崩）。
    add("Data IP", info.value(QStringLiteral("dataIp")).toString());
    add("Gateway", info.value(QStringLiteral("gateway")).toString());
    // 5WHY (复核 2026-08-19 CI 失败根因): 变量名曾为 `slots`——Qt 关键字宏
    // （#define slots Q_SLOTS，Apple 构建未定义 QT_NO_KEYWORDS）展开成
    // Q_SLOTS → "expected unqualified-id"。本段在 __APPLE__ 门内，Linux
    // 本地构建不编译、CI 才暴露。改名避开关键字。
    const QVariantList simSlotList = info.value(QStringLiteral("simSlots")).toList();
    for (const QVariant& s : simSlotList) {
        const QVariantMap sim = s.toMap();
        if (sim.isEmpty()) continue;
        props.append({QStringLiteral("SIM %1").arg(sim.value(QStringLiteral("slot")).toString()),
            QStringLiteral("%1 / %2").arg(sim.value(QStringLiteral("carrier")).toString(),
                                          sim.value(QStringLiteral("rat")).toString())});
    }
    DiagnosticResult r = g1::makeResult(id, DiagStatus::Pass,
        QStringLiteral("Carrier: %1").arg(info.value(QStringLiteral("carrierName")).toString()),
        props, {});
    // 摘要卡叙述（与桌面/Android 同构）：运营商 → 制式 → 信号 → 承载
    r.narrative = QStringLiteral("Carrier %1 on %2 (MCC %3, MNC %4). "
        "Signal, data IP, gateway and SIM slots are listed in the property cards below.")
        .arg(info.value(QStringLiteral("carrierName")).toString(),
             info.value(QStringLiteral("radioAccess")).toString(),
             info.value(QStringLiteral("mcc")).toString(),
             info.value(QStringLiteral("mnc")).toString());
    return r;
}
} // namespace
#endif

// ── Registration ───────────────────────────────────────────────────────────
// Called by registerAllAdapters() from main() (DIAG-2/A1: explicit init).
void registerG1Adapters() {
    using g1::probeNetworkAdapters; using g1::probeNicAdvanced;
    using g1::probeWifi; using g1::probeWired; using g1::probeDhcp;
    using g1::probeIpConfig; using g1::probeActiveConnections; using g1::probeCellular;

#if defined(PLATFORM_ANDROID)
    // Android：全部 8 项均有 JNI 深探测实现（NetworkDiagnostics.cpp）
    AdapterRegistry::registerAdapters(DiagId::G1NetworkAdapters, {
        { PF_Android, "Android", {}, [](DiagId i, const QString&, RunContext&) { return androidNetworkAdaptersDiag(i); } },
    });
    AdapterRegistry::registerAdapters(DiagId::G1NicAdvanced, {
        { PF_Android, "Android", {}, [](DiagId i, const QString&, RunContext&) { return androidNicAdvancedDiag(i); } },
    });
    AdapterRegistry::registerAdapters(DiagId::G1WifiDiagnostics, {
        { PF_Android, "Android", {}, [](DiagId i, const QString&, RunContext&) { return androidWifiDiag(i); } },
    });
    AdapterRegistry::registerAdapters(DiagId::G1WiredDiagnostics, {
        { PF_Android, "Android", {}, [](DiagId i, const QString&, RunContext&) { return androidWiredDiagnosticsDiag(i); } },
    });
    AdapterRegistry::registerAdapters(DiagId::G1DhcpStatus, {
        { PF_Android, "Android", {}, [](DiagId i, const QString&, RunContext&) { return androidDhcpDiag(i); } },
    });
    AdapterRegistry::registerAdapters(DiagId::G1IpConfiguration, {
        { PF_Android, "Android", {}, [](DiagId i, const QString&, RunContext&) { return androidIpConfigurationDiag(i); } },
    });
    AdapterRegistry::registerAdapters(DiagId::G1ActiveConnections, {
        { PF_Android, "Android", {}, [](DiagId i, const QString&, RunContext&) { return androidActiveConnectionsDiag(i); } },
    });
    AdapterRegistry::registerAdapters(DiagId::G1CellularInfo, {
        { PF_Android, "Android", {}, [](DiagId i, const QString&, RunContext&) { return androidCellularDiag(i); } },
    });
    return;
#else
#if defined(PLATFORM_IOS)
    // iOS：DHCP/网关经系统 API；WiFi/蜂窝经 CNCopyCurrentNetworkInfo/CTTelephony
    AdapterRegistry::registerAdapters(DiagId::G1DhcpStatus, {
        { PF_IOS, "iOS", {}, [](DiagId i, const QString&, RunContext&) { return iosDhcpDiag(i); } },
    });
    AdapterRegistry::registerAdapters(DiagId::G1WifiDiagnostics, {
        { PF_IOS, "iOS", {}, [](DiagId i, const QString& t, RunContext& ctx) { return iosWifiProbe(i, t, ctx); } },
    });
    AdapterRegistry::registerAdapters(DiagId::G1CellularInfo, {
        { PF_IOS, "iOS", {}, [](DiagId i, const QString&, RunContext&) { return iosCellularProbe(i); } },
    });
#endif
#endif

    AdapterRegistry::registerAdapters(DiagId::G1NetworkAdapters, {
        { PF_Desktop, "Desktop", {}, probeNetworkAdapters },
#if !defined(PLATFORM_ANDROID)
        { PF_IOS,     "iOS",     {}, probeNetworkAdapters },
#endif
    });
    AdapterRegistry::registerAdapters(DiagId::G1NicAdvanced, {
        { PF_Desktop, "Desktop", {}, probeNicAdvanced },
    });
    AdapterRegistry::registerAdapters(DiagId::G1WifiDiagnostics, {
        { PF_Desktop, "Desktop", {}, probeWifi },
    });
    AdapterRegistry::registerAdapters(DiagId::G1WiredDiagnostics, {
        { PF_Desktop, "Desktop", {}, probeWired },
    });
    AdapterRegistry::registerAdapters(DiagId::G1DhcpStatus, {
        { PF_Desktop, "Desktop", {}, probeDhcp },
    });
    AdapterRegistry::registerAdapters(DiagId::G1IpConfiguration, {
        { PF_Desktop, "Desktop", {}, probeIpConfig },
#if !defined(PLATFORM_ANDROID)
        { PF_IOS,     "iOS",     {}, probeIpConfig },
#endif
    });
    AdapterRegistry::registerAdapters(DiagId::G1ActiveConnections, {
        { PF_Desktop, "Desktop", {}, probeActiveConnections },
    });
    AdapterRegistry::registerAdapters(DiagId::G1CellularInfo, {
        { PF_Desktop, "Desktop", {}, probeCellular },
    });
}
