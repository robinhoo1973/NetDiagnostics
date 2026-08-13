// =============================================================================
// NetworkDiagnostics.cpp — Android platform diagnostics via JNI (G1-G5)
//
// Uses Qt's QJniObject to call Android Java APIs directly from C++.
// No separate Java/Kotlin source files needed — all JNI is inline.
//
// Design principle: on Android every G1-G5 test that CAN return real data
// has a dedicated implementation here.  Sources used:
//   · ConnectivityManager.getLinkProperties()  → DNS servers, routes,
//     interface addresses, MTU, default gateway (API 21+, no permission)
//   · ConnectivityManager.getDefaultProxy()    → system HTTP proxy
//   · NetworkCapabilities                      → transport, VPN, metered,
//     validated, private-DNS, link bandwidth
//   · WifiManager/WifiInfo                     → SSID/BSSID/RSSI/channel/speed
//   · TelephonyManager                         → carrier/MCC/MNC/network type
//   · PackageManager                           → security-agent detection
//   · getifaddrs + /sys/class/net              → interface list + MTU/state
//   · InetAddress                              → DNS resolution (A + AAAA)
//   · HttpURLConnection/HttpsURLConnection     → HTTP + TLS diagnostics
// =============================================================================
#if defined(PLATFORM_ANDROID)

#include <QJniObject>
#include <QJniEnvironment>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QDateTime>
#include <QElapsedTimer>
#include <QThread>
#include <QUrl>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "Common/Model/DiagnosticResult.h"
#include "Common/Model/DiagId.h"
#include "Diagnostics/View/DiagnosticFormatter.h"
#include "Common/Platform/Android/PlatformAndroidJni.h"

// 5WHY: Functions were wrapped in namespace G5WebsiteUrl but header
// NetworkDiagnostics.h declares them at global scope.  Removed namespace
// so definitions match declarations — fixes "undefined symbol" linker error.

static bool clearJniException(QJniEnvironment& env) {
    if (!env->ExceptionCheck())
        return false;
    env->ExceptionClear();
    return true;
}

static QString signalGlyphs(int level) {
    switch (qBound(0, level, 4)) {
        case 1: return QStringLiteral("▂");
        case 2: return QStringLiteral("▂▄");
        case 3: return QStringLiteral("▂▄▆");
        case 4: return QStringLiteral("▂▄▆█");
        default: return QStringLiteral("▁");
    }
}

// ── Runtime permission check ───────────────────────────────────────────
// 5WHY: iOS checks [CLLocationManager authorizationStatus] before accessing
// SSID/BSSID. Android code had no equivalent — just called the API and
// returned generic errors. Now mirrors iOS's actionable guidance.
// 5WHY (round 2): ACCESS_FINE_LOCATION/COARSE are now declared in the
// manifest AND requested at runtime by NetDiagApplication (both were missing
// — the check could never succeed).  Check FINE first; on devices where only
// COARSE is granted, BSSID may still be exposed, so accept either.
static QString androidLocationPermissionStatus() {
    // 5WHY (Android launch crash): use getQtActivity() (version-independent
    // QNativeInterface accessor) instead of hardcoding the Qt5-era Java class
    // "org/qtproject/qt/android/QtNative", which is not stable in Qt 6.
    QJniObject ctx = getQtActivity();
    if (!ctx.isValid()) return QString();

    // ContextCompat.checkSelfPermission(context, Manifest.permission.ACCESS_FINE_LOCATION)
    QJniObject permStr = QJniObject::fromString("android.permission.ACCESS_FINE_LOCATION");
    jint result = QJniObject::callStaticMethod<jint>(
        "androidx/core/content/ContextCompat", "checkSelfPermission",
        "(Landroid/content/Context;Ljava/lang/String;)I",
        ctx.object(), permStr.object<jstring>());

    const jint PERMISSION_GRANTED = 0;
    if (result == PERMISSION_GRANTED) return QString(); // OK

    QJniObject coarseStr = QJniObject::fromString("android.permission.ACCESS_COARSE_LOCATION");
    jint coarseResult = QJniObject::callStaticMethod<jint>(
        "androidx/core/content/ContextCompat", "checkSelfPermission",
        "(Landroid/content/Context;Ljava/lang/String;)I",
        ctx.object(), coarseStr.object<jstring>());
    if (coarseResult == PERMISSION_GRANTED) return QString(); // coarse suffices on some builds

    return QStringLiteral("WiFi SSID/BSSID: Location permission was denied. "
                          "Go to Settings > Apps > NetDiagnostics > Permissions "
                          "and enable 'Location' (required for WiFi diagnostics).");
}

// Whether the app holds the phone-state permission needed for carrier/MCC/MNC.
// API 31+ auto-grants READ_BASIC_PHONE_STATE which satisfies the same calls.
static bool androidHasPhonePermission() {
    QJniObject ctx = getQtActivity();
    if (!ctx.isValid()) return false;
    QJniObject permStr = QJniObject::fromString("android.permission.READ_PHONE_STATE");
    jint result = QJniObject::callStaticMethod<jint>(
        "androidx/core/content/ContextCompat", "checkSelfPermission",
        "(Landroid/content/Context;Ljava/lang/String;)I",
        ctx.object(), permStr.object<jstring>());
    return result == 0; // PERMISSION_GRANTED
}

// ── WiFi snapshot via WifiManager/WifiInfo ─────────────────────────────
// 5WHY: SSID/BSSID were fetched by two separate functions with error strings
// smuggled back as "SSID" values, then string-matched ("contains permission")
// by the caller — fragile and wrong.  Now one snapshot struct returns all
// WifiInfo fields (SSID/BSSID/RSSI/frequency/link-speed) plus a status.
struct AndroidWifiInfo {
    bool valid = false;           // a WifiInfo object was returned
    bool connected = false;       // valid AND an SSID is actually associated
    bool permissionDenied = false;
    QString ssid, bssid, error;
    int rssi = -127;              // dBm (WIFI_INFO_UNAVAILABLE = -127)
    int frequency = 0;            // MHz (0 = unknown)
    int linkSpeedMbps = 0;        // negotiated PHY rate
};
static AndroidWifiInfo androidWifiInfo() {
    AndroidWifiInfo w;
    QString permError = androidLocationPermissionStatus();
    if (!permError.isEmpty()) {
        w.permissionDenied = true;
        w.error = permError;
        return w;
    }
    QJniObject ctx = getQtActivity();
    if (!ctx.isValid()) { w.error = QStringLiteral("Activity unavailable"); return w; }
    QJniObject wifiService = ctx.callObjectMethod(
        "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;",
        QJniObject::fromString("wifi").object<jstring>());
    if (!wifiService.isValid()) { w.error = QStringLiteral("WifiManager unavailable"); return w; }
    QJniObject wifiInfo = wifiService.callObjectMethod(
        "getConnectionInfo", "()Landroid/net/wifi/WifiInfo;");
    if (!wifiInfo.isValid()) { w.error = QStringLiteral("No WiFi connection"); return w; }
    w.valid = true;

    QJniObject ssid = wifiInfo.callObjectMethod("getSSID", "()Ljava/lang/String;");
    if (ssid.isValid()) {
        QString s = ssid.toString();
        if (s.startsWith('"') && s.endsWith('"'))
            s = s.mid(1, s.length() - 2);
        w.ssid = s;
    }
    QJniObject bssid = wifiInfo.callObjectMethod("getBSSID", "()Ljava/lang/String;");
    if (bssid.isValid()) w.bssid = bssid.toString();

    w.rssi = wifiInfo.callMethod<jint>("getRssi", "()I");
    w.frequency = wifiInfo.callMethod<jint>("getFrequency", "()I");

    // 5WHY: WifiInfo.getConnectionInfo() returns an object even when not
    // associated — SSID="<unknown ssid>", BSSID="02:00:00:00:00:00",
    // RSSI=-127.  Without this check the diagnostic falsely reported a
    // connection (SSID "<unknown ssid>") when WiFi was simply off.
    const bool maskedSsid = w.ssid.isEmpty()
        || w.ssid == QStringLiteral("<unknown ssid>")
        || w.ssid == QStringLiteral("unknown ssid");
    const bool maskedBssid = w.bssid.isEmpty()
        || w.bssid == QStringLiteral("02:00:00:00:00:00");
    w.connected = w.valid && !maskedSsid && (!maskedBssid || w.frequency > 0);

    // Link speed: getWifiLinkSpeedMbps (API 31+) preferred; getLinkSpeed (deprecated).
    const jint sdkInt = QJniObject::getStaticField<jint>("android/os/Build$VERSION", "SDK_INT");
    QJniEnvironment env;
    if (sdkInt >= 31) {
        jint speed = wifiInfo.callMethod<jint>("getWifiLinkSpeedMbps", "()I");
        if (!clearJniException(env)) w.linkSpeedMbps = speed;
    } else {
        w.linkSpeedMbps = wifiInfo.callMethod<jint>("getLinkSpeed", "()I");
    }
    return w;
}

// ── WiFi helpers ────────────────────────────────────────────────────────
// AOSP WifiManager.calculateSignalLevel(rssi, 5): MIN=-100, MAX=-55.
static int wifiSignalLevel(int rssi) {
    const int kMinRssi = -100, kMaxRssi = -55;
    if (rssi <= kMinRssi) return 0;
    if (rssi >= kMaxRssi) return 4;
    return (rssi - kMinRssi) * 4 / (kMaxRssi - kMinRssi);
}
// Channel number from center frequency (2.4 GHz → 1-14, 5 GHz → 36-165).
static int wifiChannelFromFrequency(int freqMhz) {
    if (freqMhz <= 0) return 0;
    if (freqMhz >= 2412 && freqMhz <= 2484) return (freqMhz - 2412) / 5 + 1;
    if (freqMhz >= 5170 && freqMhz <= 5825) return (freqMhz - 5000) / 5;
    return 0;
}

// ── Cellular carrier / network via TelephonyManager ────────────────────
// Numeric NETWORK_TYPE_* → human name (getNetworkTypeName() is deprecated and
// under-detailed; getDataNetworkType() needs READ_PHONE_STATE on API 29+).
static QString androidNetworkTypeName(int netType) {
    switch (netType) {
        case 0:  return QStringLiteral("Unknown");
        case 1:  return QStringLiteral("GPRS");        // 2G
        case 2:  return QStringLiteral("EDGE");        // 2G
        case 3:  return QStringLiteral("UMTS");        // 3G
        case 4:  return QStringLiteral("CDMA");        // 2G
        case 5:  return QStringLiteral("EVDO rev.0");  // 3G
        case 6:  return QStringLiteral("EVDO rev.A");  // 3G
        case 7:  return QStringLiteral("1xRTT");       // 2G
        case 8:  return QStringLiteral("HSDPA");       // 3G
        case 9:  return QStringLiteral("HSUPA");       // 3G
        case 10: return QStringLiteral("HSPA");        // 3G
        case 11: return QStringLiteral("iDEN");        // 2G
        case 12: return QStringLiteral("EVDO rev.B");  // 3G
        case 13: return QStringLiteral("LTE");         // 4G
        case 14: return QStringLiteral("eHRPD");       // 3G
        case 15: return QStringLiteral("HSPA+");       // 3.5G
        case 16: return QStringLiteral("GSM");         // 2G
        case 17: return QStringLiteral("TD-SCDMA");    // 3G
        case 18: return QStringLiteral("IWLAN");       // WiFi calling
        case 19: return QStringLiteral("LTE CA");      // 4G+
        case 20: return QStringLiteral("NR");          // 5G
        default: return QStringLiteral("Type%1").arg(netType);
    }
}

