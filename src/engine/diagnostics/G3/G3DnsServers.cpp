#include "engine/diagnostics/GHelpers.h"

namespace G1G2G3Native {
DiagnosticResult dnsServers(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QStringList out, dnsList;
    out.append(QString());
    out.append(QStringLiteral("DNS Server Configuration (table mode):"));
    out.append(QString());

    static const QVector<DiagnosticFormatter::ColSpec> kDnsCols = {
        {"Source",   20, false},
        {"DNS Server", 0, false},
    };
    QList<QStringList> dnsRows;

#ifdef _WIN32
    ULONG bufLen = 15000;
    QByteArray buf(bufLen, '\0');
    PIP_ADAPTER_ADDRESSES adapters = (PIP_ADAPTER_ADDRESSES)buf.data();
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_UNICAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_ANYCAST, nullptr, adapters, &bufLen) == NO_ERROR) {
        for (auto* a = adapters; a; a = a->Next) {
            QString ifName = QString::fromWCharArray(a->FriendlyName);
            for (auto* dns = a->FirstDnsServerAddress; dns; dns = dns->Next) {
                char ip[64]; DWORD ipLen = sizeof(ip);
                WSAAddressToStringA(dns->Address.lpSockaddr, dns->Address.iSockaddrLength, nullptr, ip, &ipLen);
                QString ipStr = QString::fromLatin1(ip);
                dnsRows.append({ifName, ipStr});
                if (!dnsList.contains(ipStr)) dnsList.append(ipStr);
            }
        }
    }
#else
#ifdef PLATFORM_IOS
    // iOS: no /etc/resolv.conf 闁?use res_ninit
    struct __res_state res; memset(&res, 0, sizeof(res));
    if (res_ninit(&res) == 0) {
        for (int i = 0; i < res.nscount; i++) {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &res.nsaddr_list[i].sin_addr, ip, sizeof(ip));
            dnsRows.append({QStringLiteral("System DNS"), QString::fromLatin1(ip)});
            dnsList.append(QString::fromLatin1(ip));
        }
        res_nclose(&res);
    }
#else
    // Read /etc/resolv.conf
    QFile resolv(QStringLiteral("/etc/resolv.conf"));
    if (resolv.open(QIODevice::ReadOnly)) {
        QTextStream ts(&resolv);
        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            if (line.startsWith("nameserver ")) {
                QString ns = line.mid(11);
                dnsRows.append({QStringLiteral("resolv.conf"), ns});
                if (!dnsList.contains(ns)) dnsList.append(ns);
            }
            else if (line.startsWith("search "))
                dnsRows.append({QStringLiteral("search domains"), line.mid(7)});
        }
    }
    // Also check systemd-resolved stub
    QFile stub(QStringLiteral("/run/systemd/resolve/resolv.conf"));
    if (stub.open(QIODevice::ReadOnly)) {
        dnsRows.append({QStringLiteral("systemd-resolved"), QStringLiteral("(stub resolver active)")});
    }
#endif
#endif

    if (!dnsRows.isEmpty())
        out.append(DiagnosticFormatter::formatTable(kDnsCols, dnsRows));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = DiagStatus::Pass;
    r.summary = dnsList.isEmpty() ? QStringLiteral("No DNS servers found")
                 : QStringLiteral("DNS: %1").arg(dnsList.join(QStringLiteral(", ")));
    r.durationMs = 0;
    return r;
}

}
