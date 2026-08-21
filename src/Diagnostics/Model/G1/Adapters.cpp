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
#include "Diagnostics/View/DiagnosticFormatter.h"   // v0.0.3 复刻 details（DHCP Apple 桩表）
#include "Diagnostics/Model/GHelpers.h"   // iOS 蜂窝/WiFi 助手（v0.0.3 复刻）
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
#include <QHash>
#include <QStandardPaths>
#include <QMutexLocker>

#include <cstring>
#include <memory>
#include <vector>

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
#include "Diagnostics/Model/GHelpers.h"   // ipToStr/wifiChannelFromFreqMhz/cachedRunTool/readProcLines

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

// 5WHY (复核 2026-08-20 iOS 编译失败 "unknown type name 'QProcess'"):
// QtCore 在 iOS 上定义 QT_NO_PROCESS（无进程模型）——qprocess.h 存在但
// 类声明被 QT_CONFIG(process) 编译为空，引用 QProcess 即编译错误（且无
// "file not found" 可定位）。cachedRunTool（GHelpers.h）自身以
// QT_NO_PROCESS 守卫，但 nmcliDeviceShow 的调用方（probeDhcp/probeIpConfig
// 的 nmcli 块）都在 Linux 守卫内——定义与调用方同守卫（Linux 且非
// Android），iOS 完全跳过。G3 已有同款先例。
#if defined(__linux__) && !defined(PLATFORM_ANDROID)
// `nmcli -t -m multiline device show` 每轮快照。5WHY (复核 2026-08-20 双
// spawn): probeDhcp（DHCP4.OPTION）与 probeIpConfig（IP4.DNS）字段出自同
// 一次输出——池线程并行时曾每轮 spawn 两次 nmcli（各带 4s 超时）。经
// SystemDiagnostics::cachedRunTool（RunSnapshot 按 exe+args 互斥缓存）
// 一轮只 spawn 一次；无快照（harness/单探针直跑）时直接执行。参数向量
// 单一来源（曾两分支各写一份，加字段漏一处即两路径数据分叉）。
static const QStringList& nmcliShowArgs() {
    static const QStringList kArgs = QStringList() << QStringLiteral("-t")
        << QStringLiteral("-m") << QStringLiteral("multiline")
        << QStringLiteral("-f")
        << QStringLiteral("GENERAL.DEVICE,GENERAL.TYPE,IP4.ADDRESS,IP4.GATEWAY,IP4.DNS,DHCP4.OPTION")
        << QStringLiteral("device") << QStringLiteral("show");
    return kArgs;
}

static QString nmcliDeviceShow(RunContext& ctx) {
    const QString nmcli = QStandardPaths::findExecutable(QStringLiteral("nmcli"));
    if (nmcli.isEmpty()) return QString();
    return SystemDiagnostics::cachedRunTool(ctx, nmcli, nmcliShowArgs(), 4000);
}
#endif // defined(__linux__) && !defined(PLATFORM_ANDROID)

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
    // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): 曾于探针层以 propsDumpText
    // 派生 details（propsDump 标记）——"label: value" 平铺与 v0.0.3 逐
    // 探针格式化文本（ipconfig 风格头 + 列对齐表）不符，且与呈现层派生
    // 构成双路径。派生统一收敛到呈现层：resultFor 对空 details 以
    // LegacyTerminalFormat 逐字复刻；本层原样透传（propsDump 标记删除，
    // 剪贴板与终端共用呈现层同源派生）。
    r.details = details;
    r.rawOutput = details;
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

// 频率(GHz) → 信道号。5WHY (复核 2026-08-20 频段混淆): 曾单一公式
// (f-2.412)/0.005+1 并把 5.1–7.2 GHz 判为 plausible——该公式仅对
// 2.4 GHz 有效（5 GHz 信道 36 @ 5.180 GHz 会被算成 554），门控"诚实
// 缺省"因此形同虚设。
// 5WHY (复核 2026-08-20 双份漂移): 频段表曾本文件（GHz double）与
// Android G5（MHz int）各一份且边界不一致（5885 MHz 在 Linux 判
// 5 GHz、Android 判 6 GHz）——算术收敛 SystemDiagnostics::wifiChannelFromFreqMhz
// （nl80211 定义 + 带内钳制：2.4 GHz 带顶不再算出信道 15），本文件仅换算单位。
// 返回 0 = 非 WiFi 频段（未关联 AP / 驱动不报频段）→ 呈现 "-"。
static int wifiChannelFromFreq(double freq) {
    return SystemDiagnostics::wifiChannelFromFreqMhz(freq * 1000.0);
}

// ── 现代 Linux 无线接口判据（5WHY 2026-08-20 用户诉求 "WiFi 无数据"）──
// 旧判据只认 /sys/class/net/<if>/wireless（wext 目录）——内核 6.6+ 的
// mac80211 不再创建该目录（本机 wlan0 即无此目录），过滤恒空 → 探针
// Skipped、详情页零数据。现代判据按可靠性排序：
//   1. phy80211 符号链接（mac80211 始终创建，无线网卡专属）
//   2. uevent DEVTYPE=wlan
//   3. 旧 wext 目录（旧内核兼容）
//   4. /proc/net/wireless 行成员（最后手段；levels 表探针级解析一次传入）
#if defined(__linux__)
static QHash<QString, QString> wirelessLevels();   // 定义见下（单次解析 /proc/net/wireless）
static bool isWirelessInterface(const QString& ifName,
                                const QHash<QString, QString>* levels = nullptr) {
    if (QFileInfo::exists(QStringLiteral("/sys/class/net/%1/phy80211").arg(ifName)))
        return true;
    QFile uevent(QStringLiteral("/sys/class/net/%1/uevent").arg(ifName));
    if (uevent.open(QIODevice::ReadOnly)
        && QString::fromLatin1(uevent.readAll()).contains(QLatin1String("DEVTYPE=wlan")))
        return true;
    if (QFile::exists(QStringLiteral("/sys/class/net/%1/wireless").arg(ifName)))
        return true;
    if (levels) return levels->contains(ifName);
    return wirelessLevels().contains(ifName);   // 单接口调用场景：解析一次
}

// /proc/net/wireless 单次解析：接口名 → 信号电平列。5WHY (复核
// 2026-08-20 三份同构): 曾 wirelessInterfaceNames 集合、isWirelessInterface
// 单读回退、probeWifi 信号列逐接口整读——同一文件三份表头跳过逻辑各写
// 一遍（列索引曾漂移 cols[4]→cols[3]）。readProcLines 共享读取 + 单份
// 列解析：level 供 WiFi 卡信号呈现，键供判据查表。
static QHash<QString, QString> wirelessLevels() {
    QHash<QString, QString> out;
    const QStringList lines = SystemDiagnostics::readProcLines(QStringLiteral("/proc/net/wireless"), 2);
    for (const QString& raw : lines) {
        const QString t = raw.trimmed();
        const int colon = t.indexOf(QLatin1Char(':'));
        if (colon < 0) continue;
        const QString name = t.left(colon).trimmed();
        // 列：status link level noise...（与 v0.0.3 一致取 level 列）
        const QStringList cols = t.mid(colon + 1).trimmed()
            .split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (!name.isEmpty() && cols.size() >= 3) out.insert(name, cols[2]);
    }
    return out;
}
#endif

// /proc/net/route 网关行解析：接口名 → 网关列表（小端 hex → 点分十进制
// 经 SystemDiagnostics::ipToStr）。defaultOnly=true 仅默认路由（dest==0）。
// 5WHY (复核 2026-08-20 三份同构): 曾 probeDhcp/probeIpConfig/G2 各写一份
// 解析且列约束已漂移（>=3 vs >=8 vs metric 进制）——本文件两探针收敛；
// G2 保留其 metric 语义解析。
#if defined(__linux__) || defined(__ANDROID__)
static QHash<QString, QStringList> routeGateways(bool defaultOnly) {
    QHash<QString, QStringList> out;
    const QStringList lines = SystemDiagnostics::readProcLines(QStringLiteral("/proc/net/route"), 1);
    for (const QString& raw : lines) {
        const QString t = raw.trimmed();
        if (t.isEmpty()) continue;
        const QStringList cols = t.split(QLatin1Char('\t'));
        if (cols.size() < 3) continue;
        bool ok = false;
        const uint32_t gw = cols[2].toUInt(&ok, 16);
        if (!ok || gw == 0) continue;
        if (defaultOnly) {
            const uint32_t dest = cols[1].toUInt(&ok, 16);
            if (!ok || dest != 0) continue;
        }
        out[cols[0]].append(SystemDiagnostics::ipToStr(gw));
    }
    return out;
}
#endif

