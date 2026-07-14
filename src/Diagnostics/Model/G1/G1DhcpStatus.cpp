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
    QList<QStringList> dhcpRows;  // 5WHY: unified rows for formatTable

    out.append(QString());
    out.append(QStringLiteral("DHCP Client Status"));
    out.append(QString());

    // 5WHY: Used manual QString::arg formatting instead of DiagnosticFormatter::formatTable.
    // formatTable provides consistent column alignment, separators, and empty-state handling
    // across all diagnostic output.  Other tests (G3DnsServers, G2RoutingTable) use this pattern.
    static const QVector<DiagnosticFormatter::ColSpec> kDhcpCols = {
        {"Interface", 18, false},
        {"DHCP",       6, false},
        {"IP Address", 18, false},
        {"Server",     0, false},
    };

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
            dhcpRows.append({ifName, dhcp ? "Yes" : "No",
                           ipStr.isEmpty() ? "-" : ipStr,
                           serverStr.isEmpty() ? "-" : serverStr});
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
        if (!dhcpRows.isEmpty())
            out << DiagnosticFormatter::formatTable(kDhcpCols, dhcpRows);
        out.append(QString());
    }
#elif defined(PLATFORM_IOS)
    QList<QStringList> iosRows;
    iosRows.append({"(system-managed)", "Yes", "(not exposed)", "(not exposed)"});
    out.append(DiagnosticFormatter::formatTable(kDhcpCols, iosRows));
    out.append(QString());
    out.append(QStringLiteral("  iOS manages DHCP at the system level —"));
    out.append(QStringLiteral("  lease details are not accessible to third-party apps."));
    ResultProperty iosProp("iOS DHCP", "system-managed");
    props.append(iosProp);