static QString androidSimStateName(int st) {
    switch (st) {
        case 0: return QStringLiteral("Unknown");
        case 1: return QStringLiteral("Absent");
        case 2: return QStringLiteral("PIN required");
        case 3: return QStringLiteral("PUK required");
        case 4: return QStringLiteral("Network locked");
        case 5: return QStringLiteral("Ready");
        case 6: return QStringLiteral("Not ready");
        case 7: return QStringLiteral("Permanently disabled");
        case 8: return QStringLiteral("Loaded");
        default: return QStringLiteral("State%1").arg(st);
    }
}

static QString androidDataStateName(int st) {
    switch (st) {
        case 0: return QStringLiteral("Disconnected");
        case 1: return QStringLiteral("Connecting");
        case 2: return QStringLiteral("Connected");
        case 3: return QStringLiteral("Suspended");
        default: return QStringLiteral("State%1").arg(st);
    }
}

static QVariantMap androidCellularInfo() {
    QVariantMap info;

    QJniObject ctx = getQtActivity();
    if (!ctx.isValid()) return info;

    QJniObject telService = ctx.callObjectMethod(
        "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;",
        QJniObject::fromString("phone").object<jstring>());
    if (!telService.isValid()) return info;

    const jint sdkInt = QJniObject::getStaticField<jint>("android/os/Build$VERSION", "SDK_INT");

    // Carrier name (empty without READ_PHONE_STATE on API 29+)
    QJniObject carrierName = telService.callObjectMethod(
        "getNetworkOperatorName", "()Ljava/lang/String;");
    if (carrierName.isValid() && !carrierName.toString().isEmpty())
        info["carrierName"] = carrierName.toString();

    // Network type (4G/5G/etc.) — legacy name + precise numeric mapping
    QJniObject networkType = telService.callObjectMethod(
        "getNetworkTypeName", "()Ljava/lang/String;");
    if (networkType.isValid() && !networkType.toString().isEmpty())
        info["radioAccess"] = networkType.toString();
    QJniEnvironment netEnv;
    const jint dataType = telService.callMethod<jint>("getDataNetworkType", "()I");
    if (!clearJniException(netEnv) && dataType > 0)
        info["dataNetworkType"] = androidNetworkTypeName(dataType);

    // MCC/MNC
    QJniObject networkOperator = telService.callObjectMethod(
        "getNetworkOperator", "()Ljava/lang/String;");
    if (networkOperator.isValid()) {
        QString op = networkOperator.toString();
        if (op.length() >= 5) {
            info["mcc"] = op.left(3);
            info["mnc"] = op.mid(3);
        }
    }

    // Phone type, SIM state, data state, roaming (mostly permission-free)
    const jint phoneType = telService.callMethod<jint>("getPhoneType", "()I");
    if (phoneType >= 1 && phoneType <= 3) {
        static const char* kPhoneNames[] = {"NONE", "GSM", "CDMA", "SIP"};
        info["phoneType"] = QString::fromLatin1(kPhoneNames[phoneType]);
    }
    QJniEnvironment simEnv;
    const jint simState = telService.callMethod<jint>("getSimState", "()I");
    if (!clearJniException(simEnv))
        info["simState"] = androidSimStateName(simState);
    QJniEnvironment dataEnv;
    const jint dataState = telService.callMethod<jint>("getDataState", "()I");
    if (!clearJniException(dataEnv))
        info["dataState"] = androidDataStateName(dataState);
    QJniEnvironment roamEnv;
    const jboolean roaming = telService.callMethod<jboolean>("isNetworkRoaming", "()Z");
    if (!clearJniException(roamEnv))
        info["roaming"] = roaming;

    // Signal strength (API 28+; no permission required)
    if (sdkInt >= 28) {
        QJniObject signalStrength = telService.callObjectMethod(
            "getSignalStrength", "()Landroid/telephony/SignalStrength;");
        if (signalStrength.isValid()) {
            QJniEnvironment levelEnv;
            const jint signalLevel = signalStrength.callMethod<jint>("getLevel", "()I");
            if (!clearJniException(levelEnv) && signalLevel >= 0 && signalLevel <= 4)
                info["signalLevel"] = signalLevel;

            QJniEnvironment env;
            const jint rsrp = signalStrength.callMethod<jint>("getLteRsrp", "()I");
            if (!clearJniException(env) && rsrp < 0 && rsrp > -200)
                info["rsrp"] = QStringLiteral("%1dBm").arg(rsrp);
            else {
                const jint rssi = signalStrength.callMethod<jint>("getCdmaRssi", "()I");
                if (!clearJniException(env) && rssi >= -120 && rssi < 0)
                    info["cdmaRssi"] = QStringLiteral("%1dBm").arg(rssi);
            }
        } else {
            QJniEnvironment env;
            clearJniException(env);
        }
    }

    return info;
}

// ── Connectivity info via ConnectivityManager + NetworkCapabilities ────
// 5WHY: getActiveNetworkInfo() is deprecated (API 29) and returns null on
// some devices.  getActiveNetwork() + getNetworkCapabilities() is the modern
// API and additionally reports transport types (WiFi/Cellular/VPN/…) plus
// INTERNET/VALIDATED/PRIVATE_DNS capabilities.
// Forward declarations — the ConnectivityManager helpers are defined in the
// "ConnectivityManager / LinkProperties helpers" section below.
static QJniObject androidConnectivityManager();
static QJniObject androidActiveNetwork();

struct AndroidCaps {
    bool available = false;
    bool wifi = false, cellular = false, ethernet = false, vpn = false;
    bool bluetooth = false, wifiAware = false;
    bool internet = false, validated = false, notMetered = false;
    bool privateDns = false, notRestricted = false;
    int downKbps = 0, upKbps = 0;
};
static AndroidCaps androidCapabilities() {
    AndroidCaps c;
    QJniObject cm = androidConnectivityManager();
    QJniObject net = androidActiveNetwork();
    if (!cm.isValid() || !net.isValid()) return c;
    QJniObject caps = cm.callObjectMethod("getNetworkCapabilities",
        "(Landroid/net/Network;)Landroid/net/NetworkCapabilities;", net.object());
    if (!caps.isValid()) { QJniEnvironment env; clearJniException(env); return c; }
    c.available = true;
    c.wifi       = caps.callMethod<jboolean>("hasTransport", "(I)Z", 1);
    c.cellular   = caps.callMethod<jboolean>("hasTransport", "(I)Z", 0);
    c.ethernet   = caps.callMethod<jboolean>("hasTransport", "(I)Z", 3);
    c.vpn        = caps.callMethod<jboolean>("hasTransport", "(I)Z", 4);
    c.bluetooth  = caps.callMethod<jboolean>("hasTransport", "(I)Z", 2);
    c.wifiAware  = caps.callMethod<jboolean>("hasTransport", "(I)Z", 5);
    c.internet   = caps.callMethod<jboolean>("hasCapability", "(I)Z", 12);
    c.validated  = caps.callMethod<jboolean>("hasCapability", "(I)Z", 16);
    c.notMetered = caps.callMethod<jboolean>("hasCapability", "(I)Z", 11);
    c.notRestricted = caps.callMethod<jboolean>("hasCapability", "(I)Z", 13);
    c.privateDns = caps.callMethod<jboolean>("hasCapability", "(I)Z", 17);
    c.downKbps   = caps.callMethod<jint>("getLinkDownstreamBandwidthKbps", "()I");
    c.upKbps     = caps.callMethod<jint>("getLinkUpstreamBandwidthKbps", "()I");
    return c;
}

static QString androidConnectivityInfo() {
    AndroidCaps caps = androidCapabilities();
    if (!caps.available) return QStringLiteral("No active network");
    QStringList transports;
    if (caps.wifi)      transports << QStringLiteral("WiFi");
    if (caps.cellular)  transports << QStringLiteral("Cellular");
    if (caps.ethernet)  transports << QStringLiteral("Ethernet");
    if (caps.vpn)       transports << QStringLiteral("VPN");
    if (caps.bluetooth) transports << QStringLiteral("Bluetooth");
    if (caps.wifiAware) transports << QStringLiteral("WiFi Aware");
    QString result = transports.isEmpty() ? QStringLiteral("Unknown") : transports.join(QStringLiteral("+"));
    if (caps.internet) result += QStringLiteral(" (Internet)");
    return result;
}

// ── ConnectivityManager / LinkProperties helpers ───────────────────────
// 5WHY: Android 7+ blocks /proc/net/route, /proc/net/arp and /etc/resolv.conf
// for apps.  ConnectivityManager.getLinkProperties() is the ONLY public
// source for DNS servers, routes, interface addresses and MTU — no root, no
// extra permission.  All G2/G3 gateway/routing/DNS tests use it.

static QJniObject androidConnectivityManager() {
    QJniObject ctx = getQtActivity();
    if (!ctx.isValid()) return QJniObject();
    return ctx.callObjectMethod("getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;",
        QJniObject::fromString("connectivity").object<jstring>());
}

static QJniObject androidActiveNetwork() {
    QJniObject cm = androidConnectivityManager();
    if (!cm.isValid()) return QJniObject();
    return cm.callObjectMethod("getActiveNetwork", "()Landroid/net/Network;");
}

static QJniObject androidActiveLinkProperties() {
    QJniObject cm = androidConnectivityManager();
    QJniObject net = androidActiveNetwork();
    if (!cm.isValid() || !net.isValid()) return QJniObject();
    return cm.callObjectMethod("getLinkProperties",
        "(Landroid/net/Network;)Landroid/net/LinkProperties;", net.object());
}

// Iterate a java.util.List<java.net.InetAddress> → host address strings
static QStringList javaInetAddressList(QJniObject& list) {
    QStringList out;
    if (!list.isValid()) return out;
    jint size = list.callMethod<jint>("size", "()I");
    for (jint i = 0; i < size; ++i) {
        QJniObject item = list.callObjectMethod("get", "(I)Ljava/lang/Object;", i);
        if (!item.isValid()) continue;
        QJniObject ip = item.callObjectMethod("getHostAddress", "()Ljava/lang/String;");
        if (ip.isValid()) out.append(ip.toString());
    }
    return out;
}

// DNS servers of the active network (API 21+, no permission)
static QStringList androidDnsServers() {
    QJniObject lp = androidActiveLinkProperties();
    if (!lp.isValid()) return {};
    QJniObject list = lp.callObjectMethod("getDnsServers", "()Ljava/util/List;");
    return javaInetAddressList(list);
}

// Interface addresses (IPv4/IPv6 + prefix) of the active network
static QStringList androidLinkAddresses() {
    QJniObject lp = androidActiveLinkProperties();
    if (!lp.isValid()) return {};
    QJniObject list = lp.callObjectMethod("getLinkAddresses", "()Ljava/util/List;");
    QStringList out;
    if (!list.isValid()) return out;
    jint size = list.callMethod<jint>("size", "()I");
    for (jint i = 0; i < size; ++i) {
        QJniObject la = list.callObjectMethod("get", "(I)Ljava/lang/Object;", i);
        if (!la.isValid()) continue;
        QJniObject addr = la.callObjectMethod("getAddress", "()Ljava/net/InetAddress;");
        QJniObject addrStr = addr.callObjectMethod("getHostAddress", "()Ljava/lang/String;");
        int plen = la.callMethod<jint>("getPrefixLength", "()I");
        if (addrStr.isValid())
            out.append(QStringLiteral("%1/%2").arg(addrStr.toString()).arg(plen));
    }
    return out;
}