// ── G1NetworkAdapters ────────────────────────────────────────────────────
static DiagnosticResult probeNetworkAdapters(DiagId id, const QString&, RunContext& ctx) {
    QVector<ResultProperty> props;
    const auto ifaces = runningInterfaces();
#if defined(__linux__)
    // /proc/net/wireless 探针级解析一次（逐接口判据查表，见 isWirelessInterface）
    const QHash<QString, QString> wirelessLevelsMap = wirelessLevels();
#endif
    for (const auto& i : ifaces) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        ResultProperty p(i.name(), i.hardwareAddress().isEmpty()
                         ? QStringLiteral("no MAC") : i.hardwareAddress());
        QString type = (i.flags().testFlag(QNetworkInterface::IsLoopBack))
                     ? QStringLiteral("loopback")
                     : QStringLiteral("ethernet");
#if defined(__linux__)
        // 5WHY (2026-08-20 分类失真): 无线接口（wlan0）曾被归类 ethernet
        // ——用与 WiFi 探针同源的现代判据（phy80211/uevent）标注 wireless。
        if (isWirelessInterface(i.name(), &wirelessLevelsMap)) type = QStringLiteral("wireless");
#endif
        p.children.append({QStringLiteral("type"), type});
        // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): v0.0.3 适配器表含
        // MTU/Status 列——属性卡曾无此二字段，复刻层无法重建。
        // 5WHY (复核 2026-08-21 跨平台伪 DOWN): 曾 Linux-only——macOS/
        // Windows 复刻表恒 "DOWN"/"-"（活动 en0 亦 DOWN，v0.0.3 macOS
        // 经 ioctl SIOCGIFMTU/SIOCGIFFLAGS 报真实 MTU/UP）。MTU 改用
        // Qt 便携 maximumTransmissionUnit；Status：Linux 读 /sys operstate
        // （大写，v0.0.3 语义），其余平台以 IsUp 标志判 UP/DOWN；
        // 缺省 DOWN，loopback 恒 UP——v0.0.3 同。
        p.children.append({QStringLiteral("MTU"), QString::number(i.maximumTransmissionUnit())});
        QString st = QStringLiteral("DOWN");
#if defined(__linux__)
        QFile opFile(QStringLiteral("/sys/class/net/%1/operstate").arg(i.name()));
        if (opFile.open(QIODevice::ReadOnly))
            st = QString::fromLatin1(opFile.readAll().trimmed()).toUpper();
#else
        if (i.flags().testFlag(QNetworkInterface::IsUp)) st = QStringLiteral("UP");
#endif
        if (type == QLatin1String("loopback")) st = QStringLiteral("UP");
        p.children.append({QStringLiteral("status"), st});
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
        QString rawSpeed;
        QFile sf(base + QStringLiteral("speed"));
        if (sf.open(QIODevice::ReadOnly)) {
            rawSpeed = QString::fromLatin1(sf.readAll().trimmed());
            bool ok = false;
            const quint64 mbps = rawSpeed.toULongLong(&ok);
            if (ok && mbps > 0) bps = mbps * 1000000ULL;
        }
        QString duplex;
        QFile df(base + QStringLiteral("duplex"));
        if (df.open(QIODevice::ReadOnly))
            duplex = QString::fromLatin1(df.readAll().trimmed());
        const QString speed = formatLinkSpeed(bps);
        ResultProperty p(i.name(), speed);
        if (bps > 0) { knownSpeeds.append(i.name() + QLatin1Char('=') + speed); ++speedKnown; }
        // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): 复刻层 Speed 列需
        // sysfs 裸 Mbps（v0.0.3 rd("speed") 原值——"10000" 非 "10.0 Gbps"）
        // ——主值存格式化串（属性卡），补 "speed" 子属性存原值。
        if (!rawSpeed.isEmpty())
            p.children.append({QStringLiteral("speed"), rawSpeed});
        p.children.append({QStringLiteral("MTU"), QString::number(i.maximumTransmissionUnit())});
        p.children.append({QStringLiteral("duplex"), duplex.isEmpty() ? QStringLiteral("unknown") : duplex});
        p.children.append({QStringLiteral("MAC"), i.hardwareAddress()});
        // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): v0.0.3 高级属性表含
        // Carrier/State 列（/sys carrier/operstate）——补子行。
        QFile cf(base + QStringLiteral("carrier"));
        if (cf.open(QIODevice::ReadOnly))
            p.children.append({QStringLiteral("carrier"),
                QString::fromLatin1(cf.readAll().trimmed()) == QLatin1String("1")
                    ? QStringLiteral("1") : QStringLiteral("0")});
        QFile of(base + QStringLiteral("operstate"));
        if (of.open(QIODevice::ReadOnly))
            p.children.append({QStringLiteral("state"), QString::fromLatin1(of.readAll().trimmed())});
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
    // 5WHY (复核 2026-08-21 用户 "WiFi 没有任何输出"): 曾只填属性卡、
    // rawOutput 恒空——v0.0.3 的终端表格（Interface/SSID/BSSID/Channel/
    // Signal/Bitrate 对齐表）在重构中丢失，详情页终端区块空白；且无无线
    // 接口时曾 Skipped（瓦片隐藏）→ 详情页整体无输出。复刻 v0.0.3：
    // 表格入 rawOutput；无接口时 Info（可见）+ "(no wireless interfaces
    // detected)" 文本，永不静默。
    QStringList out;
    QList<QStringList> wifiRows;
#if defined(PLATFORM_IOS)
    // v0.0.3：iosWiFiInfo 在循环前一次性调用（NEHotspotNetwork 逐接口
    // 查询可阻塞至 5s，热点模式下 en0/en1/en2 三接口累计 15s）。
    QString cachedIosSsid, cachedIosBssid, wifiDiagnosticMsg;
    {
        QVariantMap wifiData = iosWiFiInfo();
        cachedIosSsid = wifiData.value(QStringLiteral("ssid"), QString()).toString();
        cachedIosBssid = wifiData.value(QStringLiteral("bssid"), QString()).toString();
        wifiDiagnosticMsg = wifiData.value(QStringLiteral("wifiDiagnostics"), QString()).toString();
    }
#endif
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
                wifiRows.append({QString::fromWCharArray(wi.strInterfaceDescription),
                    ssid, bssid, channel, signal, QStringLiteral("-")});
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
#if defined(__linux__)
    // /proc/net/wireless 探针级解析一次（逐接口判据 + 信号列共用，
    // 见 isWirelessInterface/wirelessLevels）。5WHY (复核 2026-08-20
    // Android 编译破坏): 本声明曾限 !PLATFORM_ANDROID 而下方非 Apple
    // 分支 Android 也编译——undeclared identifier 使 Android 构建失败。
    // 守卫与使用同宽（Android 有 /proc/net/wireless）。
    const QHash<QString, QString> wirelessLevelsMap = wirelessLevels();
#endif
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
            if (!isWirelessInterface(ifName, &wirelessLevelsMap)) continue;
#endif
            seen.insert(ifName);
            QString ssid = QStringLiteral("-"), bssid = QStringLiteral("-");
            QString channel = QStringLiteral("-"), signal = QStringLiteral("-"), bitrate = QStringLiteral("-");
#if defined(PLATFORM_IOS)
            // v0.0.3：循环前缓存的 NEHotspot 数据（逐接口查询阻塞风险）
            ssid = cachedIosSsid.isEmpty() ? QStringLiteral("-") : cachedIosSsid;
            bssid = cachedIosBssid.isEmpty() ? QStringLiteral("-") : cachedIosBssid;
#else
#if defined(__APPLE__)
            const QString s = macosWifiSsid();
            if (!s.isEmpty()) ssid = s;
            const QString b = macosWifiBssid();
            if (!b.isEmpty()) bssid = b;
#endif
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
                    // 5WHY (复核 2026-08-20 未关联状态): ioctl 成功但频率
                    // 为微小正值（未关联 AP / 驱动不报频段）时曾算出
                    // channel=-481 "(-481 (0.000 GHz))"。仅当频率落在
                    // 真实 WiFi 频段（2.4/5/6 GHz）内才换算信道，
                    // 否则保持 "-"（诚实缺省）。分频段公式见
                    // wifiChannelFromFreq（5/6 GHz 曾误用 2.4 GHz 公式）。
                    const int chan = wifiChannelFromFreq(freq);
                    if (chan > 0)
                        channel = QStringLiteral("%1 (%2 GHz)")
                            .arg(chan)
                            .arg(QString::number(freq, 'f', 3));
                }
                ::close(sock);
            }
            // 信号电平：wirelessLevels 探针级单次解析的 level 列（v0.0.3
            // 语义：取 level 而非噪声列——本机 -20 dBm；曾逐接口整读
            // /proc/net/wireless N+1 次且每行编译一次正则）。
            const QString lvl = wirelessLevelsMap.value(ifName);
            if (!lvl.isEmpty()) {
                QString sig = lvl;
                sig.remove(QLatin1Char('.'));
                signal = sig + QStringLiteral(" dBm");
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
                    // cachedRunTool：同一命令一轮只跑一次（按 exe+args 缓存）
                    const QStringList iwl = SystemDiagnostics::cachedRunTool(ctx, iw,
                        QStringList() << QStringLiteral("dev") << ifName << QStringLiteral("link"), 3000)
                        .split(QLatin1Char('\n'));
                    for (const QString& il : iwl) {
                        const QString t = il.trimmed();
                        if (t.startsWith(QLatin1String("tx bitrate:")))
                            bitrate = t.section(QLatin1Char(':'), 1).trimmed();
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
            wifiRows.append({ifName, ssid, bssid, channel, signal, bitrate});
            props.append(p);
            if (ssid != QLatin1String("-")) ssids.append(ssid);
        }
        freeifaddrs(ifa);
    }
#endif

    // ── v0.0.3 复刻：终端对齐表（rawOutput）+ 无接口降级文本 ──
    out.append(QString());
    out.append(QStringLiteral("Wireless LAN information:"));
    out.append(QString());
    static const QVector<DiagnosticFormatter::ColSpec> kWifiCols = {
        {"Interface", 12, false},
        {"SSID",      20, false},
        {"BSSID",     17, false},
        {"Channel",    8, true},   // numeric
        {"Signal",     7, true},   // numeric (dBm / %)
        {"Bitrate",    0, true},   // numeric (Mbps)
    };
    out.append(DiagnosticFormatter::formatTable(kWifiCols, wifiRows));
    if (wifiRows.isEmpty()) out.append(QStringLiteral("  (no wireless interfaces detected)"));

#if defined(PLATFORM_IOS)
    // v0.0.3：iOS 补 IP/网关段（en* 首个 AF_INET；热点桥接口跳过）
    {
        QString wifiIp, wifiGw, wifiIface;
        struct ifaddrs* ifa2 = nullptr;
        if (getifaddrs(&ifa2) == 0) {
            for (auto* q2 = ifa2; q2; q2 = q2->ifa_next) {
                if (!q2->ifa_addr || q2->ifa_addr->sa_family != AF_INET) continue;
                QString name = QString::fromLatin1(q2->ifa_name);
                if (!name.startsWith(QLatin1String("en"))) continue;
                if (name.contains(QLatin1String("bridge"), Qt::CaseInsensitive)) continue;
                wifiIface = name;
                char buf[INET_ADDRSTRLEN] = {0};
                auto* sin = (struct sockaddr_in*)q2->ifa_addr;
                inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
                wifiIp = QString::fromLatin1(buf);
                break;
            }
            freeifaddrs(ifa2);
        }
        if (!wifiIface.isEmpty())
            wifiGw = iosGatewayForInterface(wifiIface);
        out.append(QString());
        out.append(QStringLiteral("  IP Address: %1").arg(wifiIp.isEmpty() ? QStringLiteral("(not connected)") : wifiIp));
        if (!wifiGw.isEmpty())
            out.append(QStringLiteral("  Gateway: %1").arg(wifiGw));
        out.append(QStringLiteral("  Channel/Signal/Bitrate: unavailable on iOS (no public API)"));
        if (cachedIosSsid.isEmpty()) {
            out.append(QString());
            if (!wifiDiagnosticMsg.isEmpty()) {
                out.append(QStringLiteral("  %1").arg(wifiDiagnosticMsg));
            } else {
                out.append(QStringLiteral("  Note: SSID/BSSID need the \"Access WiFi Information\" entitlement,"));
                out.append(QStringLiteral("        Location permission, and an active WiFi connection."));
            }
        }
    }
