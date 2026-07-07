#include "engine/diagnostics/G1G2G3Native.h"
#include "engine/diagnostics/GHelpers.h"
namespace G1G2G3Native {
DiagnosticResult cellularInfo(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G1;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Cellular Information:"));
    out.append(QString());

#ifdef PLATFORM_IOS
    QVariantMap cell = iosCellularInfo();
    const bool hasCellIdentity = hasCellularIdentity(cell);
    const QString cellIp = iosInterfaceIPv4(QStringLiteral("pdp_ip0"));
    const QString cellGw = iosGatewayForInterface(QStringLiteral("pdp_ip0"));
    const QVariantList sims = cell.value(QStringLiteral("sims")).toList();
    const bool multiSim = sims.size() > 1;
    if (hasCellIdentity || !sims.isEmpty()) {
        if (multiSim)
            out.append(QStringLiteral("  %1 SIM / eSIM lines active:").arg(sims.size()));
        for (const QVariant& v : sims) {
            const QVariantMap sim = v.toMap();
            const QString pad = multiSim ? QStringLiteral("    ") : QStringLiteral("  ");
            if (multiSim)
                out.append(QStringLiteral("  SIM %1:").arg(sim.value(QStringLiteral("slot")).toInt()));
            const QString carrier = sim.value(QStringLiteral("carrierName")).toString();
            out.append(QStringLiteral("%1Carrier: %2").arg(pad,
                carrier.isEmpty() ? QStringLiteral("(hidden by iOS 16+)") : carrier));
            const QString radio = sim.value(QStringLiteral("radioAccess")).toString();
            if (!radio.isEmpty())
                out.append(QStringLiteral("%1Radio Access: %2").arg(pad, radio));
        }
        out.append(QStringLiteral("  %1: %2")
            .arg(multiSim ? QStringLiteral("Data IP (active line)") : QStringLiteral("IP Address"),
                 cellIp.isEmpty() ? QStringLiteral("(not assigned)") : cellIp));
        if (!cellGw.isEmpty())
            out.append(QStringLiteral("  Gateway: %1").arg(cellGw));
        if (hasNonEmptyValue(cell, "signalNotice"))
            out.append(QStringLiteral("  Signal: %1").arg(cell["signalNotice"].toString()));
        out.append(QString());
        r.status = DiagStatus::Pass;
        if (multiSim) {
            QStringList rats;
            for (const QVariant& v : sims) {
                const QString ra = v.toMap().value(QStringLiteral("radioAccess")).toString();
                rats.append(ra.isEmpty() ? QStringLiteral("\u2014") : ra);
            }
            r.summary = QStringLiteral("%1 SIMs (%2)").arg(sims.size()).arg(rats.join(QStringLiteral(", ")));
        } else {
            r.summary = cellularSummary(cell);
        }
    } else {
        out.append(QStringLiteral("  No cellular service available"));
        if (!cellIp.isEmpty())
            out.append(QStringLiteral("  IP Address: %1").arg(cellIp));
        if (!cellGw.isEmpty())
            out.append(QStringLiteral("  Gateway: %1").arg(cellGw));
        if (hasNonEmptyValue(cell, "signalNotice"))
            out.append(QStringLiteral("  Signal: %1").arg(cell["signalNotice"].toString()));
        r.status = DiagStatus::Info; r.summary = QStringLiteral("No cellular service");
    }
#else
    out.append(QStringLiteral("  [Skipped] Cellular info requires iOS — not applicable on this platform"));
    r.status = DiagStatus::Skipped; r.summary = QStringLiteral("Not applicable (iOS only)");
#endif
    out.append(QString());
    r.rawOutput = out.join('\n'); r.details = r.rawOutput;
    return r;
}

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// G1 闁?IP Configuration (Windows ipconfig /all format 1:1)
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
}
