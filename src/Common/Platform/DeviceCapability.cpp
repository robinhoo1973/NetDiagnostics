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
    // 报 PASS。Qt 6 已提供权威类型（QNetworkInterface::type() 经 IOKit/
    // sysfs/GetIfTable2 派生）——先信类型，启发式仅作类型 Unknown 时的回退。
    const auto all = QNetworkInterface::allInterfaces();
    for (const auto& iface : all) {
        if (!iface.flags().testFlag(QNetworkInterface::IsRunning)) continue;
        if (iface.type() == QNetworkInterface::Wifi) return true;
    }
    for (const auto& iface : all) {
        if (!iface.flags().testFlag(QNetworkInterface::IsRunning)) continue;
        // 5WHY (2026-09-05 复核): 回退启发式不得覆盖已知类型——类型已判
        // 为 Ethernet/Loopback 的接口（如 macOS 有线 en1）不得经名称/MAC
        // 启发式误判为 WiFi（与 hasEthernetInterface 的同类守卫对称）。
        if (iface.type() == QNetworkInterface::Ethernet
            || iface.type() == QNetworkInterface::Loopback)
            continue;
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
    // 误判为有线（见 hasWifiInterface 同款 5WHY）。仅当类型 Unknown 时
    // 才退回名称启发式。
    const auto all = QNetworkInterface::allInterfaces();
    for (const auto& iface : all) {
        if (!iface.flags().testFlag(QNetworkInterface::IsRunning)) continue;
        if (iface.type() == QNetworkInterface::Ethernet) return true;
    }
    for (const auto& iface : all) {
        if (!iface.flags().testFlag(QNetworkInterface::IsRunning)) continue;
        // 5WHY (2026-09-05 复核): 回退启发式跳过已知非以太类型——类型已判
        // 为 Wifi 的接口（macOS 无线 en0）不得经 "en" 名称前缀误判为有线
        // （正是本修复要消除的颠倒；缺此守卫则颠倒依旧成立）。
        if (iface.type() == QNetworkInterface::Wifi
            || iface.type() == QNetworkInterface::Loopback)
            continue;
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
