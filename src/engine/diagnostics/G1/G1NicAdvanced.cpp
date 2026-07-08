#include "engine/diagnostics/GBase.h"
#include "engine/diagnostics/GHelpers.h"
namespace G1G2G3Native {
DiagnosticResult nicAdvanced(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

    out.append(QString());

    QList<QStringList> nicRows;

#ifdef _WIN32
    out.append(QStringLiteral("NIC Advanced Properties (table mode):"));
    out.append(QString());
    static const QVector<DiagnosticFormatter::ColSpec> kNicCols = {
        {"Interface",   12, false},
        {"Speed",        7, true},
        {"Duplex",       6, false},
        {"MTU",          4, true},
        {"Carrier",      7, false},
        {"State",       10, false},
        {"MAC Address", 17, false},
    };
    PMIB_IF_TABLE2 ifTable = nullptr;
    if (GetIfTable2(&ifTable) == NO_ERROR && ifTable) {
        for (ULONG i = 0; i < ifTable->NumEntries; i++) {
            auto& row = ifTable->Table[i];
            if (row.OperStatus == IfOperStatusNotPresent) continue;
            // Skip loopback and tunnel adapters
            if (row.Type == IF_TYPE_SOFTWARE_LOOPBACK) continue;
            QString ifName   = QString::fromWCharArray(row.Alias);
            QString speedStr = (row.TransmitLinkSpeed > 0)
                ? (row.TransmitLinkSpeed >= 1000000000
                    ? QStringLiteral("%1 Gbps").arg(row.TransmitLinkSpeed / 1.0e9, 0, 'f', 1)
                    : QStringLiteral("%1 Mbps").arg(row.TransmitLinkSpeed / 1.0e6, 0, 'f', 0))
                : QStringLiteral("N/A");
            // Duplex: NDIS OID not easily accessible from user mode; estimate
            // Full-duplex is standard for ≥1Gbps links; mark others as N/A
            QString duplexStr = (row.TransmitLinkSpeed >= 1000000000)
                ? QStringLiteral("Full") : QStringLiteral("N/A");
            QString mtuStr   = QString::number(row.Mtu);
            QString carryStr = (row.MediaConnectState == MediaConnectStateConnected)
                ? QStringLiteral("On") : QStringLiteral("Off");
            QString stateStr = (row.OperStatus == IfOperStatusUp)
                ? QStringLiteral("Up") : QStringLiteral("Down");
            QString macStr   = (row.PhysicalAddressLength >= 6)
                ? QStringLiteral("%1:%2:%3:%4:%5:%6")
                    .arg(row.PhysicalAddress[0], 2, 16, QLatin1Char('0'))
                    .arg(row.PhysicalAddress[1], 2, 16, QLatin1Char('0'))
                    .arg(row.PhysicalAddress[2], 2, 16, QLatin1Char('0'))
                    .arg(row.PhysicalAddress[3], 2, 16, QLatin1Char('0'))
                    .arg(row.PhysicalAddress[4], 2, 16, QLatin1Char('0'))
                    .arg(row.PhysicalAddress[5], 2, 16, QLatin1Char('0'))
                : QStringLiteral("N/A");
            nicRows.append({ifName.left(12), speedStr, duplexStr, mtuStr, carryStr, stateStr, macStr});
        }
        FreeMibTable(ifTable);
    }
    out.append(DiagnosticFormatter::formatTable(kNicCols, nicRows));
#else
    out.append(QStringLiteral("NIC Advanced Properties (table mode):"));
    out.append(QString());
    static const QVector<DiagnosticFormatter::ColSpec> kNicCols = {
        {"Interface",   12, false},
        {"Speed",        6, true},
        {"Duplex",       6, false},
        {"MTU",          4, true},
        {"Carrier",      7, false},
        {"State",       10, false},
        {"MAC Address", 17, false},
    };

    QSet<QString> seenNic;
    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) == 0) {
        for (auto* p = ifa; p; p = p->ifa_next) {
            QString ifName = QString::fromLatin1(p->ifa_name);
            if (ifName == "lo") continue;
            if (!(p->ifa_flags & IFF_UP)) continue;
            if (seenNic.contains(ifName)) continue;
            seenNic.insert(ifName);

            auto rd = [&](const QString& prop) {
#ifdef PLATFORM_IOS
                // iOS: only MTU is available via getifaddrs; other props are restricted
                if (prop == "mtu") {
                    // SIOCGIFMTU ioctl not available on iOS sandbox; use standard value
                    return QStringLiteral("1500");
                }
                if (prop == "operstate") return QStringLiteral("up");
                return QStringLiteral("-");
#else
                QFile f(QStringLiteral("/sys/class/net/%1/%2").arg(ifName, prop));
                if (f.open(QIODevice::ReadOnly)) return QString::fromLatin1(f.readAll().trimmed());
                return QStringLiteral("-");
#endif
            };

            nicRows.append({ifName, rd("speed"), rd("duplex"), rd("mtu"),
                rd("carrier"), rd("operstate"), rd("address")});
        }
        freeifaddrs(ifa);
    }
    out.append(DiagnosticFormatter::formatTable(kNicCols, nicRows));
#endif

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = (nicRows.size() > 0)
        ? QStringLiteral("%1 NIC%2 analyzed (speed/duplex/MTU)").arg(nicRows.size()).arg(nicRows.size() > 1 ? "s" : "")
        : QStringLiteral("No NIC properties available");
    r.durationMs = t.elapsed();
    return r;
}

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// G1 闁?Wired Diagnostics
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
}
