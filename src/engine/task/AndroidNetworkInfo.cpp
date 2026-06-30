// =============================================================================
// AndroidNetworkInfo.cpp — Native Android WiFi/Cellular diagnostics via JNI
//
// Uses Qt's QAndroidJniObject to call Android Java APIs directly from C++.
// No separate Java/Kotlin source files needed — all JNI is inline.
// =============================================================================
#ifdef PLATFORM_ANDROID

#include <QAndroidJniObject>
#include <QAndroidJniEnvironment>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QDateTime>
#include "models/DiagnosticResult.h"
#include "models/DiagId.h"
#include "util/DiagnosticFormatter.h"

// ── WiFi SSID via WifiManager ──────────────────────────────────────────
static QString androidWifiSsid() {
    // Requires ACCESS_WIFI_STATE + ACCESS_FINE_LOCATION permissions
    QAndroidJniObject ctx = QAndroidJniObject::callStaticObjectMethod(
        "org/qtproject/qt5/android/QtNative", "activity",
        "()Landroid/app/Activity;");
    if (!ctx.isValid()) return QString();

    QAndroidJniObject wifiService = ctx.callObjectMethod(
        "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;",
        QAndroidJniObject::fromString("wifi").object<jstring>());
    if (!wifiService.isValid()) return QStringLiteral("WifiManager unavailable");

    QAndroidJniObject wifiInfo = wifiService.callObjectMethod(
        "getConnectionInfo", "()Landroid/net/wifi/WifiInfo;");
    if (!wifiInfo.isValid()) return QStringLiteral("No WiFi connection");

    QAndroidJniObject ssid = wifiInfo.callObjectMethod("getSSID", "()Ljava/lang/String;");
    if (!ssid.isValid()) return QStringLiteral("SSID unavailable");

    QString result = ssid.toString();
    // Android returns SSID wrapped in quotes — strip them
    if (result.startsWith('"') && result.endsWith('"'))
        result = result.mid(1, result.length() - 2);
    return result;
}

// ── Cellular Carrier via TelephonyManager ──────────────────────────────
static QVariantMap androidCellularInfo() {
    QVariantMap info;

    QAndroidJniObject ctx = QAndroidJniObject::callStaticObjectMethod(
        "org/qtproject/qt5/android/QtNative", "activity",
        "()Landroid/app/Activity;");
    if (!ctx.isValid()) return info;

    QAndroidJniObject telService = ctx.callObjectMethod(
        "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;",
        QAndroidJniObject::fromString("phone").object<jstring>());
    if (!telService.isValid()) return info;

    // Carrier name
    QAndroidJniObject carrierName = telService.callObjectMethod(
        "getNetworkOperatorName", "()Ljava/lang/String;");
    if (carrierName.isValid())
        info["carrierName"] = carrierName.toString();

    // Network type (4G/5G/etc.)
    QAndroidJniObject networkType = telService.callObjectMethod(
        "getNetworkTypeName", "()Ljava/lang/String;");
    if (networkType.isValid())
        info["radioAccess"] = networkType.toString();

    // MCC/MNC
    QAndroidJniObject networkOperator = telService.callObjectMethod(
        "getNetworkOperator", "()Ljava/lang/String;");
    if (networkOperator.isValid()) {
        QString op = networkOperator.toString();
        if (op.length() >= 5) {
            info["mcc"] = op.left(3);
            info["mnc"] = op.mid(3);
        }
    }

    return info;
}

