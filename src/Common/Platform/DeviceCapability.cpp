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
    const auto all = QNetworkInterface::allInterfaces();
    for (const auto& iface : all) {
        if (iface.flags().testFlag(QNetworkInterface::IsRunning)) {
            const QString hw = iface.hardwareAddress().toLower();
            // Typical WiFi MACs: locally-administered unicast on popular chips
            // (02:…, 0e:…, etc.) — combined with name hints for Windows.
            if (hw.startsWith(QLatin1String("02:")) || hw.startsWith(QLatin1String("0e:"))
                || iface.name().contains(QLatin1String("wl"), Qt::CaseInsensitive)
                || iface.name().contains(QLatin1String("wlan"), Qt::CaseInsensitive)
                || iface.name().contains(QLatin1String("wi-fi"), Qt::CaseInsensitive))
                return true;
        }
    }
    return false;
}

bool hasEthernetInterface() {
    const auto all = QNetworkInterface::allInterfaces();
    for (const auto& iface : all) {
        if (!iface.flags().testFlag(QNetworkInterface::IsRunning)) continue;
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
