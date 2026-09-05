// =============================================================================
// DeviceCapability.cpp — Hardware-presence probes with caching (NEW-4)
// =============================================================================
#include "Common/Platform/DeviceCapability.h"

#include <QNetworkInterface>
#include <QMap>

// Portable hardware detection via QNetworkInterface.  Platform-specific deep
// probes (Windows SetupDi / Linux sysfs / macOS IOKit) are follow-up work per
// group; QNetworkInterface gives a correct true/false for interface presence
// on all platforms.
namespace {

bool hasWifiInterface() {
    // 5WHY (2026-09-05 macOS WiFi/有线颠倒): 曾仅凭 MAC 前缀（02:/0e:）与
    // 名称启发式（wl/wlan/wi-fi）判 WiFi——macOS 的 WiFi 就是 en0 且持有
    // 真实厂商 MAC（3c:22:fb:…），两个启发式都不命中 → WiFi 判 false；
    // 而 hasEthernetInterface 的 "en" 前缀命中 en0 → 有线判 true。结果：
    // MacBook 无线联网时 G1WiFi 被自动跳过、G1Wired 把 en0（WiFi）当有线
    // 报 PASS。Qt 6 已提供类型（QNetworkInterface::type()）——但权威性
    // 平台相关（5WHY 2026-09-05 复核 Linux 颠倒复现）：仅 macOS IOKit 的
    // 类型派生可靠；Linux sysfs 的 ARPHRD 类型把多数 WiFi 驱动（wlan0）报成
    // Ethernet（ARPHRD_ETHER=1，实测本机 wlan0 即如此），若按 Ethernet 类型
    // 守卫跳过启发式，WiFi 再次判 false 且被 hasEthernetInterface 判成有线
    // ——macOS 颠倒原样复现于 Linux/Android。类型 Wifi 是各平台可靠的肯定
    // 信号（先信），Ethernet 类型仅在 macOS 上作否定信号，其余平台回退
    // 名称/MAC 启发式。
    const auto all = QNetworkInterface::allInterfaces();
    for (const auto& iface : all) {
        if (!iface.flags().testFlag(QNetworkInterface::IsRunning)) continue;
        if (iface.type() == QNetworkInterface::Wifi) return true;
    }
    for (const auto& iface : all) {
        if (!iface.flags().testFlag(QNetworkInterface::IsRunning)) continue;
#if defined(Q_OS_MACOS)
        // macOS 类型权威：Ethernet/Loopback 已确证非 WiFi（如有线 en1），
        // 不得经名称/MAC 启发式误判。
        if (iface.type() == QNetworkInterface::Ethernet
            || iface.type() == QNetworkInterface::Loopback)
            continue;
#else
        // Linux/Windows/Android：Ethernet 类型不可信（wlan0 报 ARPHRD_ETHER），
        // 仅跳过 Loopback——名称/MAC 启发式是 WiFi 判定的唯一可靠信号。
        if (iface.type() == QNetworkInterface::Loopback)
            continue;
#endif
        const QString hw = iface.hardwareAddress().toLower();
        // Typical WiFi MACs: locally-administered unicast on popular chips
        // (02:…, 0e:…, etc.) — combined with name hints for Windows.
        if (hw.startsWith(QLatin1String("02:")) || hw.startsWith(QLatin1String("0e:"))
            || iface.name().contains(QLatin1String("wl"), Qt::CaseInsensitive)
            || iface.name().contains(QLatin1String("wlan"), Qt::CaseInsensitive)
            || iface.name().contains(QLatin1String("wi-fi"), Qt::CaseInsensitive))
            return true;
    }
    return false;
}

bool hasEthernetInterface() {
    // 5WHY (2026-09-05): 类型权威优先——"en" 名称前缀把 macOS 无线 en0
    // 误判为有线。5WHY (2026-09-05 复核 Linux 颠倒复现): Ethernet 类型的
    // 权威性同样平台相关——Linux 上 wlan0 报 ARPHRD_ETHER，类型即判有线会
    // 把 WiFi 适配器当有线 PASS；软件网桥（docker0/br-*/veth，同为
    // ARPHRD_ETHER）也会误报"有线存在"。仅 macOS 以类型直接判真；其余
    // 平台保留名称启发式（与修复前的行为一致：wlan0/docker0 均不匹配
    // eth/en 前缀，真实有线 eth0/enp3s0 命中）。
    const auto all = QNetworkInterface::allInterfaces();
#if defined(Q_OS_MACOS)
    for (const auto& iface : all) {
        if (!iface.flags().testFlag(QNetworkInterface::IsRunning)) continue;
        if (iface.type() == QNetworkInterface::Ethernet) return true;
    }
#endif
    for (const auto& iface : all) {
        if (!iface.flags().testFlag(QNetworkInterface::IsRunning)) continue;
#if defined(Q_OS_MACOS)
        // 回退启发式跳过已知非以太类型——类型已判为 Wifi 的接口（无线
        // en0）不得经 "en" 名称前缀误判为有线。
        if (iface.type() == QNetworkInterface::Wifi
            || iface.type() == QNetworkInterface::Loopback)
            continue;
#else
        // Linux/Windows/Android：仅跳过 Loopback；类型为 Ethernet 的接口
        // 仍须过名称启发式（wlan0 不匹配 eth/en，不判有线）。
        if (iface.type() == QNetworkInterface::Loopback)
            continue;
#endif
        const QString name = iface.name();
        if (name.contains(QLatin1String("eth"), Qt::CaseInsensitive)
            || name.contains(QLatin1String("en"), Qt::CaseInsensitive)
            || name.contains(QLatin1String("ethernet"), Qt::CaseInsensitive)
            || name.startsWith(QLatin1String("en0")) || name.startsWith(QLatin1String("en1")))
            return true;
    }
    return false;
}

bool hasCellularModem() {
    const auto all = QNetworkInterface::allInterfaces();
    for (const auto& iface : all) {
        if (!iface.flags().testFlag(QNetworkInterface::IsRunning)) continue;
        if (iface.name().contains(QLatin1String("wwan"), Qt::CaseInsensitive)
            || iface.name().contains(QLatin1String("cellular"), Qt::CaseInsensitive)
            || iface.name().contains(QLatin1String("rmnet"), Qt::CaseInsensitive)
            || iface.name().contains(QLatin1String("pdp"), Qt::CaseInsensitive))
            return true;
    }
    return false;
}

} // namespace

namespace {
// NEW-4/R1-2：单一缓存——diagSupportedOnDevice 与 invalidateCache 共享同一个 map。
// （此前 invalidateCache 清的是另一个从未使用的堆 map → 运行前刷新完全失效，且泄漏。）
QMap<DiagId, bool>& deviceProbeCache() {
    static QMap<DiagId, bool> s_cache;
    return s_cache;
}
} // namespace

bool DeviceCapability::diagSupportedOnDevice(DiagId id) {
    auto& cache = deviceProbeCache();
    auto it = cache.constFind(id);
    if (it != cache.constEnd()) return it.value();

    bool ok = true;
    switch (id) {
        case DiagId::G1WifiDiagnostics:  ok = hasWifiInterface();      break;
        case DiagId::G1WiredDiagnostics: ok = hasEthernetInterface();  break;
        case DiagId::G1CellularInfo:     ok = hasCellularModem();      break;
        default:                         ok = true;                    break;
    }
    cache.insert(id, ok);
    return ok;
}

void DeviceCapability::invalidateCache() {
    deviceProbeCache().clear();
}
