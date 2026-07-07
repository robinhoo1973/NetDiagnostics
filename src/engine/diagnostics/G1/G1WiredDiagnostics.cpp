#include "engine/diagnostics/GBase.h"
#include "engine/diagnostics/GHelpers.h"
namespace G1G2G3Native {
DiagnosticResult wiredDiagnostics(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

#ifdef PLATFORM_IOS
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

#ifdef _WIN32
    out.append(QString());
    out.append(QStringLiteral("(use wmic nic get on Windows)"));
    r.rawOutput = out.join('\n'); r.details = r.rawOutput;
    r.status = DiagStatus::Info; r.summary = QStringLiteral("Windows wired diagnostics delegate to wmic");
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
#ifdef PLATFORM_IOS
            // iOS: classify by interface name prefix
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
    r.summary = QStringLiteral("Wired diagnostics complete");
    r.durationMs = t.elapsed();
    return r;
#endif
}

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// G1 闁?DHCP Status (ipconfig /all DHCP section format)
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
}