#endif

    if (props.isEmpty()) {
        // 5WHY (复核 2026-08-21 无输出根因): 曾 Skipped——瓦片被隐藏，
        // 详情页零输出。v0.0.3 语义：无无线接口仍以 Info 呈现表格空态文本。
        DiagnosticResult r = makeResult(id, DiagStatus::Info,
            QStringLiteral("No WiFi interface present"), {}, out.join(QLatin1Char('\n')));
        r.narrative = QStringLiteral("No wireless interface was detected on this device.");
        return r;
    }
    DiagnosticResult r = makeResult(id, DiagStatus::Pass,
        ssids.isEmpty() ? QStringLiteral("WiFi interface present")
                        : QStringLiteral("WiFi: %1").arg(ssids.join(QStringLiteral(", "))),
        props, out.join(QLatin1Char('\n')));
    r.data[QStringLiteral("ssids")] = ssids;
    r.narrative = ssids.isEmpty()
        ? QStringLiteral("A wireless interface is present but not associated with any network (SSID: -). "
            "Check that Wi-Fi is enabled and connected to a network.")
        : QStringLiteral("Wi-Fi is connected to: %1. SSID/BSSID/channel/signal are listed in the table and per interface below.")
            .arg(ssids.join(QStringLiteral(", ")));
    return r;
}

// ── G1WiredDiagnostics ────────────────────────────────────────────────────
static DiagnosticResult probeWired(DiagId id, const QString&, RunContext& ctx) {
#if defined(PLATFORM_IOS)
    // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): v0.0.3 iOS 有线探针为
    // Skipped 恒文（"Wired Information:" 头——非 table mode——+ 说明行）。
    // 曾走通用名匹配把 en0（WiFi）误归有线。恢复 v0.0.3 恒文与 Skipped。
    const QStringList iosOut = {
        QString(),
        QStringLiteral("Wired Information:"),
        QString(),
        QStringLiteral("  [iOS] No wired Ethernet interface — not applicable on iOS devices."),
    };
    return makeResult(id, DiagStatus::Skipped,
        QStringLiteral("Not applicable on iOS (no wired NIC)"), {}, iosOut.join(QLatin1Char('\n')));
#else
    QVector<ResultProperty> props;
    const auto ifaces = runningInterfaces();
    for (const auto& i : ifaces) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
#if defined(__APPLE__)
        // 5WHY (复核 2026-08-21 误排除回归): 曾排除 en*/pdp_ip——但 Apple
        // 命名下内置/Thunderbolt/USB 有线网卡同为 en*（en0 常是 WiFi，
        // 有线适配器亦挂 en1/en2 等），整族排除使真实有线网卡恒报
        // Skipped "No wired interface present"（硬假阴性）。仅排除
        // pdp_ip（蜂窝）/utun（隧道）；en* 保留（Apple 命名固有的
        // WiFi/有线歧义无法以名区分，宁列勿失）。
        if (i.name().startsWith(QLatin1String("pdp_ip"))
            || i.name().startsWith(QLatin1String("utun")))
            continue;
#endif
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
                // v0.0.3 rd("duplex") 原样（含字面 "unknown"；文件缺失才 "-"）
                const QString d = QString::fromLatin1(df.readAll().trimmed());
                if (!d.isEmpty())
                    p.children.append({QStringLiteral("duplex"), d});
            }
            // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): v0.0.3 Link 列 =
            // /sys/class/net/<if>/carrier 原值（"0"/"1"）——补子行。
            QFile cf(QStringLiteral("/sys/class/net/%1/carrier").arg(i.name()));
            if (cf.open(QIODevice::ReadOnly))
                p.children.append({QStringLiteral("carrier"), QString::fromLatin1(cf.readAll().trimmed())});
            QFile of(QStringLiteral("/sys/class/net/%1/operstate").arg(i.name()));
            if (of.open(QIODevice::ReadOnly))
                p.children.append({QStringLiteral("state"), QString::fromLatin1(of.readAll().trimmed())});
            // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): v0.0.3 有线表含
            // MAC Address 列——补 MAC 子行（表格重建所需）。
            if (!i.hardwareAddress().isEmpty())
                p.children.append({QStringLiteral("MAC"), i.hardwareAddress()});
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
#endif   // PLATFORM_IOS 分支（iOS 恒文在上方提前返回）
}

// ── G1DhcpStatus ──────────────────────────────────────────────────────────
static DiagnosticResult probeDhcp(DiagId id, const QString&, RunContext& ctx) {
    QVector<ResultProperty> props;
    QStringList leases;
    int likelyCount = 0;   // 路由表网关推断的 "Likely" 行数（非确认租约）
    // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): Apple 分支恒文（v0.0.3 桩表
    // + 说明行）——曾把 macOS 服务器地址当客户端 IP 入通用表。桩表逐字节
    // 对齐 v0.0.3 G1DhcpStatus.cpp。
    QStringList legacyDetails;

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
                    if (!ipStr.isEmpty() && ipStr != QLatin1String("0.0.0.0"))
                        leases.append(QStringLiteral("%1=%2").arg(ifName, ipStr));
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
    // v0.0.3 iOS 恒文：桩表行 + 两行说明
    {
        static const QVector<DiagnosticFormatter::ColSpec> kDhcpColsLegacy = {
            {"Interface", 18, false}, {"DHCP", 6, false},
            {"IP Address", 18, false}, {"Server", 0, false},
        };
        QList<QStringList> stubRows;
        stubRows.append({QStringLiteral("(system-managed)"), QStringLiteral("Yes"),
            QStringLiteral("(not exposed)"), QStringLiteral("(not exposed)")});
        legacyDetails = { QString(), QStringLiteral("DHCP Client Status"), QString() };
        legacyDetails.append(DiagnosticFormatter::formatTable(kDhcpColsLegacy, stubRows));
        legacyDetails.append(QString());
        legacyDetails.append(QStringLiteral("  iOS manages DHCP at the system level —"));
        legacyDetails.append(QStringLiteral("  lease details are not accessible to third-party apps."));
    }
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
        leases.append(i.name() + QLatin1Char('=') + server);
    }
    // v0.0.3 macOS 恒文：桩表行 + 三行说明
    {
        static const QVector<DiagnosticFormatter::ColSpec> kDhcpColsLegacy = {
            {"Interface", 18, false}, {"DHCP", 6, false},
            {"IP Address", 18, false}, {"Server", 0, false},
        };
        QList<QStringList> stubRows;
        stubRows.append({QStringLiteral("(system-managed)"), QStringLiteral("Yes"),
            QStringLiteral("(use ifconfig)"), QStringLiteral("(not exposed)")});
        legacyDetails = { QString(), QStringLiteral("DHCP Client Status"), QString() };
        legacyDetails.append(DiagnosticFormatter::formatTable(kDhcpColsLegacy, stubRows));
        legacyDetails.append(QString());
        legacyDetails.append(QStringLiteral("  macOS manages DHCP via the SystemConfiguration framework —"));
        legacyDetails.append(QStringLiteral("  lease details are not directly accessible. Use `ipconfig getpacket <iface>`"));
        legacyDetails.append(QStringLiteral("  in Terminal for per-interface DHCP lease information."));
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
        // 5WHY (复核 2026-08-20 越界): ".leases" 7 字符，曾 chop(8) 多吃
        // 一个字符——dhclient.wlan0.leases → mid(9)="wlan0.leases" →
        // chop(8)="wla"（接口名截断错标，租约卡标题 "wla"）。chop(7)。
        if (fileName.endsWith(QLatin1String(".leases"))) fileName.chop(7);
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
        QString ipStr, serverStr, expire, renew;
        bool hasExtras = false, hasRenew = false, hasExpire = false;
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
            else if (line.contains(QLatin1String("dhcp-server-identifier"))) {
                serverStr = line.section(QLatin1Char(' '), -1).remove(QLatin1Char(';'));
                hasExtras = true;
            }
            // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): v0.0.3 附注行
            // "Lease Renew"/"Lease Expires" 出自 dhclient 租约 renew/expire
            // 行——renew 曾未捕获（附注层无法重建，v0.0.3 行静默丢失）。
            // 5WHY (复核 2026-08-21 空值行保留): v0.0.3 行存在即印附注
            // （"expire never;" 无限租约打印空值行）——曾仅非空值才落子
            // 属性，空值附注行静默丢失。
            else if (line.startsWith(QLatin1String("renew "))) {
                renew = line.section(QLatin1Char(' '), 2, 3).remove(QLatin1Char(';'));
                hasExtras = true;
                hasRenew = true;
            }
            else if (line.startsWith(QLatin1String("expire "))) {
                expire = line.section(QLatin1Char(' '), 2, 3).remove(QLatin1Char(';'));
                hasExtras = true;
                hasExpire = true;
            }
        }
        if (ipStr.isEmpty()) return;
        ResultProperty p(ifName, ipStr);
        p.children.append({QStringLiteral("DHCP"), QStringLiteral("Yes")});
        if (!serverStr.isEmpty()) p.children.append({QStringLiteral("server"), serverStr});
        // source 标记：dhclient 租约专属（附注层按此行集重建 v0.0.3 附注，
        // 与 systemd/nmcli 行区分——后两者 v0.0.3 无附注）。
        if (hasExtras) p.children.append({QStringLiteral("source"), QStringLiteral("dhclient")});
        if (hasRenew) p.children.append({QStringLiteral("renew"), renew});
        if (hasExpire) p.children.append({QStringLiteral("expire"), expire});
        props.append(p);
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
    if (leases.isEmpty() && !ctx.cancelled.load()) {
        // nmcliDeviceShow：每轮快照共享（与 probeIpConfig 同一次输出）
        const QString text = nmcliDeviceShow(ctx);
        if (!text.isEmpty()) {
            QString dev, devType, ipAddr, gateway, server, leaseTime, optIp, subnetMask, hostName;
            qulonglong leaseExpiry = 0;
            // 5WHY (复核 2026-08-20 状态重置收敛): 8 个捕获可变字符串曾以
            // 相同 clear 序列手工复位三处（两个早退 + 尾部）——加第 9 个
            // 字段即漏一处、下一设备属性卡串入陈旧值。改为进入即全清
            // （先取局部副本后无条件 clear），任何早退路径都不可能漏。
            auto flushDevice = [&]() {
                // 5WHY (复核 2026-08-20 状态重置收敛): 8 个捕获可变字符串曾以
                // 相同 clear 序列手工复位三处（两个早退 + 尾部）——加第 9 个
                // 字段即漏一处、下一设备属性卡串入陈旧值。改为进入即全清，
                // 任何早退路径都不可能漏。注意：清空先于使用——所有字段
                // 必须先拷贝到局部副本，函数体只读副本（首版曾在清空后仍
                // 读外部变量，harness 实测 server/lease time 子行整体消失）。
                const QString key = dev;
                const QString type = devType;
                const QString vIpAddr = ipAddr;
                const QString vGateway = gateway;
                const QString vServer = server;
                const QString vLeaseTime = leaseTime;
                const QString vOptIp = optIp;
                const QString vSubnetMask = subnetMask;
                const QString vHostName = hostName;
                const qulonglong expiry = leaseExpiry;
                const bool dhcpManaged = !vServer.isEmpty() || !vLeaseTime.isEmpty()
                    || !vOptIp.isEmpty() || expiry > 0;
                // IP4.ADDRESS 带 CIDR 后缀（"192.168.20.150/24"）——租约卡
                // 值列与 leases 列表只用地址本体（CIDR 属 IP 配置卡语义）。
                QString effIp = (vIpAddr.isEmpty() ? vOptIp : vIpAddr).section(QLatin1Char('/'), 0, 0);
                dev.clear(); devType.clear();
                ipAddr.clear(); gateway.clear(); server.clear(); leaseTime.clear();
                optIp.clear(); subnetMask.clear(); hostName.clear();
                leaseExpiry = 0;
                if (key.isEmpty()) return;
                // 回环/隧道设备无 DHCP 语义——过滤，避免 lo/tailscale
                // 以 "DHCP: No" 行混入租约卡（数据噪声）。
                if (type == QLatin1String("loopback") || type == QLatin1String("tun")
                    || type == QLatin1String("wireguard"))
                    return;
                if (!dhcpManaged && effIp.isEmpty())
                    return;   // 无 DHCP 且无地址的设备：无信息可呈现
                ResultProperty p(key, effIp.isEmpty() ? QStringLiteral("(DHCP)") : effIp);
                p.children.append({QStringLiteral("DHCP"), dhcpManaged ? QStringLiteral("Yes") : QStringLiteral("No")});
                if (!vServer.isEmpty()) p.children.append({QStringLiteral("server"), vServer});
                if (!vLeaseTime.isEmpty()) {
                    bool ltOk = false;
                    const qulonglong lt = vLeaseTime.toULongLong(&ltOk);
                    // NM 的无限租约 = 4294967295（UINT32_MAX）
                    p.children.append({QStringLiteral("lease time"),
                        (ltOk && lt >= 4294967295ULL) ? QStringLiteral("infinite") : vLeaseTime + QStringLiteral(" s")});
                }
                if (!vGateway.isEmpty()) p.children.append({QStringLiteral("gateway"), vGateway});
                if (!vSubnetMask.isEmpty()) p.children.append({QStringLiteral("subnet mask"), vSubnetMask});
                if (!vHostName.isEmpty()) p.children.append({QStringLiteral("host name"), vHostName});
                if (expiry > 0)
                    p.children.append({QStringLiteral("lease expiry"),
                        QDateTime::fromSecsSinceEpoch(static_cast<qint64>(expiry))
                            .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))});
                props.append(p);
                if (dhcpManaged && !effIp.isEmpty())
                    leases.append(key + QLatin1Char('=') + effIp);
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
                    // 5WHY (复核 2026-08-20 expiry-only 租约): 新版 NM 的
                    // DHCP4.OPTION 可能只给 expiry = <epoch>（无
                    // dhcp_lease_time/ip_address）——dhcpManaged 曾因此
                    // 判 false、真实 DHCP 接口被结论为"静态 IP"。expiry
                    // 也计入 DHCP 管理证据，并以本地时间呈现。
                    // 5WHY (复核 2026-08-20 哨兵一致): UINT32_MAX
                    // （4294967295）在 lease_time 列呈现 "infinite"，曾
                    // 在 expiry 列却换算成 2106 年具体日期——同一哨兵两种
                    // 呈现自相矛盾。哨兵不落 expiry 行（与 infinite 一致）。
                    else if (name == QLatin1String("expiry")) {
                        const qulonglong ts = value.toULongLong();
                        if (ts < 4294967295ULL) leaseExpiry = ts;
                    }
                }
            }
            flushDevice();
        }
    }
    if (leases.isEmpty() && !ctx.cancelled.load()) {
        // v0.0.3 回退语义恢复：租约文件与 NM 均不可得时，用路由表默认
        // 网关推断 DHCP（"Likely"——诚实标记不确定而非断言）。
        // 5WHY (复核 2026-08-20 矛盾行): 曾不过滤 dest==0（静态路由的网关
        // 也产出 "Likely" 行）且不与 nmcli 已呈现接口去重——NM 管理静态
        // IP 时同卡同时出现 "DHCP: No" 与 "DHCP: Likely" 互相矛盾。仅默认
        // 路由（routeGateways(true)）+ 跳过已呈现接口。
        const QHash<QString, QStringList> gws = routeGateways(true);
        QSet<QString> rendered;
        for (const auto& p : props) rendered.insert(p.label);
        for (auto it = gws.constBegin(); it != gws.constEnd() && !ctx.cancelled.load(); ++it) {
            if (rendered.contains(it.key()) || it.value().isEmpty()) continue;
            ResultProperty p(it.key(), QStringLiteral("DHCP likely"));
            p.children.append({QStringLiteral("DHCP"), QStringLiteral("Likely")});
            p.children.append({QStringLiteral("gateway"), it.value().join(QStringLiteral(", "))});
            props.append(p);
            ++likelyCount;
        }
    }
