#include "Diagnostics/Model/GBase.h"
#include "Diagnostics/Model/GHelpers.h"
namespace G1G2G3Native {
DiagnosticResult dhcpStatus(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;
    QStringList dhcpSummary; // "eth0=192.168.1.100"

    out.append(QString());
    out.append(QStringLiteral("DHCP Client Status"));
    out.append(QString());

#if defined(_WIN32)
    ULONG bufLen = 15000;
    QByteArray buf(bufLen, '\0');
    PIP_ADAPTER_ADDRESSES adapters = (PIP_ADAPTER_ADDRESSES)buf.data();
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS, nullptr, adapters, &bufLen) == NO_ERROR) {
        for (auto* a = adapters; a; a = a->Next) {
            bool dhcp = (a->Flags & IP_ADAPTER_DHCP_ENABLED) != 0;
            out.append(QStringLiteral("   Adapter: %1").arg(QString::fromWCharArray(a->FriendlyName)));
            out.append(QStringLiteral("   DHCP Enabled. . . . . . . . . . . : %1").arg(dhcp ? "Yes" : "No"));
            if (a->Dhcpv4Server.iSockaddrLength > 0) {
                char ip[64]; DWORD ipLen = sizeof(ip);
                WSAAddressToStringA(a->Dhcpv4Server.lpSockaddr, a->Dhcpv4Server.iSockaddrLength, nullptr, ip, &ipLen);
                out.append(QStringLiteral("   DHCP Server . . . . . . . . . . . : %1").arg(QString::fromLatin1(ip)));
            }
            if (dhcp && a->FirstUnicastAddress) {
                char ip[64]; DWORD ipLen = sizeof(ip);
                WSAAddressToStringA(a->FirstUnicastAddress->Address.lpSockaddr, a->FirstUnicastAddress->Address.iSockaddrLength, nullptr, ip, &ipLen);
                dhcpSummary.append(QStringLiteral("%1=%2").arg(QString::fromWCharArray(a->FriendlyName), QString::fromLatin1(ip)));
            }
            out.append(QString());
        }
    }
#else
#if defined(PLATFORM_IOS)
    out.append(QStringLiteral("  [iOS] DHCP lease details: unavailable (restricted by Apple)"));
    out.append(QStringLiteral("  DHCP is system-managed on iOS; lease files are not accessible."));
