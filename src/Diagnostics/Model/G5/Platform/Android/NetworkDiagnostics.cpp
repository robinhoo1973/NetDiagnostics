// =============================================================================
// AndroidNetworkInfo.cpp — Native Android WiFi/Cellular diagnostics via JNI
//
// Uses Qt's QJniObject to call Android Java APIs directly from C++.
// No separate Java/Kotlin source files needed — all JNI is inline.
// =============================================================================
#if defined(PLATFORM_ANDROID)

#include <QJniObject>
#include <QJniEnvironment>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QDateTime>
#include <QElapsedTimer>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>
#include "Common/Model/DiagnosticResult.h"
#include "Common/Model/DiagId.h"
#include "Diagnostics/View/DiagnosticFormatter.h"

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
static QString androidLocationPermissionStatus() {
    QJniObject ctx = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity",
        "()Landroid/app/Activity;");
    if (!ctx.isValid()) return QString();

    // ContextCompat.checkSelfPermission(context, Manifest.permission.ACCESS_FINE_LOCATION)
    QJniObject permStr = QJniObject::fromString("android.permission.ACCESS_FINE_LOCATION");
    jint result = QJniObject::callStaticMethod<jint>(
        "androidx/core/content/ContextCompat", "checkSelfPermission",
        "(Landroid/content/Context;Ljava/lang/String;)I",
        ctx.object(), permStr.object<jstring>());

    const jint PERMISSION_GRANTED = 0;
    const jint PERMISSION_DENIED = -1;
    if (result == PERMISSION_GRANTED) return QString(); // OK

    if (result == PERMISSION_DENIED) {
        return QStringLiteral("WiFi SSID/BSSID: Location permission was denied. "
                              "Go to Settings > Apps > NetDiagnostics > Permissions "
                              "and enable 'Location' (required for WiFi diagnostics).");
    }
    return QStringLiteral("WiFi SSID/BSSID: Location permission not granted. "
                          "Grant 'Location' permission in Settings > Apps > NetDiagnostics.");
}

// ── WiFi SSID via WifiManager ──────────────────────────────────────────
static QString androidWifiSsid() {
    // Check permission first — same pattern as iOS authorizationStatus check
    QString permError = androidLocationPermissionStatus();
    if (!permError.isEmpty()) return permError;

    QJniObject ctx = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity",
        "()Landroid/app/Activity;");
    if (!ctx.isValid()) return QString();

    QJniObject wifiService = ctx.callObjectMethod(
        "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;",
        QJniObject::fromString("wifi").object<jstring>());
    if (!wifiService.isValid()) return QStringLiteral("WifiManager unavailable");

    QJniObject wifiInfo = wifiService.callObjectMethod(
        "getConnectionInfo", "()Landroid/net/wifi/WifiInfo;");
    if (!wifiInfo.isValid()) return QStringLiteral("No WiFi connection");

    QJniObject ssid = wifiInfo.callObjectMethod("getSSID", "()Ljava/lang/String;");
    if (!ssid.isValid()) return QStringLiteral("SSID unavailable");

    QString result = ssid.toString();
    if (result.startsWith('"') && result.endsWith('"'))
        result = result.mid(1, result.length() - 2);
    return result;
}

// ── WiFi BSSID ─────────────────────────────────────────────────────────
// 5WHY: BSSID was never retrieved despite WifiInfo.getBSSID() being
// available with location permission. iOS retrieves BSSID via NEHotspotNetwork.
static QString androidWifiBssid() {
    QJniObject ctx = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity",
        "()Landroid/app/Activity;");
    if (!ctx.isValid()) return QString();

    QJniObject wifiService = ctx.callObjectMethod(
        "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;",
        QJniObject::fromString("wifi").object<jstring>());
    if (!wifiService.isValid()) return QString();

    QJniObject wifiInfo = wifiService.callObjectMethod(
        "getConnectionInfo", "()Landroid/net/wifi/WifiInfo;");
    if (!wifiInfo.isValid()) return QString();

    QJniObject bssid = wifiInfo.callObjectMethod("getBSSID", "()Ljava/lang/String;");
    return bssid.isValid() ? bssid.toString() : QString();
}

