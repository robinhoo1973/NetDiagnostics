#include "Diagnostics/Model/GBase.h"
#include "Common/Model/ResultProperty.h"
#include "Diagnostics/Model/GHelpers.h"
namespace G1G2G3Native {
DiagnosticResult dhcpStatus(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;
    QStringList dhcpSummary; // "eth0=192.168.1.100"
    QVector<ResultProperty> props;

    out.append(QString());
    out.append(QStringLiteral("DHCP Client Status"));
    out.append(QString());

    out.append(QStringLiteral("  %1  %2  %3  %4")
        .arg(QStringLiteral("Interface"), -18)
        .arg(QStringLiteral("DHCP"), -6)
        .arg(QStringLiteral("IP Address"), -18)
        .arg(QStringLiteral("Server")));
    out.append(QStringLiteral("  %1  %2  %3  %4")
        .arg(QString(18, '-')).arg(QString(6, '-')).arg(QString(18, '-')).arg(QString(18, '-')));

#if defined(_WIN32)
    ULONG bufLen = 15000;
    QByteArray buf(bufLen, '\0');
    PIP_ADAPTER_ADDRESSES adapters = (PIP_ADAPTER_ADDRESSES)buf.data();
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS, nullptr, adapters, &bufLen) == NO_ERROR) {
        for (auto* a = adapters; a; a = a->Next) {
            QString ifName = QString::fromWCharArray(a->FriendlyName);
            bool dhcp = (a->Flags & IP_ADAPTER_DHCP_ENABLED) != 0;
            QString ipStr, serverStr;
            if (a->Dhcpv4Server.iSockaddrLength > 0) {
                char ip[64]; DWORD ipLen = sizeof(ip);
                WSAAddressToStringA(a->Dhcpv4Server.lpSockaddr, a->Dhcpv4Server.iSockaddrLength, nullptr, ip, &ipLen);
                serverStr = QString::fromLatin1(ip);
            }
            if (dhcp && a->FirstUnicastAddress) {
                char ip[64]; DWORD ipLen = sizeof(ip);
                WSAAddressToStringA(a->FirstUnicastAddress->Address.lpSockaddr, a->FirstUnicastAddress->Address.iSockaddrLength, nullptr, ip, &ipLen);
                ipStr = QString::fromLatin1(ip);
                dhcpSummary.append(QStringLiteral("%1=%2").arg(ifName, ipStr));
            }
            out.append(QStringLiteral("  %1  %2  %3  %4")
                .arg(ifName, -18).arg(dhcp ? "Yes" : "No", -6)
                .arg(ipStr.isEmpty() ? "-" : ipStr, -18)
                .arg(serverStr.isEmpty() ? "-" : serverStr));
            ResultProperty prop(ifName, ipStr.isEmpty() ? "(no IP)" : ipStr);
            {
                ResultProperty dhcpChild("DHCP", dhcp ? "Yes" : "No");
                prop.children.append(dhcpChild);
            }
            if (!serverStr.isEmpty()) {
                ResultProperty srvChild("Server", serverStr);
                prop.children.append(srvChild);
            }
            props.append(prop);
        }
        out.append(QString());
    }
#else
#if defined(PLATFORM_IOS)
    out.append(QStringLiteral("  %1  %2  %3  %4")
        .arg("(system-managed)", -18).arg("Yes", -6)
        .arg("(not exposed)", -18).arg("(not exposed)"));
    out.append(QString());
    out.append(QStringLiteral("  iOS manages DHCP at the system level —"));
    out.append(QStringLiteral("  lease details are not accessible to third-party apps."));
    ResultProperty iosProp("iOS DHCP", "system-managed");
    props.append(iosProp);
#else
    bool anyDhcp = false;
    // 1. systemd-networkd lease files (most detailed)
    QDir leaseDir(QStringLiteral("/run/systemd/netif/leases"));
    if (leaseDir.exists()) {
        for (const auto& fi : leaseDir.entryInfoList(QDir::Files)) {
            QFile f(fi.absoluteFilePath());
            if (f.open(QIODevice::ReadOnly)) {
                QString ifName = fi.fileName();
                QString ipStr, serverStr;
                anyDhcp = true;
                QTextStream ts(&f);
                while (!ts.atEnd()) {
                    QString line = ts.readLine().trimmed();
                    if (line.startsWith("ADDRESS="))
                        { ipStr = line.mid(8); dhcpSummary.append(QStringLiteral("%1=%2").arg(ifName, ipStr)); }
                    else if (line.startsWith("SERVER_ADDRESS="))
                        serverStr = line.mid(15);
                }
                out.append(QStringLiteral("  %1  %2  %3  %4")
                    .arg(ifName, -18).arg("Yes", -6)
                    .arg(ipStr.isEmpty() ? "-" : ipStr, -18)
                    .arg(serverStr.isEmpty() ? "-" : serverStr));
                ResultProperty leaseProp(ifName, ipStr.isEmpty() ? "(no IP)" : ipStr);
                leaseProp.children.append(ResultProperty("DHCP", "Yes"));
                if (!serverStr.isEmpty()) leaseProp.children.append(ResultProperty("Server", serverStr));
                props.append(leaseProp);
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
                    out.append(QStringLiteral("  %1  %2  %3  %4")
                        .arg(currentIface, -18).arg("Yes", -6)
                        .arg(currentIp, -18).arg("-"));
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
                    QString gw = ipToStr(cols[2].toUInt(nullptr, 16));
                    out.append(QStringLiteral("  %1  %2  %3  %4")
                        .arg(cols[0], -18).arg("Likely", -6)
                        .arg("-", -18).arg(gw));
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
    r.properties = props;
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