#else
    bool anyDhcp = false;
    // 1. systemd-networkd lease files (most detailed)
    QDir leaseDir(QStringLiteral("/run/systemd/netif/leases"));
    if (leaseDir.exists()) {
        for (const auto& fi : leaseDir.entryInfoList(QDir::Files)) {
            QFile f(fi.absoluteFilePath());
            if (f.open(QIODevice::ReadOnly)) {
                QString ifName = fi.fileName();
                out.append(QStringLiteral("   Interface: %1").arg(ifName));
                out.append(QStringLiteral("   DHCP Enabled. . . . . . . . . . . : Yes"));
                anyDhcp = true;
                QTextStream ts(&f);
                while (!ts.atEnd()) {
                    QString line = ts.readLine().trimmed();
                    if (line.startsWith("ADDRESS="))
                        { QString ip = line.mid(8); out.append(QStringLiteral("   IPv4 Address. . . . . . . . . . . : %1 (Preferred)").arg(ip)); if (!dhcpSummary.contains(ifName + "=" + ip)) dhcpSummary.append(QStringLiteral("%1=%2").arg(ifName, ip)); }
                    else if (line.startsWith("NETMASK="))
                        out.append(QStringLiteral("   Subnet Mask . . . . . . . . . . . : %1").arg(line.mid(8)));
                    else if (line.startsWith("ROUTER="))
                        out.append(QStringLiteral("   DHCP Server / Gateway . . . . . . : %1").arg(line.mid(7)));
                    else if (line.startsWith("SERVER_ADDRESS="))
                        out.append(QStringLiteral("   DHCP Server . . . . . . . . . . . : %1").arg(line.mid(15)));
                    else if (line.startsWith("DNS="))
                        out.append(QStringLiteral("   DNS Servers . . . . . . . . . . . : %1").arg(line.mid(4)));
                    else if (line.startsWith("LEASE_TIME=")) {
                        int secs = line.mid(11).toInt();
                        out.append(QStringLiteral("   Lease Duration . . . . . . . . . : %1").arg(secs >= 3600 ? QStringLiteral("%1 hours").arg(secs / 3600) : QStringLiteral("%1 seconds").arg(secs)));
                    }
                }
                out.append(QString());
            }
        }
    }

    // 2. dhclient leases as fallback
    QDir dhclientDir(QStringLiteral("/var/lib/dhcp"));
    if (dhclientDir.exists()) {
        for (const auto& fi : dhclientDir.entryInfoList({"dhclient*.leases"}, QDir::Files)) {
            QFile f(fi.absoluteFilePath());
            if (f.open(QIODevice::ReadOnly)) {
                QTextStream ts(&f);
                QString currentIface, currentIp;
                while (!ts.atEnd()) {
                    QString line = ts.readLine().trimmed();
                    if (line.startsWith("interface ")) currentIface = line.mid(10).remove('"').remove(';');
                    if (line.startsWith("fixed-address ")) currentIp = line.mid(14).remove(' ').remove(';');
                    if (line.contains("dhcp-server-identifier"))
                        out.append(QStringLiteral("   DHCP Server . . . . . . . . . . . : %1").arg(line.section(' ', -1).remove(';')));
                    if (line.contains("renew ")) out.append(QStringLiteral("   Lease Renew . . . . . . . . . . . : %1").arg(line.section(' ', 2, 3).remove(';')));
                    if (line.contains("expire ")) out.append(QStringLiteral("   Lease Expires . . . . . . . . . . : %1").arg(line.section(' ', 2, 3).remove(';')));
                }
                if (!currentIface.isEmpty() && !currentIp.isEmpty()) {
                    anyDhcp = true;
                    out.append(QStringLiteral("   IPv4 Address. . . . . . . . . . . : %1 (Preferred)").arg(currentIp));
                    if (!dhcpSummary.contains(QStringLiteral("%1=%2").arg(currentIface, currentIp)))
                        dhcpSummary.append(QStringLiteral("%1=%2").arg(currentIface, currentIp));
                }
                out.append(QString());
            }
        }
    }

    // 3. Check /proc/net/route for gateways (DHCP routers) on interfaces without lease files
    if (!anyDhcp) {
        QFile routeFile(QStringLiteral("/proc/net/route"));
        if (routeFile.open(QIODevice::ReadOnly)) {
            QTextStream ts(&routeFile); ts.readLine(); // header
            while (!ts.atEnd()) {
                QStringList cols = ts.readLine().trimmed().split('\t');
                if (cols.size() >= 11 && cols[2].toUInt(nullptr, 16) != 0) {
                    out.append(QStringLiteral("   Interface: %1 (via DHCP — inferred from default route)").arg(cols[0]));
                    out.append(QStringLiteral("   Default Gateway . . . . . . . . . : %1").arg(ipToStr(cols[2].toUInt(nullptr, 16))));
                    out.append(QStringLiteral("   DHCP Enabled. . . . . . . . . . . : Likely Yes"));
                    out.append(QString());
                }
            }
        }
    }

    if (!anyDhcp && out.size() <= 4)
        out.append(QStringLiteral("   No DHCP lease information found (static IP or managed externally)"));
#endif // !PLATFORM_IOS
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = dhcpSummary.isEmpty() ? QStringLiteral("No DHCP leases found (static IP?)")
                 : QStringLiteral("DHCP: %1").arg(dhcpSummary.join(QStringLiteral(", ")));
    r.durationMs = t.elapsed();
    return r;
}

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// G2 闁?Routing Table (route print format)
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
}