// ── Connectivity info via ConnectivityManager ──────────────────────────
static QString androidConnectivityInfo() {
    QAndroidJniObject ctx = QAndroidJniObject::callStaticObjectMethod(
        "org/qtproject/qt5/android/QtNative", "activity",
        "()Landroid/app/Activity;");
    if (!ctx.isValid()) return QStringLiteral("Activity unavailable");

    QAndroidJniObject connService = ctx.callObjectMethod(
        "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;",
        QAndroidJniObject::fromString("connectivity").object<jstring>());
    if (!connService.isValid()) return QStringLiteral("ConnectivityManager unavailable");

    QAndroidJniObject activeNetwork = connService.callObjectMethod(
        "getActiveNetworkInfo", "()Landroid/net/NetworkInfo;");
    if (!activeNetwork.isValid()) return QStringLiteral("No active network");

    QAndroidJniObject typeName = activeNetwork.callObjectMethod(
        "getTypeName", "()Ljava/lang/String;");
    QAndroidJniObject subtypeName = activeNetwork.callObjectMethod(
        "getSubtypeName", "()Ljava/lang/String;");

    QString result = typeName.isValid() ? typeName.toString() : QStringLiteral("Unknown");
    if (subtypeName.isValid())
        result += QStringLiteral(" (%1)").arg(subtypeName.toString());
    return result;
}

// ── Public diagnostic entry points ─────────────────────────────────────

QString androidNetworkTypeInfo() {
    return androidConnectivityInfo();
}

DiagnosticResult androidWifiDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();

    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Wireless LAN information:"));
    out.append(QString());
    QString ssid = androidWifiSsid();
    out.append(QStringLiteral("  SSID: %1").arg(ssid.isEmpty() ? QStringLiteral("(unavailable)") : ssid));
    out.append(QStringLiteral("  [Android] Signal/BSSID/Channel: requires ACCESS_FINE_LOCATION + WifiManager.calculateSignalLevel"));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.summary = ssid.isEmpty() ? QStringLiteral("No WiFi") : QStringLiteral("WiFi: %1").arg(ssid);
    r.status = ssid.isEmpty() ? DiagStatus::Info : DiagStatus::Pass;
    return r;
}

DiagnosticResult androidCellularDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();

    QVariantMap cell = androidCellularInfo();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Cellular Information:"));
    out.append(QString());

    if (!cell.isEmpty()) {
        if (cell.contains("carrierName"))
            out.append(QStringLiteral("  Carrier: %1").arg(cell["carrierName"].toString()));
        if (cell.contains("radioAccess"))
            out.append(QStringLiteral("  Radio Access: %1").arg(cell["radioAccess"].toString()));
        if (cell.contains("mcc") && cell.contains("mnc"))
            out.append(QStringLiteral("  MCC/MNC: %1-%2").arg(cell["mcc"].toString(), cell["mnc"].toString()));
        r.status = DiagStatus::Pass;
        r.summary = QStringLiteral("Carrier: %1").arg(cell.value("carrierName").toString());
    } else {
        out.append(QStringLiteral("  No cellular service available"));
        r.status = DiagStatus::Info;
        r.summary = QStringLiteral("No cellular service");
    }

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    return r;
}

DiagnosticResult androidDhcpDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    r.rawOutput = QStringLiteral("\nDHCP Client Status:\n\n  [Android] DHCP is system-managed. Lease details unavailable.\n  Use adb shell dumpsys dhcpclient for lease info.\n");
    r.details = r.rawOutput;
    r.summary = QStringLiteral("System-managed (Android)");
    r.status = DiagStatus::Info;
    return r;
}

DiagnosticResult androidGatewayDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();
    r.rawOutput = QStringLiteral("\nDefault Gateway:\n\n  [Android] Default gateway: use ConnectivityManager.getLinkProperties() via JNI.\n  Routing table inaccessible (requires root or adb shell ip route).\n");
    r.details = r.rawOutput;
    r.summary = QStringLiteral("System-managed (Android)");
    r.status = DiagStatus::Info;
    return r;
}

