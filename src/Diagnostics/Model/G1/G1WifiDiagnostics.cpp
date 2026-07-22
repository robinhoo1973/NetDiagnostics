#include "Diagnostics/Model/GBase.h"
#include "Diagnostics/Model/GHelpers.h"
#if defined(__APPLE__) && !defined(PLATFORM_IOS)
#include "Common/Platform/Apple/macOS/WifiHelper.h"
#endif
namespace SystemDiagnostics {
DiagnosticResult wifiDiagnostics(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

    out.append(QString());
    out.append(QStringLiteral("Wireless LAN information:"));
    out.append(QString());

#if defined(_WIN32)
    HANDLE hClient = nullptr;
    DWORD negotiatedVer = 0;
    if (WlanOpenHandle(2, nullptr, &negotiatedVer, &hClient) == ERROR_SUCCESS) {
        PWLAN_INTERFACE_INFO_LIST ifList = nullptr;
        if (WlanEnumInterfaces(hClient, nullptr, &ifList) == ERROR_SUCCESS) {
            for (DWORD i = 0; i < ifList->dwNumberOfItems; i++) {
                auto& wi = ifList->InterfaceInfo[i];
                out.append(QStringLiteral("   Name . . . . . . . . . . . . : %1").arg(QString::fromWCharArray(wi.strInterfaceDescription)));
                {
                    wchar_t guidStr[40] = {};
                    StringFromGUID2(wi.InterfaceGuid, guidStr, 40);
                    out.append(QStringLiteral("   GUID . . . . . . . . . . . . : %1").arg(QString::fromWCharArray(guidStr)));
                }
                out.append(QStringLiteral("   State. . . . . . . . . . . . : %1").arg(wi.isState == wlan_interface_state_connected ? "connected" : "disconnected"));

                // ── Query extended WiFi details for connected interfaces ──
                if (wi.isState == wlan_interface_state_connected) {
                    DWORD dataSize = 0;
                    PWLAN_CONNECTION_ATTRIBUTES pConn = nullptr;
                    if (WlanQueryInterface(hClient, &wi.InterfaceGuid, wlan_intf_opcode_current_connection,
                                          nullptr, &dataSize, (PVOID*)&pConn, nullptr) == ERROR_SUCCESS && pConn) {
                        // SSID
                        QString ssid = QString::fromUtf8((const char*)pConn->wlanAssociationAttributes.dot11Ssid.ucSSID,
                                                         pConn->wlanAssociationAttributes.dot11Ssid.uSSIDLength);
                        out.append(QStringLiteral("   SSID. . . . . . . . . . . . . : %1").arg(ssid));
                        // BSSID (MAC)
                        auto& bssid = pConn->wlanAssociationAttributes.dot11Bssid;
                        out.append(QStringLiteral("   BSSID . . . . . . . . . . . . : %1").arg(
                            macToStr((const unsigned char*)bssid)));
                        // Channel — query via WlanQueryInterface opcode
                        ULONG channel = 0;
                        DWORD chSize = sizeof(channel);
                        if (WlanQueryInterface(hClient, &wi.InterfaceGuid, wlan_intf_opcode_channel_number,
                                              nullptr, &chSize, (PVOID*)&channel, nullptr) == ERROR_SUCCESS
                            && channel > 0) {
                            out.append(QStringLiteral("   Channel. . . . . . . . . . . : %1").arg(channel));
                        } else {
                            out.append(QStringLiteral("   Channel. . . . . . . . . . . : N/A"));
                        }
                        // Security
                        QString auth = QStringLiteral("Unknown");
                        switch (pConn->wlanSecurityAttributes.dot11AuthAlgorithm) {
                            case DOT11_AUTH_ALGO_80211_OPEN:    auth = QStringLiteral("Open"); break;
                            case DOT11_AUTH_ALGO_80211_SHARED_KEY: auth = QStringLiteral("Shared"); break;
                            case DOT11_AUTH_ALGO_WPA:           auth = QStringLiteral("WPA"); break;
                            case DOT11_AUTH_ALGO_WPA_PSK:       auth = QStringLiteral("WPA-PSK"); break;
                            case DOT11_AUTH_ALGO_WPA3:          auth = QStringLiteral("WPA3"); break;
                            case DOT11_AUTH_ALGO_RSNA:          auth = QStringLiteral("WPA2"); break;
                            case DOT11_AUTH_ALGO_RSNA_PSK:      auth = QStringLiteral("WPA2-PSK"); break;
                        }
                        out.append(QStringLiteral("   Authentication. . . . . . . : %1").arg(auth));
                        WlanFreeMemory(pConn); pConn = nullptr;
                    }

                    // RSSI (signal strength)
                    LONG rssi = 0;
                    dataSize = sizeof(rssi);
                    if (WlanQueryInterface(hClient, &wi.InterfaceGuid, wlan_intf_opcode_rssi,
                                          nullptr, &dataSize, (PVOID*)&rssi, nullptr) == ERROR_SUCCESS) {
                        int signalPct = (rssi >= -50) ? 100 : (rssi <= -100) ? 0 : 2 * (rssi + 100);
                        out.append(QStringLiteral("   Signal . . . . . . . . . . . : %1%  (%2 dBm)")
                                       .arg(signalPct).arg(rssi));
                    }
                }
                out.append(QString());
            }
            WlanFreeMemory(ifList);
        }
        WlanCloseHandle(hClient, nullptr);
    }
#else
    // Linux: wireless extensions + /sys/class/net/<wireless_iface>/
    // 闁冲厜鍋撻柍鍏夊亾 WiFi table 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾
    static const QVector<DiagnosticFormatter::ColSpec> kWifiCols = {
        {"Interface", 12, false},
        {"SSID",      20, false},
        {"BSSID",     17, false},
        {"Channel",    8, true},   // numeric
        {"Signal",     7, true},   // numeric (dBm / %)
        {"Bitrate",    0, true},   // numeric (Mbps)
    };
    QList<QStringList> wifiRows;
    QSet<QString> seenWifi;
    // 5WHY: was #if PLATFORM_IOS only — macOS CoreWLAN also needs it
    QString wifiSsidCaptured;
#if defined(PLATFORM_IOS)
    QString wifiDiagnosticMsg;  // captured for iOS summary section
    // 5WHY: iosWiFiInfo() was called inside the getifaddrs loop — on
    // iOS 26+ SDK, NEHotspotNetwork blocks up to 5s per en* interface.
    // With Personal Hotspot active (en0=bridge, en1=WiFi, en2=USB),
    // this wasted up to 15s.  Call once before the loop and reuse.
    QString cachedIosSsid, cachedIosBssid;
    {
        QVariantMap wifiData = iosWiFiInfo();
        cachedIosSsid = wifiData.value("ssid", "").toString();
        cachedIosBssid = wifiData.value("bssid", "").toString();
        wifiDiagnosticMsg = wifiData.value("wifiDiagnostics", "").toString();
        if (!cachedIosSsid.isEmpty()) wifiSsidCaptured = cachedIosSsid;
    }
#endif

    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) == 0) {
        for (auto* p = ifa; p; p = p->ifa_next) {
            QString ifName = QString::fromLatin1(p->ifa_name);
#if defined(PLATFORM_IOS)
            // iOS: detect WiFi by interface name prefix (en0/en1 are WiFi)
            if (!ifName.startsWith("en"))
                continue;
#elif defined(__APPLE__)
            // 5WHY: macOS checked /sys/class/net/<iface>/wireless which doesn't
            // exist on macOS → ALL interfaces rejected → "(no wireless)".
            // macOS also uses en0/en1 for WiFi (like iOS). Detect by prefix.
            if (!ifName.startsWith("en"))
                continue;
#else
            if (!QFile::exists(QStringLiteral("/sys/class/net/%1/wireless").arg(ifName)))
                continue;
#endif
            if (seenWifi.contains(ifName)) continue;
            seenWifi.insert(ifName);

            QString ssid = QStringLiteral("-"), bssid = QStringLiteral("-");
            QString channel = QStringLiteral("-"), signal = QStringLiteral("-"), bitrate = QStringLiteral("-");

#if defined(PLATFORM_IOS)
            // iOS: use cached WiFi data (queried once before the loop)
            ssid = cachedIosSsid.isEmpty() ? QStringLiteral("-") : cachedIosSsid;
            bssid = cachedIosBssid.isEmpty() ? QStringLiteral("-") : cachedIosBssid;
#elif defined(__APPLE__)
            // 5WHY: macOS CoreWLAN — SSID/BSSID via dedicated .mm helper.
            // Extracted from inline objc_msgSend to avoid ObjC type issues
            // with Xcode 26.5 SDK (Class/SEL not exposed in .cpp mode).
            {
                QString s = macosWifiSsid();
                if (!s.isEmpty()) {
                    ssid = s;
                    wifiSsidCaptured = ssid;
                }
                QString b = macosWifiBssid();
                if (!b.isEmpty()) bssid = b;
            }
#endif

#if defined(__linux__) && !defined(PLATFORM_ANDROID)
            // Linux WiFi ioctl (requires <linux/wireless.h> — not available on Android Bionic)
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock >= 0) {
                struct iwreq wrq; memset(&wrq, 0, sizeof(wrq));
                strncpy(wrq.ifr_name, ifName.toUtf8().constData(), IFNAMSIZ - 1);

                char essid[IW_ESSID_MAX_SIZE + 1] = {};
                wrq.u.essid.pointer = essid; wrq.u.essid.length = IW_ESSID_MAX_SIZE + 1; wrq.u.essid.flags = 0;
                if (ioctl(sock, SIOCGIWESSID, &wrq) == 0 && wrq.u.essid.length > 0)
                    ssid = QString::fromUtf8(essid, wrq.u.essid.length);

                if (ioctl(sock, SIOCGIWAP, &wrq) == 0 && wrq.u.ap_addr.sa_family == ARPHRD_ETHER)
                    bssid = macToStr((unsigned char*)wrq.u.ap_addr.sa_data);

                if (ioctl(sock, SIOCGIWFREQ, &wrq) == 0) {
                    double freq = wrq.u.freq.m / 1e9;
                    channel = QStringLiteral("%1 (%2 GHz)").arg((int)((freq - 2.412) / 0.005 + 1)).arg(freq, 0, 'f', 3);
                }
                closeSocket(sock);
            }
#endif

#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID) && !defined(__APPLE__)
            QFile wfile(QStringLiteral("/proc/net/wireless"));
            if (wfile.open(QIODevice::ReadOnly)) {
                QTextStream ts(&wfile); ts.readLine(); ts.readLine();
                while (!ts.atEnd()) {
                    QString line = ts.readLine().trimmed();
                    if (line.startsWith(ifName + ':')) {
                        QStringList cols = line.split(QRegularExpression("\\s+"));
                        if (cols.size() >= 4) signal = cols[3].replace('.', "") + " dBm";
                        break;
                    }
                }
            }
#endif // !PLATFORM_IOS (/proc/net/wireless)

#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID) && !defined(__APPLE__)
            QFile rateFile(QStringLiteral("/sys/class/net/%1/wireless/bitrate").arg(ifName));
            if (rateFile.open(QIODevice::ReadOnly)) bitrate = QString::fromLatin1(rateFile.readAll().trimmed());
#endif

            wifiRows.append({ifName, ssid, bssid, channel, signal, bitrate});
        }
        freeifaddrs(ifa);
    }
    out.append(DiagnosticFormatter::formatTable(kWifiCols, wifiRows));
    if (wifiRows.isEmpty()) out.append(QStringLiteral("  (no wireless interfaces detected)"));