static int androidLinkMtu() {
    QJniObject lp = androidActiveLinkProperties();
    if (!lp.isValid()) return 0;
    return lp.callMethod<jint>("getMtu", "()I");
}

// Parsed LinkProperties.getRoutes() — the Android routing table
struct AndroidRoute {
    QString iface;
    QString dest;
    int prefixLen = 0;
    QString gateway;
    bool isDefault = false;
    bool hasGateway = false;
};
static QVector<AndroidRoute> androidRoutes() {
    QVector<AndroidRoute> routes;
    QJniObject lp = androidActiveLinkProperties();
    if (!lp.isValid()) return routes;
    QJniObject list = lp.callObjectMethod("getRoutes", "()Ljava/util/List;");
    if (!list.isValid()) return routes;
    jint size = list.callMethod<jint>("size", "()I");
    for (jint i = 0; i < size; ++i) {
        QJniObject ri = list.callObjectMethod("get", "(I)Ljava/lang/Object;", i);
        if (!ri.isValid()) continue;
        AndroidRoute r;
        QJniObject iface = ri.callObjectMethod("getInterface", "()Ljava/lang/String;");
        r.iface = iface.isValid() ? iface.toString() : QString();
        r.isDefault = ri.callMethod<jboolean>("isDefaultRoute", "()Z");
        r.hasGateway = ri.callMethod<jboolean>("hasGateway", "()Z");
        QJniObject dest = ri.callObjectMethod("getDestination", "()Landroid/net/LinkAddress;");
        if (dest.isValid()) {
            QJniObject addr = dest.callObjectMethod("getAddress", "()Ljava/net/InetAddress;");
            QJniObject addrStr = addr.callObjectMethod("getHostAddress", "()Ljava/lang/String;");
            if (addrStr.isValid()) r.dest = addrStr.toString();
            r.prefixLen = dest.callMethod<jint>("getPrefixLength", "()I");
        }
        if (r.hasGateway) {
            QJniObject gw = ri.callObjectMethod("getGateway", "()Ljava/net/InetAddress;");
            QJniObject gwStr = gw.callObjectMethod("getHostAddress", "()Ljava/lang/String;");
            if (gwStr.isValid()) r.gateway = gwStr.toString();
        }
        routes.append(r);
    }
    return routes;
}

// System HTTP proxy via ConnectivityManager.getDefaultProxy() (API 21+)
struct AndroidProxy {
    bool set = false;
    QString host;
    int port = 0;
    QStringList exclusions;
};
static AndroidProxy androidProxy() {
    AndroidProxy p;
    QJniObject cm = androidConnectivityManager();
    if (!cm.isValid()) return p;
    QJniObject pi = cm.callObjectMethod("getDefaultProxy", "()Landroid/net/ProxyInfo;");
    if (!pi.isValid()) return p;
    QJniObject host = pi.callObjectMethod("getHost", "()Ljava/lang/String;");
    if (host.isValid()) p.host = host.toString();
    p.port = pi.callMethod<jint>("getPort", "()I");
    QJniObject excl = pi.callObjectMethod("getExclusionList", "()[Ljava/lang/String;");
    if (excl.isValid()) {
        QJniEnvironment env;
        JNIEnv* e = env.jniEnv();
        jobjectArray arr = static_cast<jobjectArray>(excl.object());
        jsize n = e->GetArrayLength(arr);
        for (jsize i = 0; i < n; ++i) {
            jstring s = static_cast<jstring>(e->GetObjectArrayElement(arr, i));
            if (s) {
                const char* c = e->GetStringUTFChars(s, nullptr);
                if (c) { p.exclusions.append(QString::fromUtf8(c)); e->ReleaseStringUTFChars(s, c); }
                e->DeleteLocalRef(s);
            }
        }
    }
    p.set = !p.host.isEmpty() || p.port != 0;
    return p;
}

// Installed packages helper.  Requires QUERY_ALL_PACKAGES on
// API 30+ to see packages other than the app's own; without it the list is
// partial and the diagnostic says so.
static QStringList androidInstalledPackages() {
    QStringList out;
    QJniObject ctx = getQtActivity();
    if (!ctx.isValid()) return out;
    QJniObject pm = ctx.callObjectMethod("getPackageManager", "()Landroid/content/pm/PackageManager;");
    if (!pm.isValid()) return out;
    QJniObject pkgs = pm.callObjectMethod("getInstalledPackages", "(I)Ljava/util/List;", 0);
    if (!pkgs.isValid()) return out;

    QJniEnvironment env;
    JNIEnv* e = env.jniEnv();
    // 5WHY: the raw GetFieldID below uses JNIEnv directly — Qt does NOT clear
    // exceptions for raw calls, so a failed lookup would poison later JNI
    // calls on this thread.  clearJniException() after each raw call.
    jint size = pkgs.callMethod<jint>("size", "()I");
    for (jint i = 0; i < size; ++i) {
        QJniObject pi = pkgs.callObjectMethod("get", "(I)Ljava/lang/Object;", i);
        if (!pi.isValid()) continue;
        jclass cls = e->GetObjectClass(pi.object());
        jfieldID fid = e->GetFieldID(cls, "packageName", "Ljava/lang/String;");
        if (clearJniException(env)) { e->DeleteLocalRef(cls); continue; }
        if (fid) {
            jstring s = static_cast<jstring>(e->GetObjectField(pi.object(), fid));
            if (s) {
                const char* c = e->GetStringUTFChars(s, nullptr);
                if (c) { out.append(QString::fromUtf8(c)); e->ReleaseStringUTFChars(s, c); }
                e->DeleteLocalRef(s);
            }
        }
        e->DeleteLocalRef(cls);
    }
    return out;
}

// ── Public diagnostic entry points ─────────────────────────────────────

QString androidNetworkTypeInfo() {
    return androidConnectivityInfo();
}

// 5WHY: Used hardcoded note about ACCESS_FINE_LOCATION — never checked
// actual permission status, never retrieved BSSID (API available), never
// showed actionable guidance, and Signal/Channel was a fake placeholder
// ("Requires API 29+") that never computed anything.  Now the snapshot
// returns real SSID/BSSID/RSSI/frequency/link-speed (all available from
// API 21 with location permission; RSSI/frequency even without it).
DiagnosticResult androidWifiDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();

    AndroidWifiInfo w = androidWifiInfo();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Wireless LAN information:"));
    out.append(QString());

    if (w.permissionDenied) {
        out.append(QStringLiteral("  %1").arg(w.error));
        r.summary = QStringLiteral("WiFi: Location permission required");
        r.status = DiagStatus::Warning;
    } else if (!w.valid || !w.connected) {
        out.append(QStringLiteral("  %1").arg(
            w.error.isEmpty() ? QStringLiteral("SSID: (not connected or unavailable)") : w.error));
        // Even when not associated, RSSI/frequency are often reported — show them
        if (w.frequency > 0 || (w.rssi != -127 && w.rssi != 0))
            out.append(QStringLiteral("  Signal: %1 dBm").arg(w.rssi));
        r.summary = QStringLiteral("No WiFi");
        r.status = DiagStatus::Info;
    } else {
        out.append(QStringLiteral("  SSID: %1").arg(w.ssid.isEmpty() ? QStringLiteral("(unavailable)") : w.ssid));
        out.append(QStringLiteral("  BSSID: %1").arg(w.bssid.isEmpty() ? QStringLiteral("(unavailable)") : w.bssid));
        if (w.frequency > 0) {
            int ch = wifiChannelFromFrequency(w.frequency);
            QString band;
            if (w.frequency >= 5170 && w.frequency <= 5825)
                band = QStringLiteral("5 GHz");
            else if (w.frequency > 5825)
                band = QStringLiteral("6 GHz");
            else
                band = QStringLiteral("2.4 GHz");
            out.append(QStringLiteral("  Channel: %1 (%2 MHz, %3)")
                .arg(ch ? QString::number(ch) : QStringLiteral("?"))
                .arg(w.frequency).arg(band));
        }
        if (w.rssi != -127 && w.rssi != 0) {
            int level = wifiSignalLevel(w.rssi);
            out.append(QStringLiteral("  Signal: %1 (%2 dBm)").arg(signalGlyphs(level)).arg(w.rssi));
            out.append(QStringLiteral("  Signal Level: %1/4 (WifiManager.calculateSignalLevel)").arg(level));
        }
        if (w.linkSpeedMbps > 0)
            out.append(QStringLiteral("  Link Speed: %1 Mbps").arg(w.linkSpeedMbps));
        // Interface MTU from /sys/class/net (world-readable on Android)
        for (const QString& ifName : {QStringLiteral("wlan0"), QStringLiteral("wlan1"), QStringLiteral("wifi0")}) {
            QFile mtuFile(QStringLiteral("/sys/class/net/%1/mtu").arg(ifName));
            if (mtuFile.open(QIODevice::ReadOnly)) {
                out.append(QStringLiteral("  Interface %1 MTU: %2").arg(
                    ifName, QString::fromLatin1(mtuFile.readAll().trimmed())));
                break;
            }
        }
        r.summary = QStringLiteral("WiFi: %1").arg(w.ssid.isEmpty() ? w.bssid : w.ssid);
        r.status = DiagStatus::Pass;
    }

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    return r;
}

DiagnosticResult androidCellularDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();

    QVariantMap cell = androidCellularInfo();
    const bool hasPhonePerm = androidHasPhonePermission();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Cellular Information:"));
    out.append(QString());

    if (!cell.isEmpty()) {
        if (cell.contains("carrierName"))
            out.append(QStringLiteral("  Carrier: %1").arg(cell["carrierName"].toString()));
        else if (!hasPhonePerm)
            out.append(QStringLiteral("  Carrier: (grant READ_PHONE_STATE / READ_BASIC_PHONE_STATE to see)"));
        if (cell.contains("radioAccess"))
            out.append(QStringLiteral("  Radio Access (network type): %1").arg(cell["radioAccess"].toString()));
        if (cell.contains("dataNetworkType"))
            out.append(QStringLiteral("  Data Network Type: %1").arg(cell["dataNetworkType"].toString()));
        if (cell.contains("mcc") && cell.contains("mnc"))
            out.append(QStringLiteral("  MCC/MNC: %1-%2").arg(cell["mcc"].toString(), cell["mnc"].toString()));
        else if (!hasPhonePerm)
            out.append(QStringLiteral("  MCC/MNC: (grant READ_PHONE_STATE to see)"));
        if (cell.contains("phoneType"))
            out.append(QStringLiteral("  Phone Type: %1").arg(cell["phoneType"].toString()));
        if (cell.contains("simState"))
            out.append(QStringLiteral("  SIM State: %1").arg(cell["simState"].toString()));
        if (cell.contains("dataState"))
            out.append(QStringLiteral("  Data State: %1").arg(cell["dataState"].toString()));
        if (cell.contains("roaming"))
            out.append(QStringLiteral("  Roaming: %1").arg(cell["roaming"].toBool() ? "Yes" : "No"));
        if (cell.contains("signalLevel")) {
            QString signalLine = QStringLiteral("  Signal: %1")
                .arg(signalGlyphs(cell["signalLevel"].toInt()));
            if (cell.contains("rsrp"))
                signalLine += QStringLiteral(" (RSRP %1)").arg(cell["rsrp"].toString());
            else if (cell.contains("cdmaRssi"))
                signalLine += QStringLiteral(" (RSSI %1)").arg(cell["cdmaRssi"].toString());
            out.append(signalLine);
        }
        r.status = DiagStatus::Pass;
        QString carrier = cell.value("carrierName").toString();
        r.summary = QStringLiteral("Carrier: %1").arg(
            carrier.isEmpty() ? QStringLiteral("(unknown)") : carrier);
    } else {
        out.append(QStringLiteral("  No cellular service available"));
        r.status = DiagStatus::Info;
        r.summary = QStringLiteral("No cellular service");
    }

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    return r;
}

