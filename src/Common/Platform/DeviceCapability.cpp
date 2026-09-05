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

// 5WHY (simplify 2026-09-05 平台矩阵双份): hasWifi/hasEthernet 曾各持一份
// 类型权威性矩阵（#if macOS + 各自跳过集合 + IsRunning 守卫），每能力查询
// 扫 allInterfaces 两遍。单遍分类：平台矩阵只写一次，WiFi/有线一次判定。
// 类型权威性平台相关（5WHY 2026-09-05 复核 Linux 颠倒复现）：仅 macOS
// IOKit 类型派生可靠；Linux sysfs ARPHRD 把 wlan0 报成 Ethernet（实测本机），
// 软件网桥（docker0/br-*）也报 Ethernet——Ethernet 类型仅 macOS 直判真，
// 其余平台名称/MAC 启发式是唯一可靠信号；类型 Wifi 是各平台可靠的肯定
// 信号（先信）。
void classifyInterfaces(bool& hasWifi, bool& hasEthernet) {
    const auto all = QNetworkInterface::allInterfaces();
    for (const auto& iface : all) {
        // 双方为真即短路（simplify 二轮 2026-09-05）：剩余接口不会再翻转结果
        if (hasWifi && hasEthernet) return;
        if (!iface.flags().testFlag(QNetworkInterface::IsRunning)) continue;
        if (iface.type() == QNetworkInterface::Wifi) {
            hasWifi = true;
#if defined(Q_OS_MACOS)
            continue;   // 无线 en0 不得经 "en" 名称启发式误判为有线
#endif
        }
#if defined(Q_OS_MACOS)
        if (iface.type() == QNetworkInterface::Ethernet) {
            hasEthernet = true;
            continue;   // 已确证有线，无需启发式
        }
#endif
        if (iface.type() == QNetworkInterface::Loopback) continue;
        const QString hw = iface.hardwareAddress().toLower();
        // WiFi 启发式：本地管理单播 MAC（02:/0e:，常见芯片）+ 名称提示
        if (hw.startsWith(QLatin1String("02:")) || hw.startsWith(QLatin1String("0e:"))
            || iface.name().contains(QLatin1String("wl"), Qt::CaseInsensitive)
            || iface.name().contains(QLatin1String("wlan"), Qt::CaseInsensitive)
            || iface.name().contains(QLatin1String("wi-fi"), Qt::CaseInsensitive)) {
            hasWifi = true;
            continue;
        }
        // 有线启发式：wlan0/docker0/br-* 均不匹配，真实有线 eth0/enp3s0 命中
        const QString name = iface.name();
        if (name.contains(QLatin1String("eth"), Qt::CaseInsensitive)
            || name.contains(QLatin1String("en"), Qt::CaseInsensitive)
            || name.contains(QLatin1String("ethernet"), Qt::CaseInsensitive)
            || name.startsWith(QLatin1String("en0")) || name.startsWith(QLatin1String("en1")))
            hasEthernet = true;
    }
}

bool hasWifiInterface() {
    bool hasWifi = false, hasEthernet = false;
    classifyInterfaces(hasWifi, hasEthernet);
    return hasWifi;
}

bool hasEthernetInterface() {
    bool hasWifi = false, hasEthernet = false;
    classifyInterfaces(hasWifi, hasEthernet);
    return hasEthernet;
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
