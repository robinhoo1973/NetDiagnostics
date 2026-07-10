#include "engine/diagnostics/GHelpers.h"
namespace G1G2G3Native {
DiagnosticResult tcpSettings(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("TCP/IP Settings (table mode):"));
    out.append(QString());

#if defined(_WIN32)
    // Try PowerShell first (Get-NetTCPSetting), fall back to netsh + registry
    {
        bool gotData = false;
        QProcess ps;
        ps.start(QStringLiteral("powershell"), QStringList()
            << QStringLiteral("-NoProfile") << QStringLiteral("-Command")
            << QStringLiteral("Get-NetTCPSetting | Select SettingName,CongestionProvider,AutoTuningLevelLocal | Format-List"));
        if (ps.waitForFinished(5000)) {
            QString tcpOut = QString::fromUtf8(ps.readAllStandardOutput()).trimmed();
            if (!tcpOut.isEmpty()) {
                gotData = true;
                out.append(QStringLiteral("  (Get-NetTCPSetting)"));
                for (auto& line : tcpOut.split('\n')) {
                    QString t = line.trimmed();
                    if (!t.isEmpty()) out.append(QStringLiteral("    ") + t);
                }
            }
        }
        if (!gotData) {
            // Fallback: netsh interface tcp show global
            QProcess ns;
            ns.start(QStringLiteral("netsh"), QStringList()
                << QStringLiteral("interface") << QStringLiteral("tcp") << QStringLiteral("show") << QStringLiteral("global"));
            if (ns.waitForFinished(5000)) {
                QString nsOut = QString::fromLocal8Bit(ns.readAllStandardOutput()).trimmed();
                if (!nsOut.isEmpty()) {
                    out.append(QStringLiteral("  (netsh int tcp show global)"));
                    for (auto& line : nsOut.split('\n')) {
                        QString t = line.trimmed();
                        if (!t.isEmpty()) out.append(QStringLiteral("    ") + t);
                    }
                } else {
                    out.append(QStringLiteral("  (TCP settings unavailable — try 'netsh int tcp show global')"));
                }
            } else {
                out.append(QStringLiteral("  (TCP settings unavailable — netsh/powershell not available)"));
            }
        }
        // Read TCP/IP parameters from registry (available on all Windows editions)
        HKEY hKey = nullptr;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
                0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            out.append(QStringLiteral("  [Registry TCP/IP Parameters]"));
            DWORD val = 0, sz = sizeof(val);
            if (RegQueryValueExA(hKey, "KeepAliveTime", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS)
                out.append(QStringLiteral("  KeepAliveTime: %1 ms").arg(val));
            sz = sizeof(val);
            if (RegQueryValueExA(hKey, "KeepAliveInterval", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS)
                out.append(QStringLiteral("  KeepAliveInterval: %1 ms").arg(val));
            sz = sizeof(val);
            if (RegQueryValueExA(hKey, "Tcp1323Opts", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS)
                out.append(QStringLiteral("  TCP1323Opts (Window Scaling+Timestamps): %1").arg(val));
            sz = sizeof(val);
            if (RegQueryValueExA(hKey, "DefaultTTL", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS)
                out.append(QStringLiteral("  DefaultTTL: %1").arg(val));
            sz = sizeof(val);
            if (RegQueryValueExA(hKey, "EnablePMTUDiscovery", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS)
                out.append(QStringLiteral("  EnablePMTUDiscovery: %1").arg(val));
            sz = sizeof(val);
            if (RegQueryValueExA(hKey, "TcpMaxDataRetransmissions", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS)
                out.append(QStringLiteral("  TcpMaxDataRetransmissions: %1").arg(val));
            RegCloseKey(hKey);
        }
    }
#else
    static const QVector<DiagnosticFormatter::ColSpec> kTcpCols = {
        {"Setting", 20, false},
        {"Value",    0, false},
    };
    QList<QStringList> tcpRows;
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID) && !defined(__APPLE__)
    auto readSys = [&](const QString& path, const QString& label) {
        QFile f(path);
        QString val = f.open(QIODevice::ReadOnly) ? QString::fromLatin1(f.readAll().trimmed()) : QStringLiteral("-");
        tcpRows.append({label, val});
    };
    readSys(QStringLiteral("/proc/sys/net/ipv4/tcp_congestion_control"), QStringLiteral("Congestion Control"));
    readSys(QStringLiteral("/proc/sys/net/ipv4/tcp_window_scaling"), QStringLiteral("Window Scaling"));
    readSys(QStringLiteral("/proc/sys/net/ipv4/tcp_timestamps"), QStringLiteral("Timestamps"));
    readSys(QStringLiteral("/proc/sys/net/ipv4/tcp_sack"), QStringLiteral("Selective ACK"));
    readSys(QStringLiteral("/proc/sys/net/ipv4/tcp_fastopen"), QStringLiteral("TCP Fast Open"));
#elif defined(__APPLE__) && !defined(PLATFORM_IOS)
    // ── macOS: read TCP settings via sysctl ──
    auto macOSSysctl = [&](const char* name, const QString& label, bool isBool = false) {
        int val = 0;
        size_t sz = sizeof(val);
        if (sysctlbyname(name, &val, &sz, nullptr, 0) == 0) {
            if (isBool)
                tcpRows.append({label, val ? QStringLiteral("1 (enabled)") : QStringLiteral("0 (disabled)")});
            else
                tcpRows.append({label, QString::number(val)});
        } else {
            tcpRows.append({label, QStringLiteral("-")});
        }
    };
    auto macOSSysctlStr = [&](const char* name, const QString& label) {
        size_t sz = 0;
        if (sysctlbyname(name, nullptr, &sz, nullptr, 0) == 0 && sz > 0) {
            QByteArray buf((int)sz, '\0');
            if (sysctlbyname(name, buf.data(), &sz, nullptr, 0) == 0)
                tcpRows.append({label, QString::fromLatin1(buf.trimmed())});
            else
                tcpRows.append({label, QStringLiteral("-")});
        } else tcpRows.append({label, QStringLiteral("-")});
    };
    macOSSysctlStr("net.inet.tcp.cc", QStringLiteral("Congestion Algorithm"));
    macOSSysctl("net.inet.tcp.rfc1323", QStringLiteral("Window Scaling (RFC1323)"), true);
    macOSSysctl("net.inet.tcp.sack", QStringLiteral("Selective ACK"), true);
    macOSSysctl("net.inet.tcp.fastopen", QStringLiteral("TCP Fast Open"), true);
    macOSSysctl("net.inet.tcp.delayed_ack", QStringLiteral("Delayed ACK"), true);
    macOSSysctl("net.inet.tcp.keepidle", QStringLiteral("Keepalive Idle (s)"));
    macOSSysctl("net.inet.tcp.keepintvl", QStringLiteral("Keepalive Interval (s)"));
#else  // PLATFORM_IOS
    out.append(QStringLiteral("  [iOS] TCP settings: unavailable (kernel sysctls restricted)"));
#endif
    if (!tcpRows.isEmpty())
        out.append(DiagnosticFormatter::formatTable(kTcpCols, tcpRows));
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
#if defined(PLATFORM_IOS)
    r.status = DiagStatus::Skipped;
    r.summary = QStringLiteral("Unavailable on iOS (kernel sysctls restricted)");
#elif defined(__APPLE__)
    r.status = tcpRows.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass;
    r.summary = tcpRows.isEmpty()
        ? QStringLiteral("TCP settings unavailable")
        : QStringLiteral("TCP settings collected via sysctl");
#else
    r.status = DiagStatus::Pass;
    r.summary = QStringLiteral("TCP settings collected");
#endif
    r.durationMs = t.elapsed();
    return r;
}

}