// ── DNS Resolution via InetAddress ─────────────────────────────────────
QString androidDnsResolve(const QString& host, int timeoutMs) {
    QAndroidJniObject hostStr = QAndroidJniObject::fromString(host);
    QAndroidJniObject inetAddr = QAndroidJniObject::callStaticObjectMethod(
        "java/net/InetAddress", "getByName",
        "(Ljava/lang/String;)Ljava/net/InetAddress;",
        hostStr.object<jstring>());
    if (!inetAddr.isValid()) return QString();
    QAndroidJniObject ipStr = inetAddr.callObjectMethod("getHostAddress", "()Ljava/lang/String;");
    return ipStr.isValid() ? ipStr.toString() : QString();
}

DiagnosticResult androidDnsDiag(DiagId id, const QString& target) {
    DiagnosticResult r; r.id = id; r.group = diagGroup(id);
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();

    QString host = target;
    if (host.contains("://")) { QUrl u(host); host = u.host(); }
    QString ip = androidDnsResolve(host, 3000);
    qint64 elapsed = t.elapsed();
    r.durationMs = elapsed;

    // Dig-style output via shared DiagnosticFormatter
    QStringList out;
    out << DiagnosticFormatter::formatDnsHeader(host,
        ip.isEmpty() ? "SERVFAIL" : "NOERROR",
        (uint16_t)(qHash(host) & 0xFFFF), ip.isEmpty() ? 0 : 1);
    out.append(QStringLiteral(";; QUESTION SECTION:"));
    out.append(DiagnosticFormatter::formatDnsQuestion(host));
    out.append(QString());
    if (!ip.isEmpty()) {
        out.append(QStringLiteral(";; ANSWER SECTION:"));
        out.append(DiagnosticFormatter::formatDnsRecord(host, 0, "A", ip));
        out.append(QString());
    }
    out << DiagnosticFormatter::formatDnsFooter(elapsed, "system resolver (InetAddress)");
    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = ip.isEmpty() ? DiagStatus::Fail : DiagStatus::Pass;
    r.summary = ip.isEmpty() ? QStringLiteral("DNS failed") : QStringLiteral("Resolved: %1").arg(ip);
    return r;
}

// ── HTTP Diagnostics via HttpURLConnection ─────────────────────────────
DiagnosticResult androidHttpDiag(DiagId id, const QString& target) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G5;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();

    QAndroidJniObject urlStr = QAndroidJniObject::fromString(target);
    QAndroidJniObject url = QAndroidJniObject("java/net/URL", "(Ljava/lang/String;)V", urlStr.object<jstring>());
    if (!url.isValid()) {
        r.status = DiagStatus::Fail; r.summary = QStringLiteral("Invalid URL"); return r;
    }

    QAndroidJniObject conn = url.callObjectMethod("openConnection", "()Ljava/net/URLConnection;");
    if (!conn.isValid()) {
        r.status = DiagStatus::Fail; r.summary = QStringLiteral("Connection failed"); return r;
    }

    // Cast to HttpURLConnection
    QAndroidJniObject httpConn = conn; // implicit — openConnection returns the right type
    httpConn.callMethod<void>("setConnectTimeout", "(I)V", 10000);
    httpConn.callMethod<void>("setReadTimeout", "(I)V", 15000);
    httpConn.callMethod<void>("setRequestMethod", "(Ljava/lang/String;)V",
        QAndroidJniObject::fromString("GET").object<jstring>());

    int responseCode = httpConn.callMethod<jint>("getResponseCode");
    r.durationMs = t.elapsed();

    QAndroidJniObject msg = httpConn.callObjectMethod("getResponseMessage", "()Ljava/lang/String;");
    QString statusLine = QStringLiteral("HTTP/1.1 %1 %2").arg(responseCode)
        .arg(msg.isValid() ? msg.toString() : QString());

    r.rawOutput = statusLine;
    r.details = statusLine;
    r.status = (responseCode >= 200 && responseCode < 400) ? DiagStatus::Pass : DiagStatus::Warning;
    r.summary = QStringLiteral("HTTP %1 (%2ms)").arg(responseCode).arg(r.durationMs);
    return r;
}

#endif // PLATFORM_ANDROID