#endif
#endif
#endif
#endif
    // 5WHY (复核 2026-08-20 取消语义): 租约解析循环内取消仅早停（部分数据
    // 落盘）——曾直接落 Pass。与 ActiveConnections 同规则：解析后
    // 统一复查，取消整项计 Cancelled。
    if (ctx.cancelled.load())
        return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));

    if (!leases.isEmpty()) {
        DiagnosticResult r = makeResult(id, DiagStatus::Pass,
            QStringLiteral("%1 DHCP lease(s): %2").arg(leases.size()).arg(leases.join(QStringLiteral(", "))),
            props, legacyDetails.isEmpty() ? QString() : legacyDetails.join(QLatin1Char('\n')));
        r.data[QStringLiteral("leaseCount")] = leases.size();
        r.data[QStringLiteral("leases")] = leases;
        r.narrative = QStringLiteral("Found %1 active DHCP lease(s). "
            "DHCP server and lease timestamps are listed per interface below, "
            "sourced from lease files or NetworkManager (D-Bus).").arg(leases.size());
        return r;
    }
    // 5WHY (复核 2026-08-20 Likely 落穿): 回退行（"DHCP: Likely"）曾只
    // append props 不计入任何计数——指标卡 leaseCount=0 与属性行自相
    // 矛盾，且叙述声称数据"reported by NetworkManager"（实际来自
    // /proc/net/route）、结论"最可能静态 IP"与 Likely 行相抵。
    // 5WHY (复核 2026-08-20 计数锁步): 曾手维护 leaseCount 与 leases
    // 逐处同增——漏一处即指标卡与属性行再次分叉。删计数器，以
    // leases.size() 为单一事实；指标卡取页面可见证据行数
    // （确认租约 + Likely），likelyCount 另立数据键供未来消费。
    DiagnosticResult r = makeResult(id, DiagStatus::Info,
        likelyCount > 0
            ? QStringLiteral("DHCP not confirmed — %1 gateway-derived 'likely' interface(s)").arg(likelyCount)
            : (props.isEmpty() ? QStringLiteral("No DHCP information found")
                               : QStringLiteral("No DHCP lease found (static IP or managed externally)")),
        props, legacyDetails.isEmpty() ? QString() : legacyDetails.join(QLatin1Char('\n')));
    // 5WHY (复核 2026-08-21 指标诚实): 曾 leaseCount = leases.size() +
    // likelyCount——"Leases" 指标卡（metricLeases 标签）把未确认的
    // gateway-derived "Likely" 行计入租约数（0 张确认租约却显示 1），
    // 与叙述 "DHCP not confirmed" 自相矛盾。指标卡只报确认租约；
    // Likely 证据行由属性卡 + likelyCount 数据键承载。
    r.data[QStringLiteral("leaseCount")] = leases.size();
    r.data[QStringLiteral("likelyCount")] = likelyCount;
    r.narrative = likelyCount > 0
        ? QStringLiteral("No confirmed DHCP lease was found (lease files and NetworkManager data are "
            "unavailable), but the routing table shows a default gateway on %1 interface(s) — listed "
            "below as 'DHCP: Likely' (gateway-derived evidence, not a confirmed lease).").arg(likelyCount)
        : (props.isEmpty()
            ? QStringLiteral("No DHCP leases or DHCP-derived configuration were found on this device. "
                "The interface may use a static IP, or the DHCP client does not expose lease data "
                "(lease files are unreadable and NetworkManager is not installed).")
            : QStringLiteral("No active DHCP lease was found, but configuration reported by NetworkManager "
                "is listed below — the interface most likely uses a static IP configuration."));
    return r;
}