#if defined(PLATFORM_IOS)
    {
        // 5WHY: iosInterfaceIPv4 matched by interface name, but the name
        // depends on which AF_LINK entry appeared first in getifaddrs.
        // On iOS 18+ with virtual interfaces, the WiFi IP may be on a
        // different en* interface or the loop may miss it entirely.
        // Fix: directly enumerate getifaddrs for the first en* interface
        // with an AF_INET address — no dependency on interface name from
        // the wifiRows loop above.
        QString wifiIp, wifiGw, wifiIface;
        struct ifaddrs* ifa2 = nullptr;
        if (getifaddrs(&ifa2) == 0) {
            for (auto* p = ifa2; p; p = p->ifa_next) {
                if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
                QString name = QString::fromLatin1(p->ifa_name);
                if (!name.startsWith("en")) continue;
                // Skip bridge/virtual interfaces (en0 is a bridge in hotspot mode)
                if (name.contains("bridge", Qt::CaseInsensitive)) continue;
                wifiIface = name;
                char buf[INET_ADDRSTRLEN] = {0};
                auto* sin = (struct sockaddr_in*)p->ifa_addr;
                inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
                wifiIp = QString::fromLatin1(buf);
                // Found the first non-bridge en* interface — use it.
                // Prefer en0 (primary WiFi); en1 is hotspot-mode WiFi.
                // Break after first match to avoid overwriting with
                // later en* interfaces (e.g. en2 USB tethering).
                break;
            }
            freeifaddrs(ifa2);
        }
        // Gateway from routing table
        if (!wifiIface.isEmpty())
            wifiGw = iosGatewayForInterface(wifiIface);
        out.append(QString());
        out.append(QStringLiteral("  IP Address: %1").arg(wifiIp.isEmpty() ? QStringLiteral("(not connected)") : wifiIp));
        if (!wifiGw.isEmpty())
            out.append(QStringLiteral("  Gateway: %1").arg(wifiGw));
        out.append(QStringLiteral("  Channel/Signal/Bitrate: unavailable on iOS (no public API)"));
        if (wifiSsidCaptured.isEmpty()) {
            out.append(QString());
            // 5WHY: Generic note listed 3 possible causes — user couldn't tell
            // which one applied. Now shows the specific diagnostic from
            // iosWiFiInfo() which checks [CLLocationManager authorizationStatus].
            QString wifiDiag = wifiDiagnosticMsg;
            if (!wifiDiag.isEmpty()) {
                out.append(QStringLiteral("  %1").arg(wifiDiag));
            } else {
                out.append(QStringLiteral("  Note: SSID/BSSID need the \"Access WiFi Information\" entitlement,"));
                out.append(QStringLiteral("        Location permission, and an active WiFi connection."));
            }
        }
    }
#endif
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = out.size() > 3 ? DiagStatus::Pass : DiagStatus::Info;
    r.summary = QStringLiteral("WiFi Diagnostics Complete");
    r.durationMs = t.elapsed();
    return r;
}

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// G1 闁?NIC Advanced (wmic nic format)
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
}
