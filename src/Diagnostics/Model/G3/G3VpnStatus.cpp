#include "Diagnostics/Model/G3/G3InternetDns.h"
#include "Diagnostics/Model/GHelpers.h"

namespace G1G2G3Native {

// ── VPN Status Detection ──────────────────────────────────────────────
// 5WHY: Speed-test server selection needs to know if the user is behind
// a VPN to correctly prioritize CN vs global servers. This test runs
// BEFORE InternetSpeedTest to inform the speed-test pipeline.
//
// Classification:
//   A — No VPN, in China            (GeoIP=CN, connChina≈4, CN latency low)
//   B — In China behind VPN abroad   (GeoIP≠CN, connChina≈4, CN latency low)
//   C — Overseas behind CN VPN       (GeoIP=CN, connChina≈0-1, CN latency high)
//   D — No VPN, overseas             (GeoIP≠CN, connChina≈0-1, CN latency high)

DiagnosticResult vpnStatus(DiagId id) {
    DiagnosticResult r;
    r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;
    out.append(QStringLiteral("VPN Status Detection"));
    out.append(QStringLiteral("Determines if user is behind a VPN based on:"));
    out.append(QStringLiteral("  - GeoIP country detection"));
    out.append(QStringLiteral("  - Connectivity to Chinese sites"));
    out.append(QStringLiteral("  - TCP latency to Chinese speed-test servers"));
    out.append(QString());

    // ── Step 1: Connectivity check ─────────────────────────────────
    // 5WHY: strstr(s.name, "(CN)") tied classification to a display label,
    // making it fragile against name changes.  Now uses a struct bool.
    struct { const char* host; int port; const char* name; bool isChina; } sites[] = {
        {"223.5.5.5", 53, "Alibaba DNS", true},
        {"baidu.com", 443, "Baidu", true},
        {"119.29.29.29", 53, "DNSPod DNS", true},
        {"8.8.8.8", 53, "Google DNS", false},
        {"1.1.1.1", 53, "Cloudflare DNS", false},
    };
    int connChina = 0, connGlobal = 0;
    out.append(QStringLiteral("--- Connectivity Check -------------------------------------------------"));
    for (auto& s : sites) {
        int lat = tcpPingMs(s.host, s.port);
        bool ok = (lat >= 0);
        if (ok) {
            if (s.isChina) connChina++; else connGlobal++;
        }
        out.append(QStringLiteral("  %1  %2:%3  %4  %5 ms")
            .arg(QString::fromUtf8(s.name).leftJustified(22, ' '))
            .arg(s.host).arg(s.port)
            .arg(ok ? QStringLiteral("[OK]") : QStringLiteral("[FAIL]"))
            .arg(ok ? QString::number(lat) : QStringLiteral("-")));
    }
    out.append(QStringLiteral("  CN reachable: %1/3  Global reachable: %2/2").arg(connChina).arg(connGlobal));

    // ── Step 2: GeoIP detection ──────────────────────────────────
    QString country = SpeedTest::detectCountry(3000);
    out.append(QString());
    out.append(QStringLiteral("Detected country: %1").arg(country == "XX" ? "Unknown" : country));

    // ── Step 3: CN server TCP latency probe ───────────────────────
    struct { const char* host; int port; } cnProbes[] = {
        {"speedtest1.gd.chinamobile.com", 8080},
        {"speedtest.bj.chinamobile.com", 8080},
        {"speedtest1.online.sh.cn", 8080},
    };
    int cnLatencyMs = -1;
    int cnReachable = 0;
    for (auto& p : cnProbes) {
        int lat = tcpPingMs(p.host, p.port);
        if (lat >= 0) {
            cnReachable++;
            if (cnLatencyMs < 0 || lat < cnLatencyMs) cnLatencyMs = lat;
        }
    }
    out.append(QStringLiteral("CN speed-test servers reachable: %1/3 (best: %2 ms)")
        .arg(cnReachable).arg(cnLatencyMs >= 0 ? QString::number(cnLatencyMs) : QStringLiteral("N/A")));

    // ── Step 4: Classify ──────────────────────────────────────────
    QString scenario;
    QString details;
    DiagStatus status = DiagStatus::Pass;

    // ── Step 4: Classify ──────────────────────────────────────────
    // 5WHY: cnLatencyMs=-1 with country=CN → scenario C.
    // 5WHY: country=XX (GeoIP failed) was treated as non-CN, causing B
    // misclassification.  Use connChina≥3 as primary signal for CN presence.
    if (country == QStringLiteral("XX")) {
        if (connChina >= 3 && cnLatencyMs >= 0 && cnLatencyMs < 50) {
            scenario = QStringLiteral("B — In China behind VPN abroad (GeoIP failed)");
            details = QStringLiteral("GeoIP failed but %1/3 CN sites reachable, CN latency %2ms")
                .arg(connChina).arg(cnLatencyMs);
            status = DiagStatus::Warning;
        } else if (connChina <= 1) {
            scenario = QStringLiteral("D — No VPN, overseas (GeoIP failed)");
            details = QStringLiteral("GeoIP failed, %1/3 CN sites reachable, %2/2 global reachable")
                .arg(connChina).arg(connGlobal);
        } else {
            scenario = QStringLiteral("Uncertain — GeoIP failed, %1/3 CN sites reachable")
                .arg(connChina);
            details = QStringLiteral("Recommend manually checking VPN status");
            status = DiagStatus::Info;
        }
    } else if (country == QStringLiteral("CN")) {
        if (connChina >= 2 && cnLatencyMs >= 0 && cnLatencyMs < 50) {
            scenario = QStringLiteral("A — No VPN, inside China");
            details = QStringLiteral("GeoIP=CN, %1/3 CN sites reachable, CN server latency %2ms")
                .arg(connChina).arg(cnLatencyMs);
        } else if (cnLatencyMs > 100 || (cnLatencyMs < 0 && connChina <= 1)) {
            scenario = QStringLiteral("C — Overseas behind CN VPN");
            details = cnLatencyMs > 100
                ? QStringLiteral("GeoIP=CN but only %1/3 CN sites reachable, CN latency %2ms (high)")
                    .arg(connChina).arg(cnLatencyMs)
                : QStringLiteral("GeoIP=CN but all CN servers unreachable, only %1/3 CN sites")
                    .arg(connChina);
            status = DiagStatus::Warning;
        } else if (connChina <= 1 && cnLatencyMs >= 0 && cnLatencyMs < 50) {
            scenario = QStringLiteral("A — No VPN, inside China (partial connectivity)");
            details = QStringLiteral("GeoIP=CN, %1/3 CN sites reachable (degraded), CN latency %2ms")
                .arg(connChina).arg(cnLatencyMs);
        } else {
            scenario = QStringLiteral("A — No VPN, inside China (uncertain)");
            details = QStringLiteral("GeoIP=CN, %1/3 CN sites, latency %2ms").arg(connChina)
                .arg(cnLatencyMs >= 0 ? QString::number(cnLatencyMs) : "N/A");
        }
    } else {
        if (connChina >= 3 && cnLatencyMs >= 0 && cnLatencyMs < 50) {
            scenario = QStringLiteral("B — In China behind VPN abroad");
            details = QStringLiteral("GeoIP=%1 but %2/3 CN sites reachable, CN latency %3ms (low)")
                .arg(country).arg(connChina).arg(cnLatencyMs);
            status = DiagStatus::Warning;
        } else if (connChina <= 1) {
            scenario = QStringLiteral("D — No VPN, overseas");
            details = QStringLiteral("GeoIP=%1, %2/3 CN sites reachable (expected for non-CN)")
                .arg(country).arg(connChina);
        } else {
            scenario = QStringLiteral("D — No VPN, overseas (with partial CN access)");
            details = QStringLiteral("GeoIP=%1, %2/3 CN sites reachable").arg(country).arg(connChina);
        }
    }

    out.append(QString());
    out.append(QStringLiteral("--- Result -----------------------------------------------------------------"));
    out.append(QStringLiteral("  Scenario: %1").arg(scenario));
    out.append(QStringLiteral("  Details:  %1").arg(details));
    out.append(QStringLiteral("  Recommendation: %1")
        .arg(scenario.startsWith('A') || scenario.startsWith('B')
            ? QStringLiteral("Prioritize CN speed-test servers")
            : QStringLiteral("Prioritize global speed-test servers")));

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.summary = scenario.left(3); // "A —" / "B —" etc
    r.status = status;
    r.durationMs = t.elapsed();
    return r;
}

} // namespace G1G2G3Native