// ── G1IpConfiguration ─────────────────────────────────────────────────────
static DiagnosticResult probeIpConfig(DiagId id, const QString&, RunContext& ctx) {
    QVector<ResultProperty> props;
    // L7/Linux：默认网关（/proc/net/route）与 DNS 补入属性。
    // 5WHY (复核 2026-08-20 逐接口归属): 网关曾是无归属的全局列表、每个
    // 接口组重复同一串（lo/tailscale 也带 "gateway: 192.168.20.1"）——
    // 按路由表 iface 列映射到所属接口，网关只挂在其真实出口上。
#if defined(__linux__) || defined(__ANDROID__)
    // routeGateways(true)：默认路由网关按接口归属（与 probeDhcp 回退同源）
    const QHash<QString, QStringList> gatewaysByIface = routeGateways(true);
    QStringList dnsServers;
#if defined(__linux__) && !defined(PLATFORM_ANDROID)
    // 5WHY (2026-08-20 systemd-resolved stub): resolv.conf 常只有
    // 127.0.0.53（stub 解析器）——真实上游 DNS 经 NetworkManager D-Bus
    // 暴露（IP4.DNS）。nmcli 可用时取真实上游；不可用回退 resolv.conf。
    // nmcliDeviceShow：每轮快照共享（与 probeDhcp 同一次输出）
    const QString nmText = nmcliDeviceShow(ctx);
    for (const QString& line : nmText.split(QLatin1Char('\n'))) {
        const QString t = line.trimmed();
        if (t.startsWith(QLatin1String("IP4.DNS"))) {
            const QString v = t.section(QLatin1Char(':'), 1).trimmed();
            if (!v.isEmpty() && !dnsServers.contains(v)) dnsServers.append(v);
        }
    }
#endif
    // 5WHY (复核 2026-08-20 数据源收窄): resolv.conf 曾以 isEmpty() 门控
    // ——nmcli 一有输出即整体跳过，VPN/Tailscale/静态配置等非 NM 源注入
    // 的 nameserver 静默丢失。改为合并（去重）。
    // 5WHY (复核 2026-08-20 Android 回归): 该块曾被扫进 !PLATFORM_ANDROID
    // 守卫——Android（netd 填充 resolv.conf）DNS 行整体消失。移回外层
    // 守卫（nmcli 仍正确保持 Linux 专属）。
    // 5WHY (复核 2026-08-20 顺序依赖): stub 过滤曾是逐行顺序判定——
    // resolv.conf 把 stub 列在真实上游之前时 stub 照样入列（过滤失效）。
    // 改为两遍：先收全，再按"是否存在真实上游"统一过滤 127.x stub
    // （systemd-resolved 转发器噪声）；全 stub 时保留（唯一可得数据）。
    static const QRegularExpression kNsSep(QStringLiteral("\\s+"));
    QStringList rcServers;
    for (const QString& raw : SystemDiagnostics::readProcLines(QStringLiteral("/etc/resolv.conf"))) {
        const QString t = raw.trimmed();
        if (!t.startsWith(QLatin1String("nameserver"))) continue;
        const QString v = t.section(kNsSep, 1);
        if (!v.isEmpty() && !rcServers.contains(v)) rcServers.append(v);
    }
    bool hasReal = !dnsServers.isEmpty();   // nmcli 已给真实上游也算
    for (const QString& v : rcServers)
        if (!v.startsWith(QLatin1String("127."))) { hasReal = true; break; }
    for (const QString& v : rcServers) {
        if (hasReal && v.startsWith(QLatin1String("127."))) continue;
        if (!dnsServers.contains(v)) dnsServers.append(v);
    }
#endif   // __linux__ || __ANDROID__（网关 + DNS 数据源段）
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
        const auto& entries = i.addressEntries();   // 引用绑定临时，避免整表拷贝
        if (entries.isEmpty()) continue;   // 无地址接口（空 down 口）不入卡
        const QString firstIp = entries.first().ip().toString();
        ResultProperty p(i.name(), firstIp);
        // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): 复刻层需逐适配器块
        // （"Wireless LAN adapter wlan0:" 等）——type/DHCP 曾缺。
        // type 复用无线判据；DHCP 按 v0.0.3 语义（租约文件存在即 Yes）。
        {
            QString type = i.flags().testFlag(QNetworkInterface::IsLoopBack)
                ? QStringLiteral("loopback") : QStringLiteral("ethernet");
#if defined(__linux__)
            if (isWirelessInterface(i.name())) type = QStringLiteral("wireless");
            // 5WHY (复核 2026-08-21 租约索引): systemd-networkd 租约文件名是
            // 接口索引（如 "3"）而非接口名（probeDhcp 同名 5WHY 有载）——
            // 曾只查名字路径，networkd 管理的接口恒误报 "DHCP Enabled: No"。
            // 名字与索引两路并查；dhclient 名字路径不变。
            const bool dhcpEnabled =
                QFile::exists(QStringLiteral("/run/systemd/netif/leases/%1").arg(i.name()))
                || QFile::exists(QStringLiteral("/run/systemd/netif/leases/%1").arg(i.index()))
                || QFile::exists(QStringLiteral("/var/lib/dhcp/dhclient.%1.leases").arg(i.name()));
            p.children.append({QStringLiteral("DHCP"), dhcpEnabled ? QStringLiteral("Yes") : QStringLiteral("No")});
#endif
            p.children.append({QStringLiteral("type"), type});
        }
        if (!i.hardwareAddress().isEmpty()
            && i.hardwareAddress() != QLatin1String("00:00:00:00:00:00"))
            p.children.append({QStringLiteral("MAC"), i.hardwareAddress()});
#if defined(__linux__)
        // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): v0.0.3 ipconfig 逐适配器
        // 块含 Link Speed（sysfs 裸 Mbps，"%1 Mbps"）/MTU 行——补子行。
        p.children.append({QStringLiteral("MTU"), QString::number(i.maximumTransmissionUnit())});
        QFile spdFile(QStringLiteral("/sys/class/net/%1/speed").arg(i.name()));
        if (spdFile.open(QIODevice::ReadOnly)) {
            const QString s = QString::fromLatin1(spdFile.readAll().trimmed());
            if (!s.isEmpty() && s != QLatin1String("-1"))
                p.children.append({QStringLiteral("link speed"), s});
        }
#endif
        for (const auto& e : entries) {
            const bool isV6 = e.ip().protocol() == QAbstractSocket::IPv6Protocol;
            QString value = e.ip().toString();
            // 5WHY (复核 2026-08-20 Qt 内建): CIDR 后缀曾手写掩码换算且仅
            // 支持 IPv4——IPv6 地址丢失 /64 等后缀。QNetworkAddressEntry::
            // prefixLength()（Qt 5.11+）v4/v6 通吃；null/0 掩码返回 ≤0
            // 时不加后缀（与旧行为一致，不伪造 /0 路由语义）。
            const int prefix = e.prefixLength();
            if (prefix > 0) value += QStringLiteral("/%1").arg(prefix);
            p.children.append({isV6 ? QStringLiteral("IPv6") : QStringLiteral("IPv4"), value});
            ++addressCount;
        }
#if defined(__linux__) || defined(__ANDROID__)
        // 网关只挂在其真实出口接口组上（gatewaysByIface 归属映射）
        const QStringList ifaceGws = gatewaysByIface.value(i.name());
        if (!ifaceGws.isEmpty())
            p.children.append({QStringLiteral("gateway"), ifaceGws.join(QStringLiteral(", "))});
#endif
        props.append(p);
        ++interfaceCount;
    }
#if defined(__linux__) || defined(__ANDROID__)
    // 5WHY (复核 2026-08-20 孤儿网关): 路由表默认路由的出口接口若无任何
    // 地址条目（PPPoE/DSL 的 ppp0、指向 down 口的路由），上面的
    // entries.isEmpty() 提前 continue 使其网关行永不渲染——数据静默
    // 消失。补一行接口+网关（无地址信息时网关即全部可得数据）。
    {
        QSet<QString> rendered;
        for (const auto& p : props) rendered.insert(p.label);
        for (auto it = gatewaysByIface.constBegin(); it != gatewaysByIface.constEnd(); ++it) {
            if (rendered.contains(it.key()) || it.value().isEmpty()) continue;
            // 5WHY (复核 2026-08-21 主值重复): 曾 value 与子行 gateway 同串——
            // 分组卡标题右端与子行重复呈现同一网关。value 留空（无地址信息
            // 即无主值），网关仅由子行承载。
            // 5WHY (复核 2026-08-21 悬空值行): 曾主值为空——无条件终端转储
            // 下 propsDumpText 渲染 "ppp0: " 悬空行、分组卡标题空值格。
            // 主值 = 网关串（该接口的全部可得数据），子行标签语义不变。
            ResultProperty gp(it.key(), it.value().join(QStringLiteral(", ")));
            // 5WHY (复核 2026-08-21 幻影适配器块): 复刻层按 type 子行判定
            // 适配器块——孤儿行无 type 曾被标成 "Ethernet adapter ppp0:"
            // 且伪造 DHCP/Autoconfiguration 行。标 "unknown"（v0.0.3 的
            // "Unknown adapter %1:" 语义），复刻层如实呈现。
            gp.children.append({QStringLiteral("type"), QStringLiteral("unknown")});
            gp.children.append({QStringLiteral("gateway"), it.value().join(QStringLiteral(", "))});
            props.append(gp);
            ++interfaceCount;
        }
    }
#endif
    // 统一复查（同 probeDhcp/ActiveConnections 规则：解析后取消整项计 Cancelled）
    if (ctx.cancelled.load())
        return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
    // 5WHY (复核 2026-08-19 v0.0.3 对等): 主机名曾随 ipconfig 转储呈现
    // （G1IpConfiguration.cpp: Host Name）——现丢失，前置一条补回。
    // 5WHY (复核 2026-08-19 探针线程安全): QHostInfo::localHostName() 在
    // 工作线程上阻塞反查 DNS（Qt 文档明示可阻塞）——改用纯 libc
    // gethostname()（即时、无线程风险）。
    // （IPv6 已随 addressEntries 全族覆盖；DHCP Enabled/DNS 后缀无便携
    // 探测，记录为已知缺口。）
    // 5WHY (复核 2026-08-20 分支可达性): 主机名/DNS 前置曾位于空检查
    // 之前——空枚举时 props 因这些行恒非空，Info "No IP configuration
    // found" 分支不可达（空栈误报 Pass）。先判空，再前置平铺行。
    if (props.isEmpty())
        return makeResult(id, DiagStatus::Info, QStringLiteral("No IP configuration found"), {}, {});
    char hostBuf[256] = {};
    gethostname(hostBuf, sizeof(hostBuf) - 1);
    // 5WHY (复核 2026-08-20 空行守卫): gethostname 失败（受限命名空间/
    // Android）时缓冲为空——曾仍前置 "Host Name: " 空值行。非空才前置。
    if (hostBuf[0])
        props.prepend({QStringLiteral("Host Name"), QString::fromLocal8Bit(hostBuf)});
    // DNS 是主机级配置（非接口级）——独立平铺行，不重复挂每个接口组。
#if defined(__linux__) || defined(__ANDROID__)
    if (!dnsServers.isEmpty())
        props.prepend({QStringLiteral("DNS Servers"), dnsServers.join(QStringLiteral(", "))});
#endif
    DiagnosticResult r = makeResult(id, DiagStatus::Pass,
        QStringLiteral("IP configuration: %1 interface(s), %2 address(es)")
            .arg(interfaceCount).arg(addressCount), props, {});
    r.data[QStringLiteral("addressCount")] = addressCount;