// 5WHY: Used hardcoded plain-text string — table view was desktop-only.
// Now uses DiagnosticFormatter::formatTable for consistent display on all
// platforms (matching iOS, macOS, desktop implementations).
// 5WHY (round 2): the lease file is not exposed, but the DHCP-assigned
// result (IP/gateway/DNS/MTU) IS visible via LinkProperties — report it
// instead of a bare "(not exposed)".  Also fixed "dumpsh" → "dumpsys".
DiagnosticResult androidDhcpDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();

    static const QVector<DiagnosticFormatter::ColSpec> kDhcpCols = {
        {"Interface", 18, false},
        {"DHCP",       6, false},
        {"IP Address", 18, false},
        {"Server",     0, false},
    };

    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("DHCP Client Status"));
    out.append(QString());

    QList<QStringList> rows;
    rows.append({"(system-managed)", "Yes", "(not exposed)", "(not exposed)"});
    out << DiagnosticFormatter::formatTable(kDhcpCols, rows);
    out.append(QString());
    out.append(QStringLiteral("  Android manages DHCP at the system level — the lease"));
    out.append(QStringLiteral("  is not directly exposed, but the assigned values are:"));

    // DHCP-assigned values visible via LinkProperties (no permission needed)
    const QStringList addrs = androidLinkAddresses();
    QString gateway;
    for (const auto& rt : androidRoutes())
        if (rt.isDefault && rt.hasGateway) { gateway = rt.gateway; break; }
    const QStringList dns = androidDnsServers();
    const int mtu = androidLinkMtu();

    if (!addrs.isEmpty())
        out.append(QStringLiteral("  IP Address(es): %1").arg(addrs.join(QStringLiteral(", "))));
    if (!gateway.isEmpty())
        out.append(QStringLiteral("  Gateway (DHCP router): %1").arg(gateway));
    if (!dns.isEmpty())
        out.append(QStringLiteral("  DNS Servers (from DHCP): %1").arg(dns.join(QStringLiteral(", "))));
    if (mtu > 0)
        out.append(QStringLiteral("  Interface MTU: %1").arg(mtu));
    if (addrs.isEmpty() && gateway.isEmpty() && dns.isEmpty())
        out.append(QStringLiteral("  (no active network — nothing assigned)"));
    out.append(QStringLiteral("  For full lease details: adb shell dumpsys dhcpclient"));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.summary = QStringLiteral("System-managed (Android)");
    r.status = DiagStatus::Info;
    return r;
}

// 5WHY: was a hardcoded placeholder that never read the gateway.  Android
// blocks /proc/net/route, but LinkProperties.getRoutes() exposes the default
// route (gateway + interface) — real data, no root, no permission.
DiagnosticResult androidGatewayDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Default Gateway:"));
    out.append(QString());

    QString gateway, iface;
    const auto routes = androidRoutes();
    for (const auto& rt : routes) {
        if (rt.isDefault) {
            gateway = rt.gateway;
            iface = rt.iface;
            break;
        }
    }
    if (!gateway.isEmpty()) {
        out.append(QStringLiteral("  Default Gateway . . . . . . . . . : %1").arg(gateway));
        if (!iface.isEmpty())
            out.append(QStringLiteral("  Interface . . . . . . . . . . . . : %1").arg(iface));
        int extra = 0;
        for (const auto& rt : routes)
            if (rt.isDefault && !rt.gateway.isEmpty() && rt.gateway != gateway) extra++;
        if (extra > 0)
            out.append(QStringLiteral("  (+%1 additional default routes — VPN / policy routing)").arg(extra));
        r.summary = QStringLiteral("Default gateway: %1").arg(gateway);
        r.status = DiagStatus::Pass;
    } else if (!routes.isEmpty()) {
        out.append(QStringLiteral("  Default route present but no explicit gateway (on-link)."));
        r.summary = QStringLiteral("On-link default (no gateway)");
        r.status = DiagStatus::Info;
    } else {
        out.append(QStringLiteral("  No default gateway configured (no active network)."));
        r.summary = QStringLiteral("No default gateway");
        r.status = DiagStatus::Warning;
    }
    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    return r;
}

// ── G2 Routing Table ───────────────────────────────────────────────────
// Android blocks /proc/net/route for apps; LinkProperties.getRoutes() is
// the official replacement (same data Android's own UI shows).
DiagnosticResult androidRoutingTableDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Routing Table (ConnectivityManager.getLinkProperties):"));
    out.append(QString());

    static const QVector<DiagnosticFormatter::ColSpec> kRouteCols = {
        {"Interface", 12, false},
        {"Destination", 24, false},
        {"Gateway", 18, false},
        {"Flags", 8, false},
    };
    QList<QStringList> rows;
    const auto routes = androidRoutes();
    for (const auto& rt : routes) {
        QString flags;
        if (rt.isDefault) flags += QStringLiteral("D");
        if (rt.hasGateway) flags += QStringLiteral("G");
        if (flags.isEmpty()) flags = QStringLiteral("-");
        QString dest = rt.dest.isEmpty() ? QStringLiteral("0.0.0.0/0") : QStringLiteral("%1/%2").arg(rt.dest).arg(rt.prefixLen);
        rows.append({rt.iface, dest, rt.hasGateway ? rt.gateway : QStringLiteral("*"), flags});
    }
    if (rows.isEmpty()) {
        out.append(QStringLiteral("  No routes (no active network)"));
        r.summary = QStringLiteral("No active network");
        r.status = DiagStatus::Warning;
    } else {
        out << DiagnosticFormatter::formatTable(kRouteCols, rows);
        out.append(QString());
        out.append(QStringLiteral("  (source: active network LinkProperties; VPN/policy routes may add more)"));
        r.summary = QStringLiteral("%1 route%2").arg(rows.size()).arg(rows.size() > 1 ? "s" : "");
        r.status = DiagStatus::Pass;
    }
    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    return r;
}

// ── G2 Proxy Settings ─────────────────────────────────────────────────
// Android applies a system-wide HTTP proxy (from Wi-Fi settings); it is not
// exposed as environment variables.  ConnectivityManager.getDefaultProxy()
// is the official public API.
DiagnosticResult androidProxyDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Proxy Settings (ConnectivityManager.getDefaultProxy):"));
    out.append(QString());

    AndroidProxy p = androidProxy();
    if (p.set) {
        out.append(QStringLiteral("  HTTP Proxy: %1:%2").arg(p.host).arg(p.port));
        if (!p.exclusions.isEmpty())
            out.append(QStringLiteral("  Bypass list: %1").arg(p.exclusions.join(QStringLiteral(", "))));
        out.append(QStringLiteral("  (applies to system-wide traffic; VPN apps may override per-app)"));
        r.summary = QStringLiteral("Proxy: %1:%2").arg(p.host).arg(p.port);
        r.status = DiagStatus::Info;
    } else {
        out.append(QStringLiteral("  No system HTTP proxy configured."));
        r.summary = QStringLiteral("No proxy");
        r.status = DiagStatus::Pass;
    }
    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    return r;
}

// ── G2 TCP Settings ───────────────────────────────────────────────────
// /proc/sys/net is restricted on Android.  The equivalent network policy
// state (metered/validated/private-DNS/bandwidth/MTU) is public via
// NetworkCapabilities + LinkProperties.
DiagnosticResult androidTcpSettingsDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("TCP / Network Settings (Android)"));
    out.append(QString());
    out.append(QStringLiteral("  /proc/sys/net is restricted on Android; the equivalent"));
    out.append(QStringLiteral("  network policy state comes from NetworkCapabilities:"));
    out.append(QString());

    AndroidCaps caps = androidCapabilities();
    if (!caps.available) {
        out.append(QStringLiteral("  No active network"));
        r.summary = QStringLiteral("No active network");
        r.status = DiagStatus::Warning;
    } else {
        out.append(QStringLiteral("  Internet capability: %1").arg(caps.internet ? "Yes" : "No"));
        out.append(QStringLiteral("  Validated (full connectivity): %1").arg(caps.validated ? "Yes" : "No"));
        out.append(QStringLiteral("  Metered: %1").arg(caps.notMetered ? "No" : "Yes"));
        out.append(QStringLiteral("  Restricted: %1").arg(caps.notRestricted ? "No" : "Yes"));
        out.append(QStringLiteral("  Private DNS (DoT): %1").arg(caps.privateDns ? "Active" : "Off"));
        if (caps.downKbps > 0)
            out.append(QStringLiteral("  Link bandwidth: ↓%1 kbps / ↑%2 kbps").arg(caps.downKbps).arg(caps.upKbps));
        int mtu = androidLinkMtu();
        if (mtu > 0)
            out.append(QStringLiteral("  Interface MTU: %1").arg(mtu));
        r.summary = QStringLiteral("Network policy state collected");
        r.status = DiagStatus::Pass;
    }
    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    return r;
}

// ── G2 ARP Table ──────────────────────────────────────────────────────
// /proc/net/arp is blocked by Android 10+ SELinux on most devices, but
// attempting the read costs nothing and returns real data where permitted.
static const char* androidTcpStateName(int st) {
    switch (st) {
        case 1: return "ESTABLISHED";
        case 2: return "SYN_SENT";
        case 3: return "SYN_RECV";
        case 4: return "FIN_WAIT1";
        case 5: return "FIN_WAIT2";
        case 6: return "TIME_WAIT";
        case 7: return "CLOSE";
        case 8: return "CLOSE_WAIT";
        case 9: return "LAST_ACK";
        case 10: return "LISTEN";
        case 11: return "CLOSING";
        default: return "UNKNOWN";
    }
}
DiagnosticResult androidArpTableDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("ARP / Neighbour Table"));
    out.append(QString());

    QFile f(QStringLiteral("/proc/net/arp"));
    int shown = 0;
    if (f.open(QIODevice::ReadOnly)) {
        QTextStream ts(&f);
        QString header = ts.readLine();
        if (!header.isEmpty())
            out.append(QStringLiteral("  %1").arg(header));
        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            if (line.isEmpty()) continue;
            out.append(QStringLiteral("  %1").arg(line));
            shown++;
        }
        r.summary = shown > 0 ? QStringLiteral("%1 ARP entr%2").arg(shown).arg(shown > 1 ? "ies" : "y")
                              : QStringLiteral("Empty ARP table");
        r.status = DiagStatus::Pass;
    } else {
        out.append(QStringLiteral("  /proc/net/arp is not readable on this device (Android 10+ restricts it)."));
        out.append(QStringLiteral("  The neighbour table is kernel-managed with no public API."));
        out.append(QStringLiteral("  Reference: `adb shell ip neigh` (requires adb)."));
        r.summary = QStringLiteral("ARP table not accessible");
        r.status = DiagStatus::Info;
    }
    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    return r;
}