#elif defined(__APPLE__)  // macOS (not iOS)
    // 5WHY: macOS fell through to the Linux #else branch which tries
    // /run/systemd, /var/lib/dhcp, /proc/net/route — none exist on macOS.
    // Add an explicit macOS stub that reports DHCP info is system-managed.
    QList<QStringList> macRows;
    macRows.append({"(system-managed)", "Yes", "(use ifconfig)", "(not exposed)"});
    out.append(DiagnosticFormatter::formatTable(kDhcpCols, macRows));
    out.append(QString());
    out.append(QStringLiteral("  macOS manages DHCP via the SystemConfiguration framework —"));
    out.append(QStringLiteral("  lease details are not directly accessible. Use `ipconfig getpacket <iface>`"));
    out.append(QStringLiteral("  in Terminal for per-interface DHCP lease information."));
    ResultProperty macProp("macOS DHCP", "system-managed");
    props.append(macProp);
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
                QTextStream ts(&f);
                while (!ts.atEnd()) {
                    QString line = ts.readLine().trimmed();
                    if (line.startsWith("ADDRESS="))
                        { ipStr = line.mid(8); dhcpSummary.append(QStringLiteral("%1=%2").arg(ifName, ipStr)); }
                    else if (line.startsWith("SERVER_ADDRESS="))
                        serverStr = line.mid(15);
                }
                // 5WHY: anyDhcp was set before parsing — an empty/corrupt
                // lease file would set anyDhcp=true with no IP, suppressing
                // the /proc/net/route fallback. Now only set when data found.
                if (!ipStr.isEmpty()) anyDhcp = true;
                dhcpRows.append({ifName, "Yes",
                                ipStr.isEmpty() ? "-" : ipStr,
                                serverStr.isEmpty() ? "-" : serverStr});
                ResultProperty leaseProp(ifName, ipStr.isEmpty() ? "(no IP)" : ipStr);
                leaseProp.children.append(ResultProperty("DHCP", "Yes"));
                if (!serverStr.isEmpty()) leaseProp.children.append(ResultProperty("Server", serverStr));
                props.append(leaseProp);
            }
        }
        if (!dhcpRows.isEmpty())
            out << DiagnosticFormatter::formatTable(kDhcpCols, dhcpRows);
        out.append(QString());
    }
    // 5WHY: systemd block appended to shared dhcpRows. Clear so the
    // next fallback block does not duplicate rows already shown.
    dhcpRows.clear();

    // 2. dhclient leases as fallback
    QDir dhclientDir(QStringLiteral("/var/lib/dhcp"));
    if (dhclientDir.exists()) {
        for (const auto& fi : dhclientDir.entryInfoList({"dhclient*.leases"}, QDir::Files)) {
            QFile f(fi.absoluteFilePath());
            if (f.open(QIODevice::ReadOnly)) {
                QTextStream ts(&f);
                QString currentIface, currentIp;
                // 5WHY: dhclient lease files contain richer data (server
                // identifier, renew/expire times) that was lost during the
                // formatTable migration. Collect them as supplementary output
                // prefixed with the interface name for identification.
                QStringList dhclientExtras;
                while (!ts.atEnd()) {
                    QString line = ts.readLine().trimmed();
                    if (line.startsWith("interface ")) currentIface = line.mid(10).remove('"').remove(';');
                    if (line.startsWith("fixed-address ")) currentIp = line.mid(14).remove(' ').remove(';');
                    if (line.contains("dhcp-server-identifier"))
                        dhclientExtras.append(QStringLiteral("   [%1] DHCP Server . . . . . . . . . . . : %2").arg(currentIface, line.section(' ', -1).remove(';')));
                    if (line.contains("renew "))
                        dhclientExtras.append(QStringLiteral("   [%1] Lease Renew . . . . . . . . . . . : %2").arg(currentIface, line.section(' ', 2, 3).remove(';')));
                    if (line.contains("expire "))
                        dhclientExtras.append(QStringLiteral("   [%1] Lease Expires . . . . . . . . . . : %2").arg(currentIface, line.section(' ', 2, 3).remove(';')));
                }
                if (!currentIface.isEmpty() && !currentIp.isEmpty()) {
                    anyDhcp = true;
                    dhcpRows.append({currentIface, "Yes", currentIp, "-"});
                    if (!dhcpSummary.contains(QStringLiteral("%1=%2").arg(currentIface, currentIp)))
                        dhcpSummary.append(QStringLiteral("%1=%2").arg(currentIface, currentIp));
                }
                // Output per-file supplementary details after collecting row data
                for (const auto& extra : dhclientExtras)
                    out.append(extra);
            }
        }
        if (!dhcpRows.isEmpty())
            out << DiagnosticFormatter::formatTable(kDhcpCols, dhcpRows);
        out.append(QString());
    }
    // 5WHY: clear after dhclient block so /proc/net/route fallback (block 3)
    // does not see stale rows from blocks 1-2 if anyDhcp is still false.
    dhcpRows.clear();

    // 3. Check /proc/net/route for gateways (DHCP routers) on interfaces without lease files
    if (!anyDhcp) {
        QFile routeFile(QStringLiteral("/proc/net/route"));
        if (routeFile.open(QIODevice::ReadOnly)) {
            QTextStream ts(&routeFile); ts.readLine(); // header
            while (!ts.atEnd()) {
                QStringList cols = ts.readLine().trimmed().split('\t');
                if (cols.size() >= 11 && cols[2].toUInt(nullptr, 16) != 0) {
                    QString gw = ipToStr(cols[2].toUInt(nullptr, 16));
                    dhcpRows.append({cols[0], "Likely", "-", gw});
                }
            }
        }
        if (!dhcpRows.isEmpty())
            out << DiagnosticFormatter::formatTable(kDhcpCols, dhcpRows);
        out.append(QString());
    }

    // 5WHY: The old sentinel `out.size() <= 4` assumed only 3 header lines
    // + 1 blank. Each processing block now unconditionally appends a blank
    // line when its directory exists (even if empty), so out.size() can
    // reach 5-6 before this check. Bumped to 8 to be safe.
    if (!anyDhcp && out.size() <= 8)
        out.append(QStringLiteral("   No DHCP lease information found (static IP or managed externally)"));
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.properties = props;
    // 5WHY: status was unconditionally Pass — on macOS (no Linux paths,
    // no Windows API) and on API failures, the result was Pass with empty
    // data, misleading the user. Now: Info when no leases found (may be
    // intentional static IP), Pass when DHCP data collected.
    r.status = dhcpSummary.isEmpty() ? DiagStatus::Info : DiagStatus::Pass;
    r.summary = dhcpSummary.isEmpty() ? QStringLiteral("No DHCP leases found (static IP?)")
                 : QStringLiteral("DHCP: %1").arg(dhcpSummary.join(QStringLiteral(", ")));
    r.durationMs = t.elapsed();
    return r;
}

// G2 — Routing Table (route print format)
// Collects IPv4/v6 routing table entries with gateway, metric, and interface info.
}