#if defined(__linux__) && !defined(PLATFORM_ANDROID)
    // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): "IP Routing Enabled" 行数据
    // 曾缺——读 /proc/sys/net/ipv4/ip_forward（v0.0.3 同源）。
    // 5WHY (复核 2026-08-21 Android 恒文偏差): v0.0.3 守卫为
    // !__APPLE__ && !PLATFORM_ANDROID——Android 恒印 "Unknown"；曾把
    // Android 并入读取分支，可读文件时印 Yes/No（逐字节对齐落空）。
    // 守卫与 v0.0.3 同宽：Android 数据键缺席，复刻层落 "Unknown"。
    QFile fwdFile(QStringLiteral("/proc/sys/net/ipv4/ip_forward"));
    if (fwdFile.open(QIODevice::ReadOnly))
        r.data[QStringLiteral("routingEnabled")] =
            QString::fromLatin1(fwdFile.readAll().trimmed()) == QLatin1String("1");
#endif
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
    int udpCount = 0;   // v0.0.3 表含 UDP/UDP6 行（State "*:*"）
    QVector<ResultProperty> props;
    QStringList legacyOut;   // iOS v0.0.3 恒文（通用尾部分支以外）
#if defined(__linux__) || defined(__ANDROID__)
    // 5WHY (复核 2026-08-19 v0.0.3 对等): v0.0.3 以 Proto/Local/Foreign/State
    // 表格呈现全部连接——曾只抓 ESTABLISHED 的本地端点 hex 串。恢复全状态
    // + 可读端点（hex→点分十进制）+ 状态名 + tcp6。
    static const char* kStateNames[12] = {"", "ESTABLISHED", "SYN_SENT",
        "SYN_RECV", "FIN_WAIT1", "FIN_WAIT2", "TIME_WAIT", "CLOSE",
        "CLOSE_WAIT", "LAST_ACK", "LISTEN", "CLOSING"};
    const auto parseSockets = [&](const QString& path, const char* proto, bool isUdp) {
        // 5WHY (复核 2026-08-21 procfs 收敛): /proc size 0 的 atEnd 陷阱
        // ——共享 SystemDiagnostics::readProcLines（readLineInto 驱动），
        // 与 G2/G5 同源（曾为本文件唯一遗留的手写 /proc 读取）。
        for (const QString& line : SystemDiagnostics::readProcLines(path, 1)) {
            if (ctx.cancelled.load()) return;
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
            // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): v0.0.3 UDP 行 State
            // 恒 "*:*"（UDP 无状态机）。
            const QString stateName = isUdp
                ? QStringLiteral("*:*")
                : ((ok && state >= 1 && state <= 11)
                    ? QString::fromLatin1(kStateNames[state]) : QStringLiteral("UNKNOWN"));
            // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): v0.0.3 连接表列
            // Proto/Local/Remote/State——曾单串 value 内嵌全部字段，
            // 复刻层无法反解。补结构化子属性（属性卡呈现不变）。
            // 5WHY (复核 2026-08-21 双解消除): 曾同一端点 decodeEp 两遍
            // （value 一遍 + 子属性一遍）——局部一次，两处复用。
            const QString lEp = decodeEp(fields[1]);
            const QString rEp = decodeEp(fields[2]);
            ResultProperty cp(QStringLiteral("connection"),
                QStringLiteral("%1 %2 -> %3 [%4]")
                    .arg(QLatin1String(proto), lEp, rEp, stateName));
            cp.children.append({QStringLiteral("proto"), QLatin1String(proto)});
            cp.children.append({QStringLiteral("local"), lEp});
            cp.children.append({QStringLiteral("remote"), rEp});
            cp.children.append({QStringLiteral("state"), stateName});
            props.append(cp);
            if (isUdp) ++udpCount;   // UDP 行入表但不计入 TCP 计数/established
            else { ++count; if (state == 0x01) ++established; }
        }
    };
    // v0.0.3: TCP/TCP6/UDP/UDP6 全表（Proto 大写同 v0.0.3）
    parseSockets(QStringLiteral("/proc/net/tcp"), "TCP", false);
    parseSockets(QStringLiteral("/proc/net/tcp6"), "TCP6", false);
    parseSockets(QStringLiteral("/proc/net/udp"), "UDP", true);
    parseSockets(QStringLiteral("/proc/net/udp6"), "UDP6", true);
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
                // 结构化子属性（v0.0.3 复刻层表格列，见 Linux 分支同款 5WHY）
                ResultProperty cp(QStringLiteral("connection"),
                    QStringLiteral("%1:%2 -> %3:%4 (pid %5)")
                        .arg(QString::fromLatin1(inet_ntoa(la)))
                        .arg(ntohs((u_short)row.dwLocalPort))
                        .arg(QString::fromLatin1(inet_ntoa(ra)))
                        .arg(ntohs((u_short)row.dwRemotePort))
                        .arg(row.dwOwningPid));
                cp.children.append({QStringLiteral("proto"), QStringLiteral("TCP")});
                cp.children.append({QStringLiteral("local"),
                    QStringLiteral("%1:%2").arg(QString::fromLatin1(inet_ntoa(la)))
                        .arg(ntohs((u_short)row.dwLocalPort))});
                cp.children.append({QStringLiteral("remote"),
                    QStringLiteral("%1:%2").arg(QString::fromLatin1(inet_ntoa(ra)))
                        .arg(ntohs((u_short)row.dwRemotePort))});
                cp.children.append({QStringLiteral("state"),
                    QString::fromLatin1(SystemDiagnostics::tcpStateName(row.dwState))});
                props.append(cp);
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
            // 5WHY (复核 2026-08-21 列索引错误): 真实 netstat -an -p tcp 行为
            // "tcp4  0  0  local  remote  ESTABLISHED"——f[1]/f[2] 是
            // Recv-Q/Send-Q（恒 "0"），曾把队列长度当端点（"0 -> 0"）。
            // 端点取 f[3]/f[4]；macOS 端点为点号形 "ip.port"——归一化
            // 末段 '.' 为 ':'（复刻层按 ':' 拆 ip:port）。
            if (f.size() >= 6) {
                const auto normalizeEp = [](const QString& ep) {
                    const int dot = ep.lastIndexOf(QLatin1Char('.'));
                    if (dot < 0) return ep;
                    return ep.left(dot) + QLatin1Char(':') + ep.mid(dot + 1);
                };
                const QString lEp = normalizeEp(f[3]);
                const QString rEp = normalizeEp(f[4]);
                // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): 曾无子属性——
                // 复刻层表格四列全空。补 proto/local/remote/state 子行。
                ResultProperty cp(QStringLiteral("connection"),
                    QStringLiteral("%1 -> %2").arg(lEp, rEp));
                cp.children.append({QStringLiteral("proto"), QStringLiteral("TCP")});
                cp.children.append({QStringLiteral("local"), lEp});
                cp.children.append({QStringLiteral("remote"), rEp});
                cp.children.append({QStringLiteral("state"), QStringLiteral("ESTABLISHED")});
                props.append(cp);
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
    // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): v0.0.3 iOS 恒文两行。
    legacyOut.append(QString());
    legacyOut.append(QStringLiteral("Active Connections (netstat -an style)"));
    legacyOut.append(QString());
    legacyOut.append(QStringLiteral("  [iOS] Active connections: unavailable (restricted by Apple)"));
    legacyOut.append(QStringLiteral("  iOS sandbox prevents reading /proc/net/tcp — use Xcode network monitor"));
#endif
#endif
#endif
#if !defined(__linux__) && !defined(__ANDROID__)
    established = count;   // Windows/macOS 仅枚举 ESTABLISHED（Linux 在解析内累计）
#endif
    DiagnosticResult r = makeResult(id, count > 0 ? DiagStatus::Pass : DiagStatus::Info,
        QStringLiteral("%1 TCP connection(s), %2 established").arg(count).arg(established), props,
        legacyOut.isEmpty() ? QString() : legacyOut.join(QLatin1Char('\n')));
    r.data[QStringLiteral("tcpCount")] = count;
    r.data[QStringLiteral("udpCount")] = udpCount;
    r.data[QStringLiteral("establishedCount")] = established;
    r.narrative = QStringLiteral("%1 TCP connection(s) enumerated, of which %2 established. "
        "Local/remote endpoints are listed in the property card below.").arg(count).arg(established);
    return r;
}