// ── G2 Network Profile ────────────────────────────────────────────────
// Enriched with real connectivity/WiFi/cellular/VPN state instead of the
// bare hostname the shared Linux path produced on Android.
DiagnosticResult androidNetworkProfileDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Network Profile Information:"));
    out.append(QString());

    char hostname[256] = {};
    gethostname(hostname, sizeof(hostname));
    out.append(QStringLiteral("  Hostname: %1").arg(QString::fromLatin1(hostname)));
    out.append(QStringLiteral("  Connection Type: %1").arg(androidConnectivityInfo()));

    AndroidCaps caps = androidCapabilities();
    if (caps.available) {
        if (caps.vpn)
            out.append(QStringLiteral("  VPN: Active (traffic routed through a VPN tunnel)"));
        out.append(QStringLiteral("  Metered Connection: %1").arg(caps.notMetered ? "No" : "Yes"));
        out.append(QStringLiteral("  Private DNS (DoT): %1").arg(caps.privateDns ? "Active" : "Off"));
    }

    AndroidWifiInfo w = androidWifiInfo();
    if (w.permissionDenied) {
        out.append(QStringLiteral("  Active WiFi: (grant Location permission to see SSID)"));
    } else if (w.valid) {
        QString ssid = w.ssid.isEmpty() ? QStringLiteral("(unavailable)") : w.ssid;
        QString bssid = w.bssid.isEmpty() ? QString() : QStringLiteral(" (BSSID %1)").arg(w.bssid);
        out.append(QStringLiteral("  Active WiFi: %1%2").arg(ssid, bssid));
    }

    QVariantMap cell = androidCellularInfo();
    QString carrier = cell.value("carrierName").toString();
    QString radio = cell.value("radioAccess").toString();
    if (!carrier.isEmpty())
        out.append(QStringLiteral("  Cellular Carrier: %1%2").arg(carrier, radio.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(radio)));
    else if (!cell.isEmpty())
        out.append(QStringLiteral("  Cellular Carrier: (not available)"));
    else if (caps.cellular)
        out.append(QStringLiteral("  Cellular Carrier: (grant READ_PHONE_STATE to see)"));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = QStringLiteral("Network Profile Collected");
    return r;
}

// ── DNS Resolution via InetAddress ─────────────────────────────────────
// 5WHY: timeoutMs parameter was completely ignored — getByName() is a
// synchronous blocking JNI call with no built-in timeout. On unreachable
// DNS servers this blocks the calling thread indefinitely.  Wrap in
// QtConcurrent so a deadline can enforce the timeout.
// 5WHY (round 2): getByName() throws UnknownHostException on NXDOMAIN — the
// JNI exception was never cleared, poisoning later JNI calls on the thread.
// Now every lookup clears exceptions after the call.
static QString androidResolveOne(const QString& host, int timeoutMs) {
    QFuture<QString> future = QtConcurrent::run([host]() -> QString {
        QJniEnvironment env;
        QJniObject hostStr = QJniObject::fromString(host);
        QJniObject inetAddr = QJniObject::callStaticObjectMethod(
            "java/net/InetAddress", "getByName",
            "(Ljava/lang/String;)Ljava/net/InetAddress;",
            hostStr.object<jstring>());
        if (clearJniException(env)) return QString();  // UnknownHostException
        if (!inetAddr.isValid()) return QString();
        QJniObject ipStr = inetAddr.callObjectMethod("getHostAddress", "()Ljava/lang/String;");
        return ipStr.isValid() ? ipStr.toString() : QString();
    });
    // 5WHY: Android is pinned to Qt 6.5.3, whose QFuture::waitForFinished()
    // has NO timeout overload (added in Qt 6.6).  The old Qt-6.5 branch
    // called waitForFinished() unconditionally — an unreachable resolver
    // blocks InetAddress.getByName() for 30-120s, hanging the diagnostic
    // thread and leaking it past the task watchdog.  Poll isFinished()
    // against a wall-clock deadline: works on every Qt version and bounds
    // the caller's wait.  On timeout the JNI thread stays blocked but is
    // detached — the caller returns empty instead of hanging.
    const int budget = timeoutMs > 0 ? timeoutMs : 3000;
    QElapsedTimer timer; timer.start();
    while (!future.isFinished()) {
        if (timer.elapsed() >= budget) return QString();
        QThread::msleep(20);
    }
    return future.result();
}

DiagnosticResult androidDnsDiag(DiagId id, const QString& target) {
    DiagnosticResult r; r.id = id; r.group = diagGroup(id);
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();

    QString host = target;
    if (host.contains("://")) { QUrl u(host); host = u.host(); }

    // 5WHY: getByName() returned only the FIRST address.  getAllByName()
    // returns every A + AAAA record — dig-style multi-record output.
    QStringList ips;
    {
        QFuture<QStringList> future = QtConcurrent::run([host]() -> QStringList {
            QStringList result;
            QJniEnvironment env;
            QJniObject hostStr = QJniObject::fromString(host);
            QJniObject addrArr = QJniObject::callStaticObjectMethod(
                "java/net/InetAddress", "getAllByName",
                "(Ljava/lang/String;)[Ljava/net/InetAddress;", hostStr.object<jstring>());
            if (clearJniException(env)) return result;  // UnknownHostException
            if (!addrArr.isValid()) return result;
            JNIEnv* e = env.jniEnv();
            jobjectArray arr = static_cast<jobjectArray>(addrArr.object());
            jsize count = e->GetArrayLength(arr);
            for (jsize i = 0; i < count; ++i) {
                jobject addrObj = e->GetObjectArrayElement(arr, i);
                if (!addrObj) continue;
                QJniObject addr(addrObj);
                QJniObject ipStr = addr.callObjectMethod("getHostAddress", "()Ljava/lang/String;");
                if (ipStr.isValid()) {
                    QString ip = ipStr.toString();
                    if (!result.contains(ip)) result.append(ip);
                }
                e->DeleteLocalRef(addrObj);
            }
            return result;
        });
        const int budget = 3000;
        QElapsedTimer timer; timer.start();
        while (!future.isFinished()) {
            if (timer.elapsed() >= budget) break;
            QThread::msleep(20);
        }
        if (future.isFinished()) ips = future.result();
    }
    qint64 elapsed = t.elapsed();
    r.durationMs = elapsed;

    // Dig-style output via shared DiagnosticFormatter
    QStringList out;
    out << DiagnosticFormatter::formatDnsHeader(host,
        ips.isEmpty() ? "SERVFAIL" : "NOERROR",
        (uint16_t)(qHash(host) & 0xFFFF), ips.isEmpty() ? 0 : ips.size());
    out.append(QStringLiteral(";; QUESTION SECTION:"));
    out.append(DiagnosticFormatter::formatDnsQuestion(host));
    out.append(QString());
    if (!ips.isEmpty()) {
        out.append(QStringLiteral(";; ANSWER SECTION:"));
        for (const auto& ip : ips) {
            const char* rtype = ip.contains(':') ? "AAAA" : "A";
            out.append(DiagnosticFormatter::formatDnsRecord(host, 0, rtype, ip));
        }
        out.append(QString());
    }
    out << DiagnosticFormatter::formatDnsFooter(elapsed, "system resolver (InetAddress.getAllByName)");
    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = ips.isEmpty() ? DiagStatus::Fail : DiagStatus::Pass;
    r.summary = ips.isEmpty() ? QStringLiteral("DNS failed")
                              : QStringLiteral("Resolved: %1").arg(ips.join(QStringLiteral(", ")));
    return r;
}

// ── HTTP Diagnostics via HttpURLConnection ─────────────────────────────

