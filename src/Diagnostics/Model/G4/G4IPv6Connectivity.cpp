// =============================================================================
// G4IPv6Connectivity.cpp -- IPv6 connectivity diagnostic
//
// 5WHY: All existing G4/G5 tests hardcode AF_INET (IPv4-only).  This test
// provides basic IPv6 connectivity verification: DNS AAAA resolution +
// TCP connect to well-known IPv6-enabled services.  Uses the newly added
// DnsResolver::resolve6() and NetUtil::tcpConnect6() helpers.
// =============================================================================
#include "Diagnostics/Model/G4/G4Common.h"
#include "Common/Utils/NetUtil.h"

DiagnosticResult ipv6Connectivity(const QString& target) {
    DiagnosticResult r;
    r.id = DiagId::G4IPv6Connectivity; r.group = DiagGroup::G4;
    r.timestamp = QDateTime::currentDateTime();

    QString host = target.isEmpty() ? QStringLiteral("ipv6.google.com") : extractHostname(target);

    QElapsedTimer t; t.start();
    QStringList out;
    out.append(QStringLiteral("IPv6 Connectivity Test"));
    out.append(QStringLiteral("Target: %1").arg(host));
    out.append(QString());

    // Phase 1: DNS AAAA resolution
    QString ipv6 = DnsResolver::instance().resolve6(host, 5000);
    if (ipv6.isEmpty()) {
        out.append(QStringLiteral("DNS AAAA resolution FAILED -- no IPv6 address for %1").arg(host));
        out.append(QStringLiteral("Possible causes:"));
        out.append(QStringLiteral("  - No IPv6 DNS server configured"));
        out.append(QStringLiteral("  - Target has no AAAA record"));
        out.append(QStringLiteral("  - DNS timeout or network issue"));
        r.rawOutput = out.join('\n');
        r.details = r.rawOutput;
        r.durationMs = t.elapsed();
        r.status = DiagStatus::Warning;
        r.summary = QStringLiteral("No IPv6 DNS resolution");
        return r;
    }
    out.append(QStringLiteral("DNS AAAA: %1 → %2").arg(host, ipv6));

    // Phase 2: TCP connect to common IPv6 ports
    static const struct { int port; const char* name; } kTestPorts[] = {
        {80, "HTTP"}, {443, "HTTPS"}, {22, "SSH"},
    };
    int connected = 0, failed = 0;
    for (const auto& tp : kTestPorts) {
        int sock = tcpConnect6(host, tp.port, 3000);
        if (sock >= 0) {
            out.append(QStringLiteral("TCP/%1 (%2): CONNECTED").arg(tp.port).arg(tp.name));
            closeSocket(sock);
            connected++;
        } else {
            out.append(QStringLiteral("TCP/%1 (%2): FAILED").arg(tp.port).arg(tp.name));
            failed++;
        }
    }

    out.append(QString());
    out.append(QStringLiteral("Result: %1/%2 ports reachable via IPv6").arg(connected).arg(connected + failed));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.durationMs = t.elapsed();

    if (connected > 0) {
        r.status = DiagStatus::Pass;
        r.summary = QStringLiteral("IPv6 reachable (%1/%2 ports)").arg(connected).arg(connected + failed);
    } else {
        r.status = DiagStatus::Fail;
        r.summary = QStringLiteral("IPv6 unreachable (0/%1 ports)").arg(connected + failed);
    }
    return r;
}

} // namespace G4RemoteHost