// ── G1CellularInfo ────────────────────────────────────────────────────────
static DiagnosticResult probeCellular(DiagId id, const QString&, RunContext& ctx) {
    QVector<ResultProperty> props;
    QStringList out;
    // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): 头行曾缺冒号——v0.0.3 为
    // "Cellular Information:"（iOS 恒文同款头）。
    out.append(QStringLiteral("Cellular Information:"));
    out.append(QString());

#if defined(PLATFORM_IOS)
    // 5WHY (复核 2026-08-21 用户 "Cellular 与历史差距大"): v0.0.3 的 iOS
    // 深探测分支（Carrier/Radio Access/IP/Gateway/Signal/SIM 卡槽）在重构
    // 中丢失——iOS 上只剩接口名枚举，无任何蜂窝详情。逐字复刻 v0.0.3
    // cellularInfo iOS 分支（iosCellularInfo/pdp_ip0 网关）。
    {
        QVariantMap cell = iosCellularInfo();
        const bool hasCellIdentity = SystemDiagnostics::hasCellularIdentity(cell);
        // 5WHY (复核 2026-08-21 用户 "Cellular 无法找到 IP"): 曾硬编码
        // pdp_ip0——接口改名时 IP 与网关双双落空。iosCellularIPv4 候选名
        // + 排除法扫描，并回传真实接口名供网关查询。
        QString cellIface;
        const QString cellIp = iosCellularIPv4(&cellIface);
        const QString cellGw = cellIface.isEmpty()
            ? QString() : iosGatewayForInterface(cellIface);
        const QVariantList sims = cell.value(QStringLiteral("sims")).toList();
        const bool multiSim = sims.size() > 1;
        DiagStatus status = DiagStatus::Info;
        QString summary;
        if (hasCellIdentity || !sims.isEmpty()) {
            if (multiSim)
                out.append(QStringLiteral("  %1 SIM / eSIM lines active:").arg(sims.size()));
            for (const QVariant& v : sims) {
                const QVariantMap sim = v.toMap();
                const QString pad = multiSim ? QStringLiteral("    ") : QStringLiteral("  ");
                if (multiSim)
                    out.append(QStringLiteral("  SIM %1:").arg(sim.value(QStringLiteral("slot")).toInt()));
                const QString carrier = sim.value(QStringLiteral("carrierName")).toString();
                out.append(QStringLiteral("%1Carrier: %2").arg(pad,
                    carrier.isEmpty() ? QStringLiteral("(hidden by iOS 16+)") : carrier));
                const QString radio = sim.value(QStringLiteral("radioAccess")).toString();
                out.append(QStringLiteral("%1Radio Access: %2").arg(pad,
                    radio.isEmpty() ? QStringLiteral("(not available)") : radio));
                ResultProperty sp(QStringLiteral("SIM %1").arg(sim.value(QStringLiteral("slot")).toInt()),
                    carrier.isEmpty() ? QStringLiteral("(hidden by iOS 16+)") : carrier);
                if (!radio.isEmpty()) sp.children.append({QStringLiteral("radio access"), radio});
                props.append(sp);
            }
            out.append(QStringLiteral("  %1: %2")
                .arg(multiSim ? QStringLiteral("Data IP (active line)") : QStringLiteral("IP Address"),
                     cellIp.isEmpty() ? QStringLiteral("(not assigned)") : cellIp));
            if (!cellGw.isEmpty())
                out.append(QStringLiteral("  Gateway: %1").arg(cellGw));
            if (SystemDiagnostics::hasNonEmptyValue(cell, "signalNotice"))
                out.append(QStringLiteral("  Signal: %1").arg(cell["signalNotice"].toString()));
            out.append(QString());
            status = DiagStatus::Pass;
            if (multiSim) {
                QStringList rats;
                for (const QVariant& v : sims) {
                    const QString ra = v.toMap().value(QStringLiteral("radioAccess")).toString();
                    rats.append(ra.isEmpty() ? QStringLiteral("\u2014") : ra);
                }
                summary = QStringLiteral("%1 SIMs (%2)").arg(sims.size()).arg(rats.join(QStringLiteral(", ")));
            } else {
                summary = SystemDiagnostics::cellularSummary(cell);
            }
        } else {
            out.append(QStringLiteral("  No cellular service available"));
            if (!cellIp.isEmpty())
                out.append(QStringLiteral("  IP Address: %1").arg(cellIp));
            if (!cellGw.isEmpty())
                out.append(QStringLiteral("  Gateway: %1").arg(cellGw));
            if (SystemDiagnostics::hasNonEmptyValue(cell, "signalNotice"))
                out.append(QStringLiteral("  Signal: %1").arg(cell["signalNotice"].toString()));
            summary = QStringLiteral("No cellular service");
        }
        out.append(QString());
        DiagnosticResult r = makeResult(id, status, summary, props, out.join(QLatin1Char('\n')));
        r.narrative = (status == DiagStatus::Pass)
            ? QStringLiteral("Cellular service is active: %1.").arg(summary)
            : QStringLiteral("No cellular service available on this device.");
        return r;
    }
#endif

    // 5WHY (复核 2026-08-20 计数锁步): 曾 found bool 与 foundNames 逐处同
    // 置——漏一处即摘要与 Skipped 分支矛盾（同 DHCP leaseCount 类缺陷）。
    // found ≡ !foundNames.isEmpty()，删 bool 以列表为单一事实。
    QStringList foundNames;
    // 5WHY (复核 2026-08-21 跨平台编译破坏): mmcliListed/mmcliFailed 曾声明
    // 在 Linux 专属守卫内、却在守卫外的公共尾部（foundNames.isEmpty() 分支）
    // 读取——Windows/macOS/iOS/Android 上 undeclared identifier 硬编译失败。
    // 声明提升到函数作用域（非 Linux 平台恒 0，尾部分支语义不变）。
    int mmcliListed = 0;   // mmcli -L 枚举到的 modem 数（失败可见性，见下）
    int mmcliFailed = 0;   // 详情查询超时/格式解析失败的 modem 数
    bool mmcliListFailed = false;   // mmcli -L 本身超时/失败（区别于"无 modem"）

#if defined(__linux__) && !defined(PLATFORM_ANDROID)
    // 5WHY (2026-08-20 用户诉求 "Cellular 信息不完全"): v0.0.3 仅在 iOS
    // 输出 Raw 数据（Carrier/Radio Access/IP/Gateway/Signal/SIM 卡槽），
    // 桌面移植版只剩接口名+MAC——Raw 数据丢失。桌面 Linux 的标准蜂窝
    // 管理栈是 ModemManager（mmcli，免 root 读运营商/制式/信号/IMEI），
    // 有则完整转储；无则诚实地只报接口枚举（下方）。
    const QString mmcli = QStandardPaths::findExecutable(QStringLiteral("mmcli"));
    if (!mmcli.isEmpty()) {
        const QString listOut = SystemDiagnostics::cachedRunTool(ctx, mmcli,
            QStringList() << QStringLiteral("-L"), 3000);
        // 5WHY (复核 2026-08-21 列表失败可见): mmcli -L 成功至少输出
        // "No modems were found"——空输出 ⇒ 命令超时/失败。曾把列表
        // 失败与"无 modem"混为 Skipped（与详情查询失败同源的失败伪装，
        // 本 commit 的 Warning 修复漏了列表层）。
        mmcliListFailed = listOut.isEmpty();
        static const QRegularExpression kModemRe(QStringLiteral("Modem/(\\d+)"));
        // 5WHY (复核 2026-08-20 串行等待): 曾逐 modem 串行
        // waitForFinished(3000)——M 个 modem 累计 M×3s 启动延迟（modem
        // 间互不依赖，天然可并行）。先全部并发启动，再统一等待收割
        // （任一等待点取消即杀净仍在运行的进程，R5-1 析构安全）。
        QStringList nums;
        auto it = kModemRe.globalMatch(listOut);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            nums.append(m.captured(1));
        }
        mmcliListed = nums.size();
        struct ModemOutput { QString num; QString text; bool ok = false; };
        QVector<ModemOutput> outs;
        outs.reserve(nums.size());
        // std::vector 而非 QVector：unique_ptr 为 move-only（QVector 通用
        // 追加走拷贝构造编译失败），std::vector 原生支持。
        std::vector<std::unique_ptr<QProcess>> procs;
        procs.reserve(nums.size());
        for (const QString& num : nums) {
            auto p = std::make_unique<QProcess>();
            p->start(mmcli, QStringList() << QStringLiteral("-m") << num);
            procs.push_back(std::move(p));
        }
        const auto killAll = [&procs]() {
            for (auto& p : procs) {
                if (p && p->state() != QProcess::NotRunning) {
                    p->kill();
                    p->waitForFinished(2000);   // R5-1：析构前必须已终止
                }
            }
        };
        for (std::size_t i = 0; i < procs.size(); ++i) {
            if (ctx.cancelled.load()) { killAll(); return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled")); }
            ModemOutput mo;
            mo.num = nums[i];
            if (procs[i]->waitForFinished(3000)) {
                mo.ok = true;
                mo.text = QString::fromLocal8Bit(procs[i]->readAllStandardOutput());
            } else {
                procs[i]->kill();
                procs[i]->waitForFinished(2000);   // R5-1
            }
            outs.append(std::move(mo));
        }
        for (const auto& mo : outs) {
            if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
            const QString num = mo.num;
            // 5WHY (复核 2026-08-20 失败可见): 详情查询超时曾静默 continue
            // ——有 modem 却因 mmcli 卡死/格式变化时结果报 Skipped "No
            // cellular modem present"（把失败伪装成无硬件）。计数失败，
            // 尾部以 Warning 区分"无 modem"与"有 modem 但详情不可得"。
            if (!mo.ok) { ++mmcliFailed; continue; }
            const QString text = mo.text;
            // 5WHY (复核 2026-08-20 mmcli 格式): 真实 mmcli -m N 每行形如
            // "  3GPP   | operator name: 'AT&T'"（左侧节名 + '|' 分隔）——
            // 曾以整行 indexOf(':') 前部作 key 精确比较，"3GPP | operator
            // name" ≠ "operator name" 恒不匹配，carrier/rat/state 全空、
            // 守卫跳过每个 modem，深探测整体静默失效（本机无 modem 无法
            // 实测，仅代码审查可查）。修正：先剥离 '|' 左侧节名再取键值，
            // 并去除值两侧单引号。单遍 split+trim（曾对同一文本 split
            // 两次），解析与 Raw 转储共用一份行列表。
            QString carrier, rat, signal, mccmnc, state, imei, device;
            QStringList cleanLines;
            for (const QString& rawLine : text.split(QLatin1Char('\n'))) {
                QString t = rawLine.trimmed();
                if (t.isEmpty()) continue;
                cleanLines.append(t);
                const int sep = t.indexOf(QLatin1Char('|'));
                if (sep >= 0) t = t.mid(sep + 1).trimmed();
                const int colon = t.indexOf(QLatin1Char(':'));
                if (colon < 0) continue;
                const QString key = t.left(colon).trimmed();
                QString value = t.mid(colon + 1).trimmed();
                if (value.size() >= 2 && value.startsWith(QLatin1Char('\''))
                    && value.endsWith(QLatin1Char('\'')))
                    value = value.mid(1, value.size() - 2);
                if (key == QLatin1String("operator name")) carrier = value;
                else if (key == QLatin1String("access tech")) rat = value;
                else if (key == QLatin1String("signal quality")) signal = value;
                else if (key == QLatin1String("operator code")) mccmnc = value;
                else if (key == QLatin1String("state")) state = value;
                else if (key == QLatin1String("imei")) imei = value;
                else if (key == QLatin1String("device")) device = value;
            }
            // Raw 数据转储先于守卫判定（历史版本样式）——解析失败/格式
            // 变化时终端仍保留原始数据（"尽可能展示 Raw 数据"契约），
            // 失败对用户可见而非静默消失。
            out.append(QStringLiteral("  Modem %1:").arg(num));
            for (const QString& t : cleanLines)
                out.append(QStringLiteral("    %1").arg(t));
            out.append(QString());
            if (carrier.isEmpty() && rat.isEmpty() && state.isEmpty()) {
                ++mmcliFailed;   // 输出格式变化：键全空
                continue;
            }
            ResultProperty p(QStringLiteral("modem %1").arg(num),
                state.isEmpty() ? QStringLiteral("present") : state);
            if (!carrier.isEmpty()) p.children.append({QStringLiteral("carrier"), carrier});
            if (!rat.isEmpty()) p.children.append({QStringLiteral("radio access"), rat});
            if (!signal.isEmpty()) p.children.append({QStringLiteral("signal"), signal});
            if (!mccmnc.isEmpty()) p.children.append({QStringLiteral("MCC/MNC"), mccmnc});
            if (!imei.isEmpty()) p.children.append({QStringLiteral("IMEI"), imei});
            if (!device.isEmpty()) p.children.append({QStringLiteral("device"), device});
            props.append(p);
            foundNames.append(carrier.isEmpty() ? QStringLiteral("modem %1").arg(num) : carrier);
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
            foundNames.append(i.name());
            out.append(QStringLiteral("  %1  MAC %2").arg(i.name(), i.hardwareAddress()));
        }
    }

    // 取消复查（同 probeDhcp 规则）：取消发生在 mmcli -L 等待期间时
    // cachedRunTool 返回空 → mmcliListFailed=true——先分离取消语义，
    // 否则取消被误报为 ModemManager 故障 Warning（5WHY 复核 2026-08-21）。
    if (ctx.cancelled.load())
        return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
    // 5WHY (复核 2026-08-21 失败伪装残留): 曾把 mmcliListFailed 藏在
    // foundNames.isEmpty() 分支内——wwan/cellular 命名接口已枚举到
    // （foundNames 非空）时 `mmcli -L` 失败被静默吞掉，结果报干净 Pass
    // （"Cellular modem detected: wwan0"），正是本 commit 声称消除的
    // "失败伪装成成功"。枚举失败语义独立于接口枚举：先于一切分支上报
    // Warning（接口属性卡/原始转储尽力保留）。
    if (mmcliListFailed) {
        DiagnosticResult wr = makeResult(id, DiagStatus::Warning,
            QStringLiteral("ModemManager present but modem enumeration failed — cellular status unknown"),
            props, out.join(QLatin1Char('\n')));
        wr.narrative = QStringLiteral("ModemManager is installed, but `mmcli -L` produced no output "
            "(timeout or daemon failure), so cellular status could not be determined.");
        return wr;
    }
    if (foundNames.isEmpty()) {
        // 5WHY (复核 2026-08-20 失败伪装): mmcli 已枚举到 modem 但详情
        // 全部失败（超时/输出格式变化）时，曾报 Skipped "No cellular
        // modem present"——把失败伪装成无硬件，用户被误导。区分两种
        // 语义：无 modem = Skipped；有 modem 但详情不可得 = Warning
        // （Raw 转储尽力保留，故障可见）。
        if (mmcliListed > 0) {
            DiagnosticResult wr = makeResult(id, DiagStatus::Warning,
                QStringLiteral("Cellular modem(s) present but detail query failed (%1/%2)")
                    .arg(mmcliFailed).arg(mmcliListed),
                {}, out.join(QLatin1Char('\n')));
            wr.narrative = QStringLiteral("ModemManager reported %1 modem(s), but the detail query "
                "failed for %2 of them (timeout or output format change). "
                "Raw output is preserved in the terminal section.").arg(mmcliListed).arg(mmcliFailed);
            return wr;
        }
        // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): v0.0.3 非 iOS 平台无
        // modem 时恒文 "  [Skipped] Cellular info requires iOS — not
        // applicable on this platform"——曾空 details 落 props 转储。
        const QStringList skipOut = {
            QString(),
            QStringLiteral("Cellular Information:"),
            QString(),
            QStringLiteral("  [Skipped] Cellular info requires iOS — not applicable on this platform"),
            QString(),
        };
        return makeResult(id, DiagStatus::Skipped, QStringLiteral("No cellular modem present"), {},
            skipOut.join(QLatin1Char('\n')));
    }

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
    const QString ssid = info.value(QStringLiteral("ssid")).toString();
    const QString bssid = info.value(QStringLiteral("bssid")).toString();
    add("SSID", ssid);
    add("BSSID", bssid);
    add("signal dBm", info.value(QStringLiteral("rssi")).toString());
    add("channel", info.value(QStringLiteral("channel")).toString());
    add("security", info.value(QStringLiteral("security")).toString());
    // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): 曾空 details 落复刻层——
    // 平铺 props 被误当逐接口行（表头列名当接口名）。重建 v0.0.3 iOS
    // 终端：表行（en0 + SSID/BSSID，Channel/Signal/Bitrate "-"）+
    // IP/Gateway + "unavailable" 行 + SSID 缺失时的说明块。
    QStringList vOut;
    vOut.append(QString());
    vOut.append(QStringLiteral("Wireless LAN information:"));
    vOut.append(QString());
    {
        static const QVector<DiagnosticFormatter::ColSpec> kWifiCols = {
            {"Interface", 12, false}, {"SSID", 20, false}, {"BSSID", 17, false},
            {"Channel", 8, true}, {"Signal", 7, true}, {"Bitrate", 0, true},
        };
        QList<QStringList> rows;
        rows.append({QStringLiteral("en0"),
            ssid.isEmpty() ? QStringLiteral("-") : ssid,
            bssid.isEmpty() ? QStringLiteral("-") : bssid,
            QStringLiteral("-"), QStringLiteral("-"), QStringLiteral("-")});
        vOut.append(DiagnosticFormatter::formatTable(kWifiCols, rows));
    }
    const QString wifiIp = iosInterfaceIPv4(QStringLiteral("en0"));
    const QString wifiGw = iosGatewayForInterface(QStringLiteral("en0"));
    vOut.append(QString());
    vOut.append(QStringLiteral("  IP Address: %1").arg(wifiIp.isEmpty() ? QStringLiteral("(not connected)") : wifiIp));
    if (!wifiGw.isEmpty())
        vOut.append(QStringLiteral("  Gateway: %1").arg(wifiGw));
    vOut.append(QStringLiteral("  Channel/Signal/Bitrate: unavailable on iOS (no public API)"));
    if (ssid.isEmpty()) {
        vOut.append(QString());
        const QString diag = info.value(QStringLiteral("wifiDiagnostics")).toString();
        if (!diag.isEmpty())
            vOut.append(QStringLiteral("  %1").arg(diag));
        else {
            vOut.append(QStringLiteral("  Note: SSID/BSSID need the \"Access WiFi Information\" entitlement,"));
            vOut.append(QStringLiteral("        Location permission, and an active WiFi connection."));
        }
    }
    DiagnosticResult r = g1::makeResult(id, DiagStatus::Pass,
        QStringLiteral("WiFi: %1").arg(ssid),
        props, vOut.join(QLatin1Char('\n')));
    r.narrative = QStringLiteral("Wi-Fi is connected to %1 (BSSID %2). "
        "Signal, channel and security are listed in the property cards below.")
        .arg(ssid, bssid);
    return r;
}