namespace {

// Helper: read response headers from HttpURLConnection as a QString.
// 5WHY: getHeaderFields() returns Map<String, List<String>> — the old code
// only took the FIRST value per key, dropping multi-value headers
// (Set-Cookie, Vary, …).  Now emits every value per header line.
QString readHeaders(QJniObject& httpConn) {
    QJniObject headerMap = httpConn.callObjectMethod("getHeaderFields",
        "()Ljava/util/Map;");
    if (!headerMap.isValid()) return {};
    // Map.entrySet() → Iterable<Map.Entry>
    QJniObject entries = headerMap.callObjectMethod("entrySet", "()Ljava/util/Set;");
    if (!entries.isValid()) return {};
    QJniObject iter = entries.callObjectMethod("iterator", "()Ljava/util/Iterator;");
    QString result;
    while (iter.callMethod<jboolean>("hasNext")) {
        QJniObject entry = iter.callObjectMethod("next", "()Ljava/lang/Object;");
        QJniObject key = entry.callObjectMethod("getKey", "()Ljava/lang/Object;");
        QJniObject val = entry.callObjectMethod("getValue", "()Ljava/lang/Object;");
        // val is a List<String>
        QJniObject vals = val;
        jint n = vals.callMethod<jint>("size", "()I");
        QStringList values;
        for (jint i = 0; i < n; ++i) {
            QJniObject v = vals.callObjectMethod("get", "(I)Ljava/lang/Object;", i);
            if (v.isValid()) values << v.toString();
        }
        if (key.isValid()) {
            for (const auto& v : values)
                result += QStringLiteral("%1: %2\n").arg(key.toString(), v);
        } else {
            // null key = HTTP status line ("HTTP/1.1 200 OK")
            for (const auto& v : values)
                result += v + "\n";
        }
    }
    return result;
}

// Read response body; on error responses (4xx/5xx) getInputStream() throws
// and getErrorStream() must be used instead.  Always clears JNI exceptions so
// a failed read never poisons later JNI calls on this thread.
static QString readHttpBody(QJniObject& httpConn, int maxBytes) {
    QJniObject inStream = httpConn.callObjectMethod("getInputStream", "()Ljava/io/InputStream;");
    QJniEnvironment env;
    if (clearJniException(env) || !inStream.isValid()) {
        QJniEnvironment errEnv;
        inStream = httpConn.callObjectMethod("getErrorStream", "()Ljava/io/InputStream;");
        clearJniException(errEnv);
        if (!inStream.isValid()) return {};
    }
    QJniObject bufReader("java/io/BufferedReader",
        "(Ljava/io/Reader;)V",
        QJniObject("java/io/InputStreamReader",
            "(Ljava/io/InputStream;)V", inStream.object()).object());
    QString body;
    QJniObject line;
    while ((line = bufReader.callObjectMethod("readLine", "()Ljava/lang/String;")).isValid()
           && body.size() < maxBytes)
        body += line.toString() + "\n";
    return body;
}

// Rich TLS certificate details from HttpsURLConnection (cipher suite +
// per-certificate subject/issuer/validity/days-left/SHA-256 thumbprint).
// Falls back gracefully for plain-HTTP connections (NoSuchMethodError).
static QString androidCertDetails(QJniObject& httpConn) {
    QJniEnvironment env;
    JNIEnv* e = env.jniEnv();
    QString out;

    QJniObject cipher = httpConn.callObjectMethod("getCipherSuite", "()Ljava/lang/String;");
    if (!clearJniException(env) && cipher.isValid())
        out += QStringLiteral("Cipher Suite: %1\n").arg(cipher.toString());

    QJniObject serverCerts = httpConn.callObjectMethod("getServerCertificates",
        "()[Ljava/security/cert/Certificate;");
    if (clearJniException(env) || !serverCerts.isValid())
        return out + QStringLiteral("(no certificate chain available)\n");

    jobjectArray arr = static_cast<jobjectArray>(serverCerts.object());
    jsize count = e->GetArrayLength(arr);
    out += QStringLiteral("Server certificates: %1\n").arg(count);
    for (jsize i = 0; i < count && i < 3; ++i) {
        jobject certObj = e->GetObjectArrayElement(arr, i);
        if (!certObj) continue;
        QJniObject cert(certObj);
        QJniObject subj = cert.callObjectMethod("getSubjectX500Principal",
            "()Ljavax/security/auth/x500/X500Principal;");
        QJniObject issuer = cert.callObjectMethod("getIssuerX500Principal",
            "()Ljavax/security/auth/x500/X500Principal;");
        QJniObject notBefore = cert.callObjectMethod("getNotBefore", "()Ljava/util/Date;");
        QJniObject notAfter = cert.callObjectMethod("getNotAfter", "()Ljava/util/Date;");
        QString subject = subj.isValid()
            ? subj.callObjectMethod("getName", "()Ljava/lang/String;").toString()
            : QStringLiteral("?");
        QString issuerStr = issuer.isValid()
            ? issuer.callObjectMethod("getName", "()Ljava/lang/String;").toString()
            : QStringLiteral("?");
        out += QStringLiteral("  Cert %1:\n").arg(i + 1);
        out += QStringLiteral("    Subject: %1\n").arg(subject);
        out += QStringLiteral("    Issuer:  %1\n").arg(issuerStr);
        if (notBefore.isValid() && notAfter.isValid()) {
            qint64 fromMs = notBefore.callMethod<jlong>("getTime", "()J");
            qint64 toMs = notAfter.callMethod<jlong>("getTime", "()J");
            QDateTime from = QDateTime::fromMSecsSinceEpoch(fromMs);
            QDateTime to = QDateTime::fromMSecsSinceEpoch(toMs);
            qint64 daysLeft = QDateTime::currentDateTime().daysTo(to);
            out += QStringLiteral("    Valid:   %1 → %2 (%3 days left)\n")
                .arg(from.toString(QStringLiteral("yyyy-MM-dd")),
                     to.toString(QStringLiteral("yyyy-MM-dd")))
                .arg(daysLeft);
            if (daysLeft < 0) out += QStringLiteral("    ⚠ EXPIRED\n");
            else if (daysLeft < 30) out += QStringLiteral("    ⚠ Expires soon\n");
        }
        // SHA-256 thumbprint
        QJniObject encoded = cert.callObjectMethod("getEncoded", "()[B");
        if (encoded.isValid()) {
            jbyteArray jba = encoded.object<jbyteArray>();
            jsize len = e->GetArrayLength(jba);
            jbyte* bytes = e->GetByteArrayElements(jba, nullptr);
            QByteArray der(reinterpret_cast<const char*>(bytes), (int)len);
            e->ReleaseByteArrayElements(jba, bytes, JNI_ABORT);
            QByteArray thumb = QCryptographicHash::hash(der, QCryptographicHash::Sha256).toHex();
            out += QStringLiteral("    SHA-256: %1\n").arg(QString::fromLatin1(thumb).left(40));
        }
        e->DeleteLocalRef(certObj);
    }
    return out;
}

// Check 7 standard security headers and return audit string
QString auditSecurityHeaders(QJniObject& httpConn) {
    static const char* securityHeaders[] = {
        "Strict-Transport-Security", "Content-Security-Policy",
        "X-Frame-Options", "X-Content-Type-Options",
        "Referrer-Policy", "Permissions-Policy",
        "X-XSS-Protection"
    };
    QStringList present, missing;
    for (const char* h : securityHeaders) {
        QJniObject hdr = httpConn.callObjectMethod("getHeaderField",
            "(Ljava/lang/String;)Ljava/lang/String;",
            QJniObject::fromString(QString::fromLatin1(h)).object<jstring>());
        if (hdr.isValid() && !hdr.toString().isEmpty())
            present.append(QString::fromLatin1(h));
        else
            missing.append(QString::fromLatin1(h));
    }
    QString r;
    r += QString::number(present.size()) + "/7 security headers present\n";
    if (!present.isEmpty())
        r += "  Present: " + present.join(", ") + "\n";
    if (!missing.isEmpty())
        r += "  Missing: " + missing.join(", ") + "\n";
    return r;
}

} // anonymous namespace

DiagnosticResult androidHttpDiag(DiagId id, const QString& target) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G5;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();

    // 5WHY (round 2 fixes):
    //   · getResponseCode() throws IOException on failure (cleartext policy,
    //     DNS failure, refused) — that is a JNI exception, NOT a C++ exception,
    //     so the old try/catch never fired; the pending exception poisoned
    //     later JNI calls on this thread.  Cleared explicitly now.
    //   · Accept-Encoding was never sent → G5HttpCompression always reported
    //     "none".  Now sent explicitly (and caches disabled so a stale cached
    //     response cannot skew measurements).
    QJniObject urlStr = QJniObject::fromString(target);
    QJniObject url = QJniObject("java/net/URL", "(Ljava/lang/String;)V", urlStr.object<jstring>());
    // 5WHY: new URL(malformed) throws MalformedURLException.  QJniObject's
    // constructor stores a global ref even when the Java ctor threw, so
    // url.isValid() may be true with a pending exception — clear it so it
    // cannot poison the next JNI call.
    {
        QJniEnvironment env;
        clearJniException(env);
    }
    if (!url.isValid()) {
        r.status = DiagStatus::Fail; r.summary = QStringLiteral("Invalid URL"); r.durationMs = t.elapsed(); return r;
    }

    QJniObject conn = url.callObjectMethod("openConnection", "()Ljava/net/URLConnection;");
    {
        QJniEnvironment env;
        clearJniException(env);   // IOException: unsupported protocol, etc.
    }
    if (!conn.isValid()) {
        r.status = DiagStatus::Fail; r.summary = QStringLiteral("Connection failed"); r.durationMs = t.elapsed(); return r;
    }

    QJniObject httpConn = conn;
    httpConn.callMethod<void>("setConnectTimeout", "(I)V", 10000);
    httpConn.callMethod<void>("setReadTimeout", "(I)V", 15000);
    httpConn.callMethod<void>("setUseCaches", "(Z)V", false);
    httpConn.callMethod<void>("setRequestProperty", "(Ljava/lang/String;Ljava/lang/String;)V",
        QJniObject::fromString("User-Agent").object<jstring>(),
        QJniObject::fromString("NetDiagnostics/1.0 (Android)").object<jstring>());
    httpConn.callMethod<void>("setRequestProperty", "(Ljava/lang/String;Ljava/lang/String;)V",
        QJniObject::fromString("Accept-Encoding").object<jstring>(),
        QJniObject::fromString("gzip, deflate").object<jstring>());

    // For redirect detection: don't follow automatically
    if (id == DiagId::G5HttpRedirect)
        httpConn.callMethod<void>("setInstanceFollowRedirects", "(Z)V", false);
    else
        httpConn.callMethod<void>("setInstanceFollowRedirects", "(Z)V", true);

    httpConn.callMethod<void>("setRequestMethod", "(Ljava/lang/String;)V",
        QJniObject::fromString("GET").object<jstring>());

    int responseCode = 0;
    {
        QJniEnvironment env;
        responseCode = httpConn.callMethod<jint>("getResponseCode");
        clearJniException(env);   // IOException: cleartext blocked / refused / timeout
    }
    qint64 dur = t.elapsed();

    if (responseCode == 0) {
        r.durationMs = dur;
        r.status = DiagStatus::Fail;
        r.summary = QStringLiteral("HTTP request failed (connectivity / cleartext policy)");
        r.rawOutput = QStringLiteral("\nHTTP request failed — no response.\n"
                                     "  Possible causes: no connectivity, server unreachable,\n"
                                     "  or the target uses plain http:// (see manifest cleartext flag).\n");
        r.details = r.rawOutput;
        return r;
    }

    QString headers = readHeaders(httpConn);
    r.rawOutput = headers;
    r.details = headers;

    switch (id) {
        case DiagId::G5CurlVerbose: {
            // 5WHY: getInputStream() throws on 4xx/5xx — readHttpBody() clears
            // the exception and falls back to getErrorStream().
            QString body = readHttpBody(httpConn, 2048);
            r.rawOutput = headers + "\n" + body;
            r.summary = QStringLiteral("HTTP %1, %2 bytes").arg(responseCode).arg(r.rawOutput.size());
            r.status = (responseCode >= 200 && responseCode < 400) ? DiagStatus::Pass : DiagStatus::Warning;
            break;
        }
        case DiagId::G5HttpHeaders:
            r.summary = QStringLiteral("HTTP %1, %2 headers").arg(responseCode)
                .arg(headers.count('\n'));
            r.status = DiagStatus::Pass;
            break;
        case DiagId::G5SecurityHeaders: {
            QString audit = auditSecurityHeaders(httpConn);
            r.rawOutput = headers + "\n" + audit;
            r.summary = audit.section('\n', 0, 0);
            r.status = audit.contains("Missing:") ? DiagStatus::Warning : DiagStatus::Pass;
            break;
        }
        case DiagId::G5SslCertificate: {
            // 5WHY (round 2): the old code only listed up to 3 subject DNs via a
            // broken array access.  androidCertDetails() now returns cipher
            // suite + full per-certificate subject/issuer/validity/thumbprint.
            QString certInfo = androidCertDetails(httpConn);
            r.rawOutput = certInfo;
            if (certInfo.contains("Server certificates: 0") || certInfo.contains("no certificate")) {
                r.summary = QStringLiteral("TLS connected (no cert details)");
                r.status = DiagStatus::Warning;
            } else {
                static const QRegularExpression certRe(QStringLiteral("Server certificates: (\\d+)"));
                auto m = certRe.match(certInfo);
                QString count = m.hasMatch() ? m.captured(1) : QStringLiteral("?");
                bool expired = certInfo.contains(QStringLiteral("EXPIRED"));
                r.summary = expired
                    ? QStringLiteral("TLS OK — cert EXPIRED (%1 in chain)").arg(count)
                    : QStringLiteral("TLS OK, %1 cert(s)").arg(count);
                r.status = expired ? DiagStatus::Warning : DiagStatus::Pass;
            }
            break;
        }
        case DiagId::G5HttpRedirect: {
            if (responseCode >= 300 && responseCode < 400) {
                QJniObject location = httpConn.callObjectMethod("getHeaderField",
                    "(Ljava/lang/String;)Ljava/lang/String;",
                    QJniObject::fromString("Location").object<jstring>());
                r.summary = QStringLiteral("Redirect %1 → %2").arg(responseCode)
                    .arg(location.isValid() ? location.toString() : QStringLiteral("unknown"));
                r.status = DiagStatus::Info;
            } else {
                r.summary = QStringLiteral("No redirect (HTTP %1)").arg(responseCode);
                r.status = DiagStatus::Pass;
            }
            break;
        }
        case DiagId::G5HttpCompression: {
            QJniObject contentEnc = httpConn.callObjectMethod("getContentEncoding",
                "()Ljava/lang/String;");
            QString enc = contentEnc.isValid() ? contentEnc.toString() : QString();
            if (enc.isEmpty()) {
                // Server may omit the getContentEncoding helper; check the raw header
                QJniObject ce = httpConn.callObjectMethod("getHeaderField",
                    "(Ljava/lang/String;)Ljava/lang/String;",
                    QJniObject::fromString("Content-Encoding").object<jstring>());
                enc = ce.isValid() ? ce.toString() : QStringLiteral("none");
            }
            // Body length (compressed vs raw) — extra signal
            QJniObject contentLength = httpConn.callObjectMethod("getHeaderField",
                "(Ljava/lang/String;)Ljava/lang/String;",
                QJniObject::fromString("Content-Length").object<jstring>());
            r.rawOutput = headers;
            if (contentLength.isValid() && !contentLength.toString().isEmpty())
                r.rawOutput += QStringLiteral("\nContent-Length: %1\n").arg(contentLength.toString());
            r.summary = QStringLiteral("Content-Encoding: %1").arg(enc);
            r.status = (enc == "none" || enc.isEmpty()) ? DiagStatus::Warning : DiagStatus::Pass;
            break;
        }
        case DiagId::G5HttpTiming:
            r.summary = QStringLiteral("HTTP %1 (%2ms, %3 headers)")
                .arg(responseCode).arg(dur).arg(headers.count('\n'));
            r.status = (responseCode >= 200 && responseCode < 400) ? DiagStatus::Pass : DiagStatus::Warning;
            break;
        default:
            r.summary = QStringLiteral("HTTP %1 (%2ms)").arg(responseCode).arg(dur);
            r.status = (responseCode >= 200 && responseCode < 400) ? DiagStatus::Pass : DiagStatus::Warning;
            break;
    }
    r.durationMs = dur;
    return r;
}

