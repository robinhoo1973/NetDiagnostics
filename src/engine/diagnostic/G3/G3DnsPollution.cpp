#include "engine/diagnostic/GHelpers.h"
#include "engine/diagnostic/SpeedTest.h"
namespace G1G2G3Native {
DiagnosticResult dnsPollution(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

    out.append(QString());
    out.append(QStringLiteral("DNS Pollution / Hijacking Check"));
    out.append(QStringLiteral("================================="));
    out.append(QString());
    out.append(QStringLiteral("Tests whether non-existent domains resolve to IP addresses."));
    out.append(QStringLiteral("If they do, your DNS provider is redirecting NXDOMAIN responses"));
    out.append(QStringLiteral("(DNS hijacking / DNS pollution). A clean resolver returns NXDOMAIN."));
    out.append(QString());

    // Show current DNS server
    QFile resolv(QStringLiteral("/etc/resolv.conf"));
    if (resolv.open(QIODevice::ReadOnly)) {
        QTextStream ts(&resolv);
        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            if (line.startsWith("nameserver ")) {
                out.append(QStringLiteral("DNS Server: %1").arg(line.mid(11)));
                break;
            }
        }
    }
    out.append(QString());
    out.append(QStringLiteral("  %1  %2  %3")
        .arg(QStringLiteral("Test Domain"), -40)
        .arg(QStringLiteral("Result"), -16)
        .arg(QStringLiteral("Response")));
    out.append(QStringLiteral("  %1  %2  %3")
        .arg(QString(40, '-'))
        .arg(QString(16, '-'))
        .arg(QString(20, '-')));

    struct { const char* domain; } testCases[] = {
        {"thisdomainshouldnotexist12345.com"},
        {"nonexistent-test-domain-98765.org"},
        {"definitely-not-real-domain-42.net"},
    };

    int resolved = 0, clean = 0, timedOut = 0;
    QStringList hijackIPs;
    for (auto& tc : testCases) {
        QElapsedTimer probe; probe.start();
        QString ip = DnsResolver::instance().resolve(tc.domain, 4000);
        int elapsed = static_cast<int>(probe.elapsed());
        if (!ip.isEmpty()) {
            out.append(QStringLiteral("  %1  %2  %3")
                .arg(tc.domain, -40).arg(QStringLiteral("RESOLVED"), -16).arg(QStringLiteral("%1 (%2 ms)").arg(ip).arg(elapsed)));
            resolved++;
            if (!hijackIPs.contains(ip)) hijackIPs.append(ip);
        } else if (elapsed >= 4000) {
            out.append(QStringLiteral("  %1  %2  %3")
                .arg(tc.domain, -40).arg(QStringLiteral("TIMEOUT"), -16).arg(QStringLiteral("%1 ms").arg(elapsed)));
            timedOut++;
        } else {
            out.append(QStringLiteral("  %1  %2  %3")
                .arg(tc.domain, -40).arg(QStringLiteral("NXDOMAIN"), -16).arg(QStringLiteral("%1 ms").arg(elapsed)));
            clean++;
        }
    }

    out.append(QString());
    out.append(QStringLiteral("------------------------------------------------------------------"));
    out.append(QStringLiteral("Results: %1 resolved, %2 clean, %3 timed out")
        .arg(resolved).arg(clean).arg(timedOut));
    if (resolved > 0) {
        out.append(QStringLiteral("Verdict: DNS HIJACKING DETECTED — non-existent domains redirected to:"));
        for (const auto& ip : hijackIPs) out.append(QStringLiteral("  • %1").arg(ip));
        out.append(QString());
        out.append(QStringLiteral("This typically means your ISP or DNS provider is intercepting"));
        out.append(QStringLiteral("NXDOMAIN responses and redirecting to a search/advertising page."));
    } else if (timedOut > 0) {
        out.append(QStringLiteral("Verdict: INCONCLUSIVE — %1 probes timed out (DNS may be slow or filtered)").arg(timedOut));
    } else {
        out.append(QStringLiteral("Verdict: DNS CLEAN — no hijacking detected"));
    }

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.status = resolved > 0 ? DiagStatus::Warning : (timedOut > 0 ? DiagStatus::Info : DiagStatus::Pass);
    r.summary = resolved > 0 ? QStringLiteral("DNS hijack: %1 IPs").arg(hijackIPs.size())
               : timedOut > 0 ? QStringLiteral("DNS: %1 timeout(s)").arg(timedOut)
               : QStringLiteral("DNS clean");
    r.durationMs = t.elapsed();
    return r;
}

// internetConnectivity() removed 闁?merged into speedTest() (Phase 0 connectivity check)

// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// Speed Test 闁?Speedtest.net protocol (Ookla-compatible)
// Mimics "speedtest-cli" output format
// =============================================================================
// Protocol:
//   1. GET /api/js/servers 闁?JSON server list
//   2. TCP ping each candidate 闁?pick lowest latency
//   3. GET {url}/download?size=N 闁?measure download throughput
//   4. POST {url}/upload 闁?measure upload throughput
// 闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍鐑樺姀閺呮煡鍩￠幇銊︽珳闁崇儤鍔忛弲鏌ュ煛閹般劍娅滈柍?
// Convert host (name or IP) to sockaddr_in 闁?returns true on success

// 闁冲厜鍋撻柍鍏夊亾 Simple HTTP GET via raw socket (no Qt event loop needed) 闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾闁冲厜鍋撻柍鍏夊亾
}