DiagnosticResult iosCellularProbe(DiagId id) {
    const QVariantMap info = iosCellularInfo();
    // 5WHY (复核 2026-08-19 v0.0.3 对等): v0.0.3 的 "No cellular service"
    // 是 Info 态（G1CellularInfo.cpp）——曾返回 skipped 丢失该呈现。
    // iOS 设备恒有调制解调器：空图即"无服务"而非"无硬件"。
    if (info.isEmpty()) {
        // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): 空服务恒文（v0.0.3 iOS
        // "  No cellular service available" + 可选 IP/Gateway/Signal 行 +
        // 尾部空行）——曾空 details 落复刻层误判。
        const QStringList vOut = {
            QString(),
            QStringLiteral("Cellular Information:"),
            QString(),
            QStringLiteral("  No cellular service available"),
            QString(),
        };
        DiagnosticResult nr = g1::makeResult(id, DiagStatus::Info,
            QStringLiteral("No cellular service"),
            {{QStringLiteral("cellular"), QStringLiteral("No cellular service available")}},
            vOut.join(QLatin1Char('\n')));
        // 5WHY (复核 2026-08-20 文案三份): 同一句叙述曾逐字复制于 iOS/
        // Android——措辞修正需逐文件改。共享 GHelpers 单一来源。
        nr.narrative = SystemDiagnostics::cellularNoServiceNarrative();
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
    // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): v0.0.3 iOS 蜂窝逐行恒文——
    // 多卡 "  N SIM / eSIM lines active:" + 逐卡 "  SIM n:"/缩进 Carrier/
    // Radio Access（空值 "(hidden by iOS 16+)"/"(not available)"）+
    // Data IP/Gateway/Signal + 尾部空行。
    QStringList vOut;
    vOut.append(QString());
    vOut.append(QStringLiteral("Cellular Information:"));
    vOut.append(QString());
    const bool multiSim = simSlotList.size() > 1;
    if (multiSim)
        vOut.append(QStringLiteral("  %1 SIM / eSIM lines active:").arg(simSlotList.size()));
    for (const QVariant& s : simSlotList) {
        const QVariantMap sim = s.toMap();
        const QString pad = multiSim ? QStringLiteral("    ") : QStringLiteral("  ");
        if (multiSim)
            vOut.append(QStringLiteral("  SIM %1:").arg(sim.value(QStringLiteral("slot")).toString()));
        const QString carrier = sim.value(QStringLiteral("carrier")).toString();
        vOut.append(QStringLiteral("%1Carrier: %2").arg(pad,
            carrier.isEmpty() ? QStringLiteral("(hidden by iOS 16+)") : carrier));
        const QString rat = sim.value(QStringLiteral("rat")).toString();
        vOut.append(QStringLiteral("%1Radio Access: %2").arg(pad,
            rat.isEmpty() ? QStringLiteral("(not available)") : rat));
    }
    const QString dataIp = info.value(QStringLiteral("dataIp")).toString();
    vOut.append(QStringLiteral("  %1: %2")
        .arg(multiSim ? QStringLiteral("Data IP (active line)") : QStringLiteral("IP Address"),
             dataIp.isEmpty() ? QStringLiteral("(not assigned)") : dataIp));
    const QString gw = info.value(QStringLiteral("gateway")).toString();
    if (!gw.isEmpty())
        vOut.append(QStringLiteral("  Gateway: %1").arg(gw));
    const QString sig = info.value(QStringLiteral("signalStrength")).toString();
    if (!sig.isEmpty())
        vOut.append(QStringLiteral("  Signal: %1").arg(sig));
    vOut.append(QString());
    // 5WHY (复核 2026-08-20 复用): "Carrier: X" 字符串曾各平台各写一份
    // ——共享 GHelpers::cellularSummary（carrier+radio 组合回退逻辑）。
    DiagnosticResult r = g1::makeResult(id, DiagStatus::Pass,
        SystemDiagnostics::cellularSummary(info), props, vOut.join(QLatin1Char('\n')));
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
