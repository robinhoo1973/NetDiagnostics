// =============================================================================
// AndroidNetworkInfo.cpp — Native Android WiFi/Cellular diagnostics via JNI
//
// Uses Qt's QJniObject to call Android Java APIs directly from C++.
// No separate Java/Kotlin source files needed — all JNI is inline.
// =============================================================================
#ifdef PLATFORM_ANDROID

#include <QJniObject>
#include <QJniEnvironment>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QDateTime>
#include <QElapsedTimer>
#include <QUrl>
#include "models/DiagnosticResult.h"
#include "models/DiagId.h"
#include "util/DiagnosticFormatter.h"

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

// ── WiFi SSID via WifiManager ──────────────────────────────────────────
static QString androidWifiSsid() {
    // Requires ACCESS_WIFI_STATE + ACCESS_FINE_LOCATION permissions
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
    // Android returns SSID wrapped in quotes — strip them
    if (result.startsWith('"') && result.endsWith('"'))
        result = result.mid(1, result.length() - 2);
    return result;
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
    QJniObject hostStr = QJniObject::fromString(host);
    QJniObject inetAddr = QJniObject::callStaticObjectMethod(
        "java/net/InetAddress", "getByName",
        "(Ljava/lang/String;)Ljava/net/InetAddress;",
        hostStr.object<jstring>());
    if (!inetAddr.isValid()) return QString();
    QJniObject ipStr = inetAddr.callObjectMethod("getHostAddress", "()Ljava/lang/String;");
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
                jint certCount = QJniEnvironment().callMethod<jint>(certArray.object(), "length", "()I");
                QString certInfo = QStringLiteral("Server certificates: %1\n").arg(certCount);
                for (jint i = 0; i < certCount && i < 3; i++) {
                    QJniObject cert = QJniEnvironment().callObjectMethod(certArray.object(),
                        "[Ljava/lang/Object;", "(I)Ljava/lang/Object;", i);
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

#endif // PLATFORM_ANDROID