// namespace G5WebsiteUrl removed — see 5WHY above

// ═══════════════════════════════════════════════════════════════════════
// G1: System & Adapters (Android-native implementations)
// ═══════════════════════════════════════════════════════════════════════

// ── G1 Network Adapters ────────────────────────────────────────────────
// getifaddrs (works on Android NDK) + /sys/class/net MTU/state (world
// readable) + JNI enrichment (active transport, WiFi, cellular).
DiagnosticResult androidNetworkAdaptersDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Network Adapters (ifconfig -s style)"));
    out.append(QString());

    static const QVector<DiagnosticFormatter::ColSpec> kNetCols = {
        {"Iface", 12, false},
        {"MTU", 4, true},
        {"Status", 10, false},
        {"IPv4 Address", 0, false},
    };
    QList<QStringList> netRows;

    struct IfInfo { QString name; QStringList ips; int flags; };
    QMap<QString, IfInfo> ifMap;
    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) == 0) {
        for (auto* p = ifa; p; p = p->ifa_next) {
            if (!p->ifa_addr) continue;
            IfInfo& info = ifMap[QString::fromLatin1(p->ifa_name)];
            info.name = QString::fromLatin1(p->ifa_name);
            info.flags = p->ifa_flags;
            if (p->ifa_addr->sa_family == AF_INET) {
                auto* sa = (struct sockaddr_in*)p->ifa_addr;
                char b[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &sa->sin_addr, b, sizeof(b));
                info.ips.append(QString::fromLatin1(b));
            }
        }
        freeifaddrs(ifa);
    }
    for (auto it = ifMap.begin(); it != ifMap.end(); ++it) {
        const auto& info = it.value();
        bool isLoopback = (info.flags & IFF_LOOPBACK);
        bool isUp = (info.flags & IFF_UP) && (info.flags & IFF_RUNNING);
        QString mtu = QStringLiteral("-");
        QFile mtuFile(QStringLiteral("/sys/class/net/%1/mtu").arg(info.name));
        if (mtuFile.open(QIODevice::ReadOnly))
            mtu = QString::fromLatin1(mtuFile.readAll().trimmed());
        QString state = isLoopback ? QStringLiteral("UP") : (isUp ? QStringLiteral("UP") : QStringLiteral("DOWN"));
        QString ip4 = info.ips.isEmpty() ? (isLoopback ? QStringLiteral("127.0.0.1") : QStringLiteral("-"))
                                         : info.ips.join(',');
        netRows.append({info.name, mtu, state, ip4});
    }
    out << DiagnosticFormatter::formatTable(kNetCols, netRows);

    // Enrich with JNI data not visible in getifaddrs
    out.append(QString());
    out.append(QStringLiteral("  Active Connection: %1").arg(androidConnectivityInfo()));
    AndroidCaps caps = androidCapabilities();
    if (caps.available && caps.vpn)
        out.append(QStringLiteral("  VPN Tunnel: active"));
    AndroidWifiInfo w = androidWifiInfo();
    if (w.valid && !w.permissionDenied && !w.ssid.isEmpty())
        out.append(QStringLiteral("  WiFi SSID: %1").arg(w.ssid));
    QVariantMap cell = androidCellularInfo();
    if (!cell.value("carrierName").toString().isEmpty())
        out.append(QStringLiteral("  Cellular: %1 (%2)").arg(
            cell["carrierName"].toString(), cell.value("radioAccess").toString()));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = netRows.size() > 0
        ? QStringLiteral("%1 network adapter%2 enumerated").arg(netRows.size()).arg(netRows.size() > 1 ? "s" : "")
        : QStringLiteral("No network adapters found");
    r.durationMs = t.elapsed();
    return r;
}

// ── G1 NIC Advanced ────────────────────────────────────────────────────
// /sys/class/net driver details are restricted on Android, but the WiFi PHY
// state (link speed, frequency/channel, RSSI) and per-interface MTU/operstate
// are public — more useful than the desktop NIC driver dump anyway.
DiagnosticResult androidNicAdvancedDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("NIC Advanced Properties"));
    out.append(QString());

    AndroidWifiInfo w = androidWifiInfo();
    if (w.permissionDenied) {
        out.append(QStringLiteral("  %1").arg(w.error));
    } else if (w.valid) {
        if (w.linkSpeedMbps > 0)
            out.append(QStringLiteral("  WiFi Link Speed: %1 Mbps").arg(w.linkSpeedMbps));
        if (w.frequency > 0)
            out.append(QStringLiteral("  WiFi Frequency: %1 MHz (channel %2)")
                .arg(w.frequency).arg(wifiChannelFromFrequency(w.frequency)));
        if (w.rssi != -127)
            out.append(QStringLiteral("  WiFi RSSI: %1 dBm").arg(w.rssi));
    } else {
        out.append(QStringLiteral("  WiFi: %1").arg(w.error.isEmpty() ? QStringLiteral("not connected") : w.error));
    }
    out.append(QString());

    // Per-interface MTU / operstate (world-readable on Android)
    QDir netDir(QStringLiteral("/sys/class/net"));
    bool any = false;
    for (const auto& fi : netDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString ifName = fi.fileName();
        QFile mtuFile(QStringLiteral("/sys/class/net/%1/mtu").arg(ifName));
        QFile stateFile(QStringLiteral("/sys/class/net/%1/operstate").arg(ifName));
        QString mtu, state;
        if (mtuFile.open(QIODevice::ReadOnly)) mtu = QString::fromLatin1(mtuFile.readAll().trimmed());
        if (stateFile.open(QIODevice::ReadOnly)) state = QString::fromLatin1(stateFile.readAll().trimmed()).toUpper();
        out.append(QStringLiteral("  %1: MTU=%2, state=%3")
            .arg(ifName, mtu.isEmpty() ? QStringLiteral("?") : mtu, state.isEmpty() ? QStringLiteral("?") : state));
        any = true;
    }
    if (!any) out.append(QStringLiteral("  (no interfaces reported by /sys/class/net)"));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.summary = QStringLiteral("NIC details collected");
    r.status = DiagStatus::Pass;
    return r;
}

// ── G1 Wired Diagnostics ───────────────────────────────────────────────
// Phones have no wired NIC, but tablets/OTG with USB-Ethernet expose
// eth0/usb0/rndis0 under /sys/class/net — detect and report them honestly.
DiagnosticResult androidWiredDiagnosticsDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Wired (Ethernet) Diagnostics"));
    out.append(QString());

    QDir netDir(QStringLiteral("/sys/class/net"));
    QStringList wired;
    for (const auto& fi : netDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString ifName = fi.fileName();
        if (ifName == "lo" || ifName.startsWith("wlan") || ifName.startsWith("rmnet")
            || ifName.startsWith("radio") || ifName.startsWith("dummy"))
            continue;
        if (ifName.startsWith("eth") || ifName.startsWith("usb") || ifName.startsWith("rndis"))
            wired << ifName;
    }
    if (wired.isEmpty()) {
        out.append(QStringLiteral("  No wired Ethernet interface detected."));
        out.append(QStringLiteral("  (USB-Ethernet adapters appear as eth0/usb0/rndis0)"));
        r.summary = QStringLiteral("No wired interface");
        r.status = DiagStatus::Info;
    } else {
        for (const auto& ifName : wired) {
            QFile mtuFile(QStringLiteral("/sys/class/net/%1/mtu").arg(ifName));
            QFile stateFile(QStringLiteral("/sys/class/net/%1/operstate").arg(ifName));
            QString mtu = QStringLiteral("-"), state = QStringLiteral("?");
            if (mtuFile.open(QIODevice::ReadOnly)) mtu = QString::fromLatin1(mtuFile.readAll().trimmed());
            if (stateFile.open(QIODevice::ReadOnly)) state = QString::fromLatin1(stateFile.readAll().trimmed()).toUpper();
            out.append(QStringLiteral("  %1: MTU=%2, state=%3").arg(ifName, mtu, state));
        }
        r.summary = QStringLiteral("Wired: %1").arg(wired.join(QStringLiteral(", ")));
        r.status = DiagStatus::Pass;
    }
    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    return r;
}