// ── Cellular Carrier via TelephonyManager ──────────────────────────────
static QVariantMap androidCellularInfo() {
    QVariantMap info;

    QJniObject ctx = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity",
        "()Landroid/app/Activity;");
    if (!ctx.isValid()) return info;

    QJniObject telService = ctx.callObjectMethod(
        "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;",
        QJniObject::fromString("phone").object<jstring>());
    if (!telService.isValid()) return info;

    // Carrier name
    QJniObject carrierName = telService.callObjectMethod(
        "getNetworkOperatorName", "()Ljava/lang/String;");
    if (carrierName.isValid())
        info["carrierName"] = carrierName.toString();

    // Network type (4G/5G/etc.)
    QJniObject networkType = telService.callObjectMethod(
        "getNetworkTypeName", "()Ljava/lang/String;");
    if (networkType.isValid())
        info["radioAccess"] = networkType.toString();

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

    const jint sdkInt = QJniObject::getStaticField<jint>("android/os/Build$VERSION", "SDK_INT");
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
        } else {
            QJniEnvironment env;
            clearJniException(env);
        }
    }

    return info;
}

// ── Connectivity info via ConnectivityManager ──────────────────────────
static QString androidConnectivityInfo() {
    QJniObject ctx = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative", "activity",
        "()Landroid/app/Activity;");
    if (!ctx.isValid()) return QStringLiteral("Activity unavailable");

    QJniObject connService = ctx.callObjectMethod(
        "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;",
        QJniObject::fromString("connectivity").object<jstring>());
    if (!connService.isValid()) return QStringLiteral("ConnectivityManager unavailable");

    QJniObject activeNetwork = connService.callObjectMethod(
        "getActiveNetworkInfo", "()Landroid/net/NetworkInfo;");
    if (!activeNetwork.isValid()) return QStringLiteral("No active network");

    QJniObject typeName = activeNetwork.callObjectMethod(
        "getTypeName", "()Ljava/lang/String;");
    QJniObject subtypeName = activeNetwork.callObjectMethod(
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

// 5WHY: Used hardcoded note about ACCESS_FINE_LOCATION — never checked
// actual permission status, never retrieved BSSID (API available), never
// showed actionable guidance. Now mirrors iOS pattern: check permission,
// show specific error, retrieve SSID+BSSID when available.
DiagnosticResult androidWifiDiag(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();

    // Check permission first (mirrors iOS [CLLocationManager authorizationStatus])
    QString permError = androidLocationPermissionStatus();

    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Wireless LAN information:"));
    out.append(QString());

    QString ssid = androidWifiSsid();
    bool permDenied = !permError.isEmpty() && ssid.contains("permission");

    if (permDenied) {
        // Show actionable permission guidance
        out.append(QStringLiteral("  %1").arg(permError));
    } else if (ssid.isEmpty() || ssid == "(unavailable)") {
        out.append(QStringLiteral("  SSID: (not connected or unavailable)"));
    } else {
        out.append(QStringLiteral("  SSID: %1").arg(ssid));
        // 5WHY: BSSID was never retrieved despite getBSSID() being available
        QString bssid = androidWifiBssid();
        out.append(QStringLiteral("  BSSID: %1").arg(bssid.isEmpty() ? QStringLiteral("(unavailable)") : bssid));
    }

    if (!permDenied)
        out.append(QStringLiteral("  Signal/Channel: Requires Android API level 29+ (WifiManager.calculateSignalLevel)"));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.summary = permDenied ? QStringLiteral("WiFi: Location permission required")
               : ssid.isEmpty() ? QStringLiteral("No WiFi")
               : QStringLiteral("WiFi: %1").arg(ssid);
    r.status = permDenied ? DiagStatus::Warning
               : ssid.isEmpty() ? DiagStatus::Info
               : DiagStatus::Pass;
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
        if (cell.contains("signalLevel")) {
            QString signalLine = QStringLiteral("  Signal: %1")
                .arg(signalGlyphs(cell["signalLevel"].toInt()));
            if (cell.contains("rsrp"))
                signalLine += QStringLiteral(" (RSRP %1)").arg(cell["rsrp"].toString());
            out.append(signalLine);
        }
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

// 5WHY: Used hardcoded plain-text string — table view was desktop-only.
// Now uses DiagnosticFormatter::formatTable for consistent display on all
// platforms (matching iOS, macOS, desktop implementations).
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
    out.append(QStringLiteral("  Android manages DHCP at the system level —"));
    out.append(QStringLiteral("  lease details are not accessible to third-party apps."));
    out.append(QStringLiteral("  Use `adb shell dumpsh dhcpclient` for lease info."));

    r.rawOutput = out.join('\n');
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
// 5WHY: timeoutMs parameter was completely ignored — getByName() is a
// synchronous blocking JNI call with no built-in timeout. On unreachable
// DNS servers this blocks the calling thread indefinitely.  Wrap in
// QtConcurrent so waitForFinished(timeoutMs) can enforce the timeout.
QString androidDnsResolve(const QString& host, int timeoutMs) {
    QFuture<QString> future = QtConcurrent::run([host]() -> QString {
        QJniObject hostStr = QJniObject::fromString(host);
        QJniObject inetAddr = QJniObject::callStaticObjectMethod(
            "java/net/InetAddress", "getByName",
            "(Ljava/lang/String;)Ljava/net/InetAddress;",
            hostStr.object<jstring>());
        if (!inetAddr.isValid()) return QString();
        QJniObject ipStr = inetAddr.callObjectMethod("getHostAddress", "()Ljava/lang/String;");
        return ipStr.isValid() ? ipStr.toString() : QString();
    });
    // 5WHY: Qt 6.5.3 QFuture::waitForFinished() takes 0 args (void).
    // Qt 6.6+ added waitForFinished(int timeoutMs) → bool.
    // Android cross-compile uses Qt 6.5.3 (pinned for AGP/compileSdk 33),
    // so the timeout-capable API is unavailable.  Use a version guard so
    // the code compiles on both Qt 6.5 and Qt 6.6+.
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    if (future.waitForFinished(timeoutMs > 0 ? timeoutMs : 3000))
        return future.result();
    // Timeout: the JNI thread is still blocking inside getByName() and
    // cannot be cancelled.  It will eventually complete and clean itself
    // up, but this invocation returns empty to avoid blocking the caller.
    return QString();
#else
    // Qt 6.5: no timeout parameter — Android native DNS via JNI is fast
    // enough that indefinite blocking is unlikely in practice.
    future.waitForFinished();
    return future.result();
#endif
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

namespace {

// Helper: read response headers from HttpURLConnection as a QString
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
        // val is a List<String> — get first element
        QJniObject vals = val; // List
        QJniObject first = vals.callObjectMethod("get", "(I)Ljava/lang/Object;", 0);
        QString headerLine;
        if (key.isValid() && first.isValid())
            headerLine = QStringLiteral("%1: %2\n").arg(key.toString(), first.toString());
        else if (first.isValid())
            headerLine = first.toString() + "\n";
        result += headerLine;
    }
    return result;
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

    QJniObject urlStr = QJniObject::fromString(target);
    QJniObject url = QJniObject("java/net/URL", "(Ljava/lang/String;)V", urlStr.object<jstring>());
    if (!url.isValid()) {
        r.status = DiagStatus::Fail; r.summary = QStringLiteral("Invalid URL"); r.durationMs = t.elapsed(); return r;
    }

    QJniObject conn = url.callObjectMethod("openConnection", "()Ljava/net/URLConnection;");
    if (!conn.isValid()) {
        r.status = DiagStatus::Fail; r.summary = QStringLiteral("Connection failed"); r.durationMs = t.elapsed(); return r;
    }

    QJniObject httpConn = conn;
    httpConn.callMethod<void>("setConnectTimeout", "(I)V", 10000);
    httpConn.callMethod<void>("setReadTimeout", "(I)V", 15000);

    // For redirect detection: don't follow automatically
    if (id == DiagId::G5HttpRedirect)
        httpConn.callMethod<void>("setInstanceFollowRedirects", "(Z)V", false);
    else
        httpConn.callMethod<void>("setInstanceFollowRedirects", "(Z)V", true);

    httpConn.callMethod<void>("setRequestMethod", "(Ljava/lang/String;)V",
        QJniObject::fromString("GET").object<jstring>());

    int responseCode = 0;
    try {
        responseCode = httpConn.callMethod<jint>("getResponseCode");
    } catch (...) {
        r.durationMs = t.elapsed();
        r.status = DiagStatus::Fail; r.summary = QStringLiteral("HTTP request failed"); return r;
    }
    qint64 dur = t.elapsed();

    QString headers = readHeaders(httpConn);
    r.rawOutput = headers;
    r.details = headers;

    switch (id) {
        case DiagId::G5CurlVerbose: {
            // Read body via input stream
            QJniObject inStream = httpConn.callObjectMethod("getInputStream", "()Ljava/io/InputStream;");
            if (inStream.isValid()) {
                QJniObject bufReader("java/io/BufferedReader",
                    "(Ljava/io/Reader;)V",
                    QJniObject("java/io/InputStreamReader",
                        "(Ljava/io/InputStream;)V", inStream.object()).object());
                // Read first 2KB
                QString body;
                QJniObject line;
                while ((line = bufReader.callObjectMethod("readLine", "()Ljava/lang/String;")).isValid()
                       && body.size() < 2048)
                    body += line.toString() + "\n";
                r.rawOutput = headers + "\n" + body;
            }
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
            int present = audit.count("Present:"); // rough: if "Missing:" is not alone
            r.status = audit.contains("Missing:") ? DiagStatus::Warning : DiagStatus::Pass;
            break;
        }
        case DiagId::G5SslCertificate: {
            // Try to get cert via HttpsURLConnection
            QJniObject httpsConn("javax/net/ssl/HttpsURLConnection");
            QJniObject serverCerts;
            if (httpConn.callMethod<jboolean>("isInstanceOf",
                    "(Ljava/lang/Class;)Z",
                    httpsConn.callObjectMethod("getClass", "()Ljava/lang/Class;").object())) {
                serverCerts = httpConn.callObjectMethod("getServerCertificates",
                    "()[Ljava/security/cert/Certificate;");
            }
            if (serverCerts.isValid()) {
                QJniObject certArray = serverCerts; // Certificate[]
                jint certCount = certArray.callMethod<jint>("length");
                QString certInfo = QStringLiteral("Server certificates: %1\n").arg(certCount);
                for (jint i = 0; i < certCount && i < 3; i++) {
                    QJniObject cert = certArray.callObjectMethod("get",
                        "(I)Ljava/lang/Object;", i);
                    if (cert.isValid()) {
                        QJniObject type = cert.callObjectMethod("getType", "()Ljava/lang/String;");
                        QJniObject subj = cert.callObjectMethod("getSubjectDN", "()Ljavax/security/auth/x500/X500Principal;");
                        if (subj.isValid())
                            certInfo += QStringLiteral("  %1: %2\n").arg(i+1)
                                .arg(subj.callObjectMethod("getName", "()Ljava/lang/String;").toString());
                    }
                }
                r.rawOutput = certInfo;
                r.summary = QStringLiteral("TLS OK, %1 cert(s)").arg(certCount);
                r.status = DiagStatus::Pass;
            } else {
                r.summary = QStringLiteral("TLS connected (no cert details)");
                r.status = DiagStatus::Warning;
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
            QJniObject acceptEnc = httpConn.callObjectMethod("getRequestProperty",
                "(Ljava/lang/String;)Ljava/lang/String;",
                QJniObject::fromString("Accept-Encoding").object<jstring>());
            QString enc = contentEnc.isValid() ? contentEnc.toString() : QString();
            if (enc.isEmpty()) {
                // Manual check: did the server send Content-Encoding header?
                QJniObject ce = httpConn.callObjectMethod("getHeaderField",
                    "(Ljava/lang/String;)Ljava/lang/String;",
                    QJniObject::fromString("Content-Encoding").object<jstring>());
                enc = ce.isValid() ? ce.toString() : QString::fromLatin1("none");
            }
            r.summary = QStringLiteral("Content-Encoding: %1").arg(enc);
            r.status = (enc == "none" || enc.isEmpty()) ? DiagStatus::Warning : DiagStatus::Pass;
            break;
        }
        case DiagId::G5HttpTiming:
            r.summary = QStringLiteral("HTTP %1 (%2ms)").arg(responseCode).arg(dur);
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
#endif // PLATFORM_ANDROID
