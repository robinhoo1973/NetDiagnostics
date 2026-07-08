#include "engine/diagnostics/GBase.h"
#include "engine/diagnostics/GHelpers.h"
namespace G1G2G3Native {
DiagnosticResult nicAdvanced(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

    out.append(QString());
#ifdef _WIN32
    out.append(QStringLiteral("NIC Advanced Properties:"));
    // ... Windows-specific registry queries would go here
    out.append(QStringLiteral("  (use wmic nic / Device Manager for full details)"));
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
    QList<QStringList> nicRows;

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
