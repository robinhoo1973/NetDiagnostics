#include "Diagnostics/Model/GBase.h"
#include "Diagnostics/Model/GHelpers.h"
namespace G1G2G3Native {
DiagnosticResult wiredDiagnostics(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

#if defined(PLATFORM_IOS)
    // iOS devices have no wired Ethernet NIC, and /sys/class/net is inaccessible,
    // so every field would be blank. Report as Skipped (consistent with ARP/TCP).
    out.append(QString());
    out.append(QStringLiteral("Wired Information:"));
    out.append(QString());
    out.append(QStringLiteral("  [iOS] No wired Ethernet interface — not applicable on iOS devices."));
    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Skipped;
    r.summary = QStringLiteral("Not applicable on iOS (no wired NIC)");
    r.durationMs = t.elapsed();
    return r;
#endif

#if defined(_WIN32)
    // ── GetIfTable2 (replaces wmic placeholder) ──────────────────────
    out.append(QStringLiteral("Wired Information (table mode):"));
    out.append(QString());
    static const QVector<DiagnosticFormatter::ColSpec> kWiredCols = {
        {"Interface",   12, false}, {"Speed",    7, true}, {"Duplex", 6, false},
        {"MTU",         4, true},   {"Link",     4, false}, {"State", 10, false},
        {"MAC Address", 17, false},
    };
    QList<QStringList> wiredRows;
    PMIB_IF_TABLE2 ifTable = nullptr;
    if (GetIfTable2(&ifTable) == NO_ERROR && ifTable) {
        for (ULONG i = 0; i < ifTable->NumEntries; i++) {
            auto& row = ifTable->Table[i];
            if (row.Type != IF_TYPE_ETHERNET_CSMACD) continue;
            if (row.OperStatus == IfOperStatusNotPresent) continue;
            QString ifName   = QString::fromWCharArray(row.Alias);
            QString speedStr = (row.TransmitLinkSpeed > 0)
                ? (row.TransmitLinkSpeed >= 1000000000
                    ? QStringLiteral("%1 Gbps").arg(row.TransmitLinkSpeed / 1.0e9, 0, 'f', 1)
                    : QStringLiteral("%1 Mbps").arg(row.TransmitLinkSpeed / 1.0e6, 0, 'f', 0))
                : QStringLiteral("N/A");
            QString linkStr = (row.OperStatus == IfOperStatusUp) ? "Up" : "Down";
            QString mtuStr  = QString::number(row.Mtu);
            QString stateStr = (row.OperStatus == IfOperStatusUp) ? "Up" : "Down";
            QString macStr  = (row.PhysicalAddressLength >= 6)
                ? QStringLiteral("%1:%2:%3:%4:%5:%6")
                    .arg(row.PhysicalAddress[0], 2, 16, QLatin1Char('0'))
                    .arg(row.PhysicalAddress[1], 2, 16, QLatin1Char('0'))
                    .arg(row.PhysicalAddress[2], 2, 16, QLatin1Char('0'))
                    .arg(row.PhysicalAddress[3], 2, 16, QLatin1Char('0'))
                    .arg(row.PhysicalAddress[4], 2, 16, QLatin1Char('0'))
                    .arg(row.PhysicalAddress[5], 2, 16, QLatin1Char('0'))
                : QStringLiteral("N/A");
            wiredRows.append({ifName.left(12), speedStr, QStringLiteral("N/A"), mtuStr, linkStr, stateStr, macStr});
        }
        FreeMibTable(ifTable);
    }
    r.status  = wiredRows.isEmpty() ? DiagStatus::Info : DiagStatus::Pass;
    r.summary = wiredRows.isEmpty() ? QStringLiteral("No wired Ethernet adapters found")
                                    : QStringLiteral("Wired NIC properties collected via GetIfTable2");
    QStringList table = DiagnosticFormatter::formatTable(kWiredCols, wiredRows);
    r.details = table.join('\n');
    r.rawOutput = out.join('\n') + '\n' + r.details;
    r.durationMs = t.elapsed(); return r;
#else

    out.append(QString());
    out.append(QStringLiteral("Wired Information (table mode):"));
    out.append(QString());
    static const QVector<DiagnosticFormatter::ColSpec> kWiredCols = {
        {"Interface",   12, false},
        {"Speed",        6, true},
        {"Duplex",       6, false},
        {"MTU",          4, true},
        {"Link",         4, false},
        {"State",       10, false},
        {"MAC Address", 17, false},
    };
    QList<QStringList> wiredDataRows;

    QSet<QString> seenWired;
    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) == 0) {
        for (auto* p = ifa; p; p = p->ifa_next) {
            QString ifName = QString::fromLatin1(p->ifa_name);
            if (ifName == "lo") continue;
            if (!(p->ifa_flags & IFF_UP)) continue;
            if (seenWired.contains(ifName)) continue;
            // Skip wireless interfaces
#if defined(PLATFORM_IOS) || defined(__APPLE__)
            // iOS/macOS: classify by interface name prefix
            // (macOS has no /sys/class/net — en* prefix covers Wi-Fi adapters)
            if (ifName.startsWith("en") || ifName.startsWith("pdp_ip")) continue;
#else
            if (QFile::exists(QStringLiteral("/sys/class/net/%1/wireless").arg(ifName))) continue;
#endif
            seenWired.insert(ifName);

            auto rd = [&](const QString& prop) {
#if !defined(PLATFORM_IOS) && !defined(PLATFORM_ANDROID) && !defined(__APPLE__)
                QFile f(QStringLiteral("/sys/class/net/%1/%2").arg(ifName, prop));
                if (f.open(QIODevice::ReadOnly)) return QString::fromLatin1(f.readAll().trimmed());
#endif
                return QStringLiteral("-");
            };

            wiredDataRows.append({ifName, rd("speed"), rd("duplex"), rd("mtu"),
                rd("carrier"), rd("operstate"), rd("address")});
        }
        freeifaddrs(ifa);
    }
    out.append(DiagnosticFormatter::formatTable(kWiredCols, wiredDataRows));
    if (wiredDataRows.isEmpty()) out.append(QStringLiteral("  (no wired interfaces detected)"));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = out.size() > 3 ? DiagStatus::Pass : DiagStatus::Info;
    r.summary = wiredDataRows.isEmpty()
        ? QStringLiteral("No wired Ethernet adapters found")
        : QStringLiteral("%1 Ethernet adapter%2").arg(wiredDataRows.size()).arg(wiredDataRows.size() > 1 ? "s" : "");
    r.durationMs = t.elapsed();
    return r;
#endif
}

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// G1 闁?DHCP Status (ipconfig /all DHCP section format)
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
}
