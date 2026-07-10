#include "engine/diagnostics/GHelpers.h"
namespace G1G2G3Native {
DiagnosticResult networkProfile(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G2;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Network Profile Information:"));
    out.append(QString());

#if defined(_WIN32)
    // ── Hostname ──────────────────────────────────────────────────────
    char hostBuf[256] = {};
    DWORD hostLen = sizeof(hostBuf);
    GetComputerNameA(hostBuf, &hostLen);
    out.append(QStringLiteral("  Hostname: %1").arg(QString::fromLatin1(hostBuf)));

    // ── IP Forwarding from registry ───────────────────────────────────
    {
        HKEY hKey = nullptr;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                "SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
                0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD val = 0, sz = sizeof(val);
            if (RegQueryValueExA(hKey, "IPEnableRouter", nullptr, nullptr, (LPBYTE)&val, &sz) == ERROR_SUCCESS)
                out.append(QStringLiteral("  IP Forwarding: %1").arg(val ? "Enabled" : "Disabled"));
            RegCloseKey(hKey);
        }
    }
    // ── Firewall status per profile via QProcess (PowerShell) ─────────
    {
        QProcess ps;
        ps.start(QStringLiteral("powershell"), QStringList()
            << QStringLiteral("-NoProfile") << QStringLiteral("-Command")
            << QStringLiteral("Get-NetFirewallProfile | Select Name,Enabled | Format-List"));
        if (ps.waitForFinished(5000)) {
            QString fwOut = QString::fromUtf8(ps.readAllStandardOutput()).trimmed();
            if (!fwOut.isEmpty()) {
                out.append(QString());
                out.append(QStringLiteral("  Firewall Profile Status:"));
                for (auto& line : fwOut.split('\n')) {
                    QString t = line.trimmed();
                    if (!t.isEmpty()) out.append(QStringLiteral("    ") + t);
                }
            }
        }
    }
    // ── Network category (Public/Private/Domain) ──────────────────────
    {
        QProcess ps;
        ps.start(QStringLiteral("powershell"), QStringList()
            << QStringLiteral("-NoProfile") << QStringLiteral("-Command")
            << QStringLiteral("Get-NetConnectionProfile | Select Name,NetworkCategory | Format-List"));
        if (ps.waitForFinished(5000)) {
            QString catOut = QString::fromUtf8(ps.readAllStandardOutput()).trimmed();
            if (!catOut.isEmpty()) {
                out.append(QString());
                out.append(QStringLiteral("  Connection Profile:"));
                for (auto& line : catOut.split('\n')) {
                    QString t = line.trimmed();
                    if (!t.isEmpty()) out.append(QStringLiteral("    ") + t);
                }
            }
        }
    }
#else
    char hostname[256] = {};
    gethostname(hostname, sizeof(hostname));
    out.append(QStringLiteral("  Hostname: %1").arg(QString::fromLatin1(hostname)));

    // Check common network profiles via /proc/sys/net
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID) && !defined(__APPLE__)
    QFile fwd(QStringLiteral("/proc/sys/net/ipv4/ip_forward"));
    if (fwd.open(QIODevice::ReadOnly))
        out.append(QStringLiteral("  IP Forwarding: %1").arg(QString::fromLatin1(fwd.readAll().trimmed()) == "1" ? "Enabled" : "Disabled"));
#else
#if defined(__APPLE__) && !defined(PLATFORM_IOS)
    int fwd = 0; size_t fwdSz = sizeof(fwd);
    if (sysctlbyname("net.inet.ip.forwarding", &fwd, &fwdSz, nullptr, 0) == 0)
        out.append(QStringLiteral("  IP Forwarding: %1").arg(fwd ? QStringLiteral("Enabled") : QStringLiteral("Disabled")));
    else
        out.append(QStringLiteral("  IP Forwarding: Unknown"));
#endif // !PLATFORM_IOS
#if defined(PLATFORM_IOS)
    out.append(QStringLiteral("  [iOS] IP forwarding: unavailable (restricted by Apple)"));
#endif
#endif // _WIN32
#endif  // close converted #elif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = QStringLiteral("Network profile collected");
    r.durationMs = 0;
    return r;
}

}