// ── G1 IP Configuration ────────────────────────────────────────────────
// getifaddrs (all interface IPs) + LinkProperties (DNS/gateway/MTU) so the
// output matches ipconfig /all detail — Android edition.
DiagnosticResult androidIpConfigurationDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;
    char hostname[256] = {};
    out.append(QString());
    out.append(QStringLiteral("IP Configuration"));
    out.append(QString());
    out.append(QStringLiteral("   Host Name . . . . . . . . . . . . : %1")
        .arg(gethostname(hostname, sizeof(hostname)) == 0 ? QString::fromLatin1(hostname) : QStringLiteral("Unknown")));
    out.append(QString());

    struct IfInfo { QString name; QStringList ips4; QStringList ips6; QStringList masks4; int flags; };
    QMap<QString, IfInfo> ifMap;
    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) == 0) {
        for (auto* p = ifa; p; p = p->ifa_next) {
            if (!p->ifa_addr) continue;
            IfInfo& info = ifMap[QString::fromLatin1(p->ifa_name)];
            info.name = QString::fromLatin1(p->ifa_name);
            info.flags = p->ifa_flags;
            if (p->ifa_addr->sa_family == AF_INET) {
                auto* sa = (struct sockaddr_in*)p->ifa_addr;
                char b[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &sa->sin_addr, b, sizeof(b));
                info.ips4.append(QString::fromLatin1(b));
                if (p->ifa_netmask && p->ifa_netmask->sa_family == AF_INET) {
                    auto* nm = (struct sockaddr_in*)p->ifa_netmask;
                    inet_ntop(AF_INET, &nm->sin_addr, b, sizeof(b));
                    info.masks4.append(QString::fromLatin1(b));
                }
            } else if (p->ifa_addr->sa_family == AF_INET6) {
                auto* sa6 = (struct sockaddr_in6*)p->ifa_addr;
                char b6[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &sa6->sin6_addr, b6, sizeof(b6));
                info.ips6.append(QString::fromLatin1(b6));
            }
        }
        freeifaddrs(ifa);
    }

    for (auto it = ifMap.begin(); it != ifMap.end(); ++it) {
        const auto& info = it.value();
        bool isLoopback = (info.flags & IFF_LOOPBACK);
        QString ifName = info.name;
        out.append(QStringLiteral("%1 adapter %2:")
            .arg(isLoopback ? QStringLiteral("Unknown") : QStringLiteral("Network"), ifName));
        QFile mtuFile(QStringLiteral("/sys/class/net/%1/mtu").arg(ifName));
        if (mtuFile.open(QIODevice::ReadOnly))
            out.append(QStringLiteral("   MTU . . . . . . . . . . . . . . . : %1")
                .arg(QString::fromLatin1(mtuFile.readAll().trimmed())));
        QFile stateFile(QStringLiteral("/sys/class/net/%1/operstate").arg(ifName));
        if (stateFile.open(QIODevice::ReadOnly))
            out.append(QStringLiteral("   Operational State . . . . . . . . : %1")
                .arg(QString::fromLatin1(stateFile.readAll().trimmed()).toUpper()));
        for (int i = 0; i < info.ips4.size(); i++) {
            out.append(QStringLiteral("   IPv4 Address. . . . . . . . . . . : %1 (Preferred)").arg(info.ips4[i]));
            if (i < info.masks4.size())
                out.append(QStringLiteral("   Subnet Mask . . . . . . . . . . . : %1").arg(info.masks4[i]));
        }
        for (const auto& ip6 : info.ips6) {
            if (ip6.startsWith("fe80:"))
                out.append(QStringLiteral("   Link-local IPv6 Address . . . . . : %1").arg(ip6));
            else
                out.append(QStringLiteral("   IPv6 Address. . . . . . . . . . . : %1").arg(ip6));
        }
        out.append(QString());
    }

    // DHCP-assigned values via LinkProperties
    QString gateway;
    for (const auto& rt : androidRoutes())
        if (rt.isDefault && rt.hasGateway) { gateway = rt.gateway; break; }
    if (!gateway.isEmpty())
        out.append(QStringLiteral("   Default Gateway . . . . . . . . . : %1").arg(gateway));
    const QStringList dns = androidDnsServers();
    if (!dns.isEmpty()) {
        out.append(QStringLiteral("   DNS Servers . . . . . . . . . . . : %1").arg(dns.join(QStringLiteral(", "))));
        int mtu = androidLinkMtu();
        if (mtu > 0)
            out.append(QStringLiteral("   Active Interface MTU . . . . . . . : %1").arg(mtu));
    } else {
        out.append(QStringLiteral("   DNS Servers . . . . . . . . . . . : (none reported)"));
    }

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = QStringLiteral("IP Configuration Collected");
    r.durationMs = t.elapsed();
    return r;
}

// ── G1 Active Connections ──────────────────────────────────────────────
// Android 10+ hides the system-wide /proc/net/tcp, but /proc/self/net/tcp
// (this app's own sockets) is readable and still useful.
DiagnosticResult androidActiveConnectionsDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Active TCP Connections (this app)"));
    out.append(QString());
    out.append(QStringLiteral("  Android 10+ hides /proc/net/tcp (system-wide sockets)."));
    out.append(QStringLiteral("  /proc/self/net/tcp lists this app's own sockets:"));
    out.append(QString());

    auto hexIp4 = [](const QString& hp) -> QString {
        int colon = hp.indexOf(':');
        if (colon <= 0) return QString();
        bool ok = false;
        quint32 v = hp.left(colon).toUInt(&ok, 16);
        if (!ok) return QString();
        return QStringLiteral("%1.%2.%3.%4")
            .arg(v & 0xFF).arg((v >> 8) & 0xFF).arg((v >> 16) & 0xFF).arg((v >> 24) & 0xFF);
    };
    auto hexPort = [](const QString& hp) -> int {
        int colon = hp.indexOf(':');
        if (colon < 0) return 0;
        bool ok = false;
        int port = hp.mid(colon + 1).toInt(&ok, 16);
        return ok ? port : 0;
    };
    auto ipv6HexToStr = [](const QString& hp) -> QString {
        // "00000000000000000000000000000000:0000" — 8 groups of 4 hex
        int colon = hp.indexOf(':');
        if (colon != 32) return QString();
        QStringList groups;
        for (int i = 0; i < 8; i++) {
            QString g = hp.mid(i * 4, 4);
            groups << QString::number(g.toUInt(nullptr, 16), 16);
        }
        return groups.join(QStringLiteral(":"));
    };

    int shown = 0;
    const QStringList paths = {QStringLiteral("/proc/self/net/tcp"), QStringLiteral("/proc/self/net/tcp6")};
    for (const auto& path : paths) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) continue;
        QTextStream ts(&f);
        ts.readLine(); // header
        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            if (line.isEmpty()) continue;
            QStringList cols = line.split(QRegularExpression("\\s+"));
            if (cols.size() < 4) continue;
            int state = cols[3].toInt(nullptr, 16);
            QString local = cols[1], rem = cols[2];
            bool isV6 = path.contains("tcp6");
            QString lip = isV6 ? ipv6HexToStr(local) : hexIp4(local);
            QString rip = isV6 ? ipv6HexToStr(rem) : hexIp4(rem);
            if (lip.isEmpty()) lip = QStringLiteral("?");
            if (rip.isEmpty()) rip = QStringLiteral("*");
            out.append(QStringLiteral("  %1:%2 → %3:%4  %5")
                .arg(lip).arg(hexPort(local))
                .arg(rip).arg(hexPort(rem))
                .arg(QString::fromLatin1(androidTcpStateName(state))));
            shown++;
        }
    }
    if (shown == 0)
        out.append(QStringLiteral("  (no readable connections)"));
    out.append(QString());

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.summary = shown > 0 ? QStringLiteral("%1 active connection%2").arg(shown).arg(shown > 1 ? "s" : "")
                          : QStringLiteral("No readable connections");
    r.status = DiagStatus::Info;
    return r;
}

// ═══════════════════════════════════════════════════════════════════════
// G3: Internet & DNS (Android-native implementations)
// ═══════════════════════════════════════════════════════════════════════

// ── G3 DNS Servers ─────────────────────────────────────────────────────
// 5WHY: /etc/resolv.conf is empty on Android; ConnectivityManager
// getLinkProperties().getDnsServers() is the official source — this test
// was previously skipped entirely even though the data is public.
DiagnosticResult androidDnsServersDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out, dnsList;
    out.append(QString());
    out.append(QStringLiteral("DNS Server Configuration (table mode):"));
    out.append(QString());

    static const QVector<DiagnosticFormatter::ColSpec> kDnsCols = {
        {"Source", 20, false},
        {"DNS Server", 0, false},
    };
    QList<QStringList> dnsRows;
    const QStringList dns = androidDnsServers();
    for (const auto& ns : dns) {
        dnsRows.append({QStringLiteral("ConnectivityManager"), ns});
        if (!dnsList.contains(ns)) dnsList.append(ns);
    }
    // Also attempt /etc/resolv.conf (empty on most Android builds)
    QFile resolv(QStringLiteral("/etc/resolv.conf"));
    if (resolv.open(QIODevice::ReadOnly)) {
        QTextStream ts(&resolv);
        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            if (line.startsWith("nameserver ")) {
                QString ns = line.mid(11);
                dnsRows.append({QStringLiteral("resolv.conf"), ns});
                if (!dnsList.contains(ns)) dnsList.append(ns);
            }
        }
    }
    // Private DNS (DoT) state
    AndroidCaps caps = androidCapabilities();
    if (caps.available)
        dnsRows.append({QStringLiteral("Private DNS (DoT)"), caps.privateDns ? QStringLiteral("active") : QStringLiteral("off")});

    if (!dnsRows.isEmpty())
        out << DiagnosticFormatter::formatTable(kDnsCols, dnsRows);
    if (dnsRows.isEmpty()) {
        out.append(QStringLiteral("  No DNS servers reported by the system"));
        r.status = DiagStatus::Warning;
        r.summary = QStringLiteral("No DNS servers found");
    } else {
        r.status = DiagStatus::Pass;
        r.summary = QStringLiteral("DNS: %1").arg(dnsList.join(QStringLiteral(", ")));
    }
    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    return r;
}

// ── G3 DNS Cache ───────────────────────────────────────────────────────
// Android has no public resolver-cache API.  A cold-vs-warm double lookup
// timing probe infers cache state from the latency delta.
DiagnosticResult androidDnsCacheDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("DNS Resolver Cache (warm/cold probe)"));
    out.append(QString());
    out.append(QStringLiteral("  Android does not expose the system resolver cache directly."));
    out.append(QStringLiteral("  A cold-vs-warm double-resolution timing probe infers cache state:"));
    out.append(QString());

    static const char* kDomains[] = {
        "www.google.com", "www.baidu.com", "www.cloudflare.com", "www.wikipedia.org",
    };
    int hits = 0;
    for (const char* d : kDomains) {
        QString host = QString::fromLatin1(d);
        QElapsedTimer t; t.start();
        QString ip1 = androidResolveOne(host, 3000);
        int firstMs = (int)t.elapsed();
        t.restart();
        QString ip2 = androidResolveOne(host, 3000);
        int secondMs = (int)t.elapsed();
        bool cached = !ip2.isEmpty() && ip2 == ip1 && secondMs < firstMs && firstMs > 0;
        if (cached) hits++;
        QString ip = ip2.isEmpty() ? ip1 : ip2;
        out.append(QStringLiteral("  %1: cold=%2ms, warm=%3ms → %4")
            .arg(host).arg(firstMs).arg(secondMs)
            .arg(cached ? QStringLiteral("cache hit")
                        : (ip.isEmpty() ? QStringLiteral("unresolved") : QStringLiteral("uncached"))));
    }
    out.append(QString());
    if (hits > 0) {
        out.append(QStringLiteral("  %1/%2 domains served from the resolver cache.").arg(hits).arg(4));
        r.summary = QStringLiteral("DNS cache active (%1/%2 warm)").arg(hits).arg(4);
        r.status = DiagStatus::Pass;
    } else {
        out.append(QStringLiteral("  No cache hits observed (system cache may be disabled or every lookup was cold)."));
        r.summary = QStringLiteral("No DNS cache hits");
        r.status = DiagStatus::Info;
    }
    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    return r;
}

#endif // PLATFORM_ANDROID
