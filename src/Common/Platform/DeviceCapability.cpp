// =============================================================================
// DeviceCapability.cpp — Runtime hardware-presence probes
//
// Lightweight per-platform adapter scans.  These MUST stay fast (called on
// the main thread while building the Config list) and MUST NOT crash when a
// capability cannot be determined — unknown capabilities default to
// "supported" so a probe failure never hides a diagnostic.
// =============================================================================
#include "Common/Platform/DeviceCapability.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#endif

#if defined(__APPLE__)
#include <ifaddrs.h>
#include <net/if.h>
#endif

#if defined(PLATFORM_ANDROID)
#include <QJniObject>
#include "Common/Platform/Android/PlatformAndroidJni.h"
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
#include <QDir>
#include <QFile>
#include <QFileInfo>
#endif

namespace DeviceCapability {

namespace {
bool s_initialized = false;
bool s_wifi = false;
bool s_cellular = false;
bool s_wired = false;

#if defined(_WIN32)
// Presence of adapter types (hardware capability — do not require UP state,
// a disabled WiFi card still means the device can run a WiFi diagnostic).
static void probePlatform() {
    ULONG bufLen = 15000;
    QByteArray buf(bufLen, '\0');
    PIP_ADAPTER_ADDRESSES adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());
    ULONG flags = GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_ANYCAST;
    if (GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, adapters, &bufLen) != NO_ERROR) {
        buf.resize(int(bufLen));
        adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());
        if (GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, adapters, &bufLen) != NO_ERROR)
            return; // unknown → defaults stay false
    }
    for (auto* a = adapters; a; a = a->Next) {
        if (a->IfType == IF_TYPE_IEEE80211)           s_wifi = true;
        else if (a->IfType == IF_TYPE_ETHERNET_CSMACD) s_wired = true;
        else if (a->IfType == IF_TYPE_WWANPP
                 || a->IfType == IF_TYPE_WWANPP2)     s_cellular = true;
    }
}
#else
#if defined(PLATFORM_ANDROID)
// 5WHY: virtually every Android device ships WiFi, so WiFi presence is a
// constant true.  Cellular varies (WiFi-only tablets) and is checked via
// TelephonyManager.getPhoneType(); wired Ethernet (Android TV / USB dongle)
// is checked via the active network's TRANSPORT_ETHERNET capability.
static bool androidHasCellular() {
    QJniObject ctx = getQtActivity();
    if (!ctx.isValid()) return true; // unknown → assume present (safe default)
    QJniObject tm = ctx.callObjectMethod(
        "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;",
        QJniObject::fromString("phone").object<jstring>());
    if (!tm.isValid()) return false;
    const jint phoneType = tm.callMethod<jint>("getPhoneType", "()I");
    return phoneType != 0; // PHONE_TYPE_NONE = 0
}
static bool androidHasWiredEthernet() {
    QJniObject ctx = getQtActivity();
    if (!ctx.isValid()) return false;
    QJniObject cm = ctx.callObjectMethod(
        "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;",
        QJniObject::fromString("connectivity").object<jstring>());
    if (!cm.isValid()) return false;
    QJniObject net = cm.callObjectMethod("getActiveNetwork", "()Landroid/net/Network;");
    if (!net.isValid()) return false;
    QJniObject caps = cm.callObjectMethod(
        "getNetworkCapabilities", "(Landroid/net/Network;)Landroid/net/NetworkCapabilities;",
        net.object());
    if (!caps.isValid()) return false;
    // NetworkCapabilities.TRANSPORT_ETHERNET == 3
    return caps.callMethod<jboolean>("hasTransport", "(I)Z", 3) == JNI_TRUE;
}
static void probePlatform() {
    s_wifi = true;               // all Android devices have WiFi hardware
    s_cellular = androidHasCellular();
    s_wired = androidHasWiredEthernet();
}
#else
#if defined(__APPLE__)
// getifaddrs interface names: iOS/macOS use "en*" for Wi-Fi/Ethernet and
// "pdp_ip*" for cellular.  Macs always ship Wi-Fi; wired Ethernet and
// cellular cannot be told apart from the name alone, so they default to
// false (conservative: an Ethernet-only Mac won't show the wired test).
static void probePlatform() {
    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) != 0) {
        s_wifi = true; // safe default — Macs/iOS devices have Wi-Fi
        return;
    }
    for (auto* p = ifa; p; p = p->ifa_next) {
        if (!p->ifa_name) continue;
        const QString name = QString::fromLatin1(p->ifa_name);
        if (name.startsWith(QLatin1String("en"))) s_wifi = true;
        else if (name.startsWith(QLatin1String("pdp_ip"))) s_cellular = true;
    }
    freeifaddrs(ifa);
}
#else
// Linux: /sys/class/net scan.
//   WiFi   → /sys/class/net/<iface>/wireless exists
//   Wired  → ARPHRD_ETHER (type 1) and not wireless, not virtual
//   Cell   → interface names wwan* / rmnet* / ppp* / usb*
static void probePlatform() {
    const QDir netDir(QStringLiteral("/sys/class/net"));
    const QStringList ifaces = netDir.entryList(QDir::NoDotAndDotDot | QDir::Dirs);
    for (const auto& iface : ifaces) {
        const QString base = QStringLiteral("/sys/class/net/%1").arg(iface);
        const bool isWifi = QFile::exists(base + QStringLiteral("/wireless"));
        if (isWifi) { s_wifi = true; }

        QFile typeFile(base + QStringLiteral("/type"));
        if (typeFile.open(QIODevice::ReadOnly)) {
            const int type = typeFile.readAll().trimmed().toInt();
            if (type == 1 /* ARPHRD_ETHER */ && !isWifi) {
                // Exclude common virtual/bridge names so a Docker bridge or
                // VPN doesn't masquerade as a physical wired NIC.
                if (!iface.startsWith(QLatin1String("br-"))
                    && !iface.startsWith(QLatin1String("veth"))
                    && !iface.startsWith(QLatin1String("docker"))
                    && !iface.startsWith(QLatin1String("virbr"))
                    && !iface.startsWith(QLatin1String("tun"))
                    && !iface.startsWith(QLatin1String("tap")))
                    s_wired = true;
            }
            typeFile.close();
        }
        if (iface.startsWith(QLatin1String("wwan"))
            || iface.startsWith(QLatin1String("rmnet"))
            || iface.startsWith(QLatin1String("ppp")))
            s_cellular = true;
    }
}
#endif // __APPLE__
#endif // PLATFORM_ANDROID
#endif // _WIN32

void ensureProbed() {
    if (s_initialized) return;
    s_wifi = s_cellular = s_wired = false;
    probePlatform();
    s_initialized = true;
}

} // namespace

void invalidateCache() {
    s_initialized = false;
}

bool diagSupportedOnDevice(DiagId id) {
    ensureProbed();
    switch (id) {
        case DiagId::G1WifiDiagnostics:   return s_wifi;
        case DiagId::G1CellularInfo:      return s_cellular;
        case DiagId::G1WiredDiagnostics:  return s_wired;
        default:                          return true; // device-independent
    }
}

} // namespace DeviceCapability
