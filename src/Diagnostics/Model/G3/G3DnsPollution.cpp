#include "Diagnostics/Model/GHelpers.h"
#include "Diagnostics/Model/G3/G3DnsIntegrity.h"
#include "Common/Services/DnsResolver.h"
#include <QCryptographicHash>

namespace G1G2G3Native {

// ── DNS pollution detection helpers ─────────────────────────────────
// Best practices (2024-2026 research + industry standards):
//   1. DoH results are TRUSTED (encrypted, cannot be tampered mid-path).
//   2. Private/reserved IP from local DNS = ISP hijacking.
//   3. /24 match = same CDN edge = clean (CDN geo-routing, not pollution).
//   4. /8 mismatch = different org/AS = polluted (GFW injects foreign IPs).
//   5. Otherwise = suspicious (inconclusive, may need manual check).
static QString prefix(int n, const QString& ip) {
    int dots = 0, i = 0;
    for (; i < ip.length() && dots < n; ++i)
        if (ip[i] == '.') ++dots;
    return (dots == n) ? ip.left(i - 1) : ip;
}

static bool isPrivateIp(const QString& ip) {
    // RFC 1918 + loopback + CGNAT + special-use
    if (ip.startsWith("127.") || ip.startsWith("10.")) return true;
    if (ip.startsWith("192.168.")) return true;
    if (ip.startsWith("172.")) {
        // 172.16.0.0 – 172.31.255.255 (RFC 1918)
        int dot = ip.indexOf('.', 4);
        if (dot > 4) {
            int second = ip.mid(4, dot - 4).toInt();
            if (second >= 16 && second <= 31) return true;
        }
    }
    if (ip.startsWith("100.")) {
        // 100.64.0.0 – 100.127.255.255 (RFC 6598 CGNAT)
        int dot = ip.indexOf('.', 4);
        if (dot > 4) {
            int second = ip.mid(4, dot - 4).toInt();
            if (second >= 64 && second <= 127) return true;
        }
    }
    if (ip == "0.0.0.0") return true;
    return false;
}

static bool sharesPrefix(const QStringList& a, const QStringList& b) {
    for (const auto& ipA : a) {
        QString p24 = prefix(3, ipA);
        for (const auto& ipB : b) {
            if (prefix(3, ipB) == p24) return true;
        }
    }
    return false;
}

static bool differentOrg(const QStringList& a, const QStringList& b) {
    // Different /8 = different organization → strong pollution signal
    for (const auto& ipA : a) {
        QString p8 = prefix(1, ipA);
        for (const auto& ipB : b) {
            if (prefix(1, ipB) == p8) return false;
        }
    }
    return !a.isEmpty() && !b.isEmpty();
}

DiagnosticResult dnsPollution(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

    out.append(QStringLiteral("DNS Pollution & Hijacking Check"));
    out.append(QStringLiteral("================================"));
    out.append(QString());

    int hijackWarn = 0, pollutionWarn = 0;
    int hijackClean = 0, pollutionClean = 0, pollutionSuspicious = 0;
    int hijackTimeout = 0, pollutionErrors = 0;
    QStringList hijackIPs, pollutionDetails;

    // ═══════════════════════════════════════════════════════════════════
    // Phase 1: ISP DNS Hijacking (NXDOMAIN hijack test)
    // ═══════════════════════════════════════════════════════════════════
    out.append(QStringLiteral("── Phase 1: ISP DNS Hijacking ──"));
    out.append(QStringLiteral("Fake Domains That Resolve to an IP Indicate ISP DNS Hijacking."));
    out.append(QString());

    // 5WHY: Static fake domains could be whitelisted by hijacking ISPs
    // or cached by DNS servers.  Date+hash-hex domains are unique per
    // day per index, making preemptive whitelisting impractical — any
    // IP returned is definitive hijacking evidence.
    // Format: YYYYMMDD-xxxx-dns-test.{com,org,net}
    QString datePrefix = QDateTime::currentDateTime().toString("yyyyMMdd");
    QString domains[3];
    for (int i = 0; i < 3; ++i) {
        QByteArray seed = QStringLiteral("%1-%2-dns-hijack-test")
            .arg(datePrefix).arg(i).toUtf8();
        QString hex = QString::fromUtf8(
            QCryptographicHash::hash(seed, QCryptographicHash::Md5).toHex().left(4));
        static const char* tlds[] = {"com", "org", "net"};
        domains[i] = QStringLiteral("%1-%2-dns-test.%3")
            .arg(datePrefix, hex, tlds[i]);
    }

    for (const auto& domain : domains) {
        QElapsedTimer probe; probe.start();
        QString ip = DnsResolver::instance().resolve(domain, 3000);
        int elapsed = static_cast<int>(probe.elapsed());
        if (!ip.isEmpty()) {
            out.append(QStringLiteral("  %1 → HIJACKED: %2 (%3ms)").arg(domain, ip).arg(elapsed));
            hijackWarn++;
            if (!hijackIPs.contains(ip)) hijackIPs.append(ip);
        } else if (elapsed >= 3000) {
            out.append(QStringLiteral("  %1 → TIMEOUT (%2ms)").arg(domain).arg(elapsed));
            hijackTimeout++;
        } else {
            out.append(QStringLiteral("  %1 → Not Resolved").arg(domain));
            hijackClean++;
        }
    }
    out.append(QString());

    // ═══════════════════════════════════════════════════════════════════
    // Phase 2: DNS Pollution (multi-signal integrity check)
    // ═══════════════════════════════════════════════════════════════════
    out.append(QStringLiteral("── Phase 2: DNS Integrity Check ──"));
    out.append(QStringLiteral("Multi-Signal Cross-Verification: CNAME Chain, TTL, Response Timing, and TLS Certificate."));
    out.append(QString());

    static const struct {
        const char* domain;
        const char* description;
    } kTestDomains[] = {
        {"www.google.com",    "Search Engine"},
        {"www.youtube.com",   "Video Platform"},
        {"www.telegram.org",  "Messaging"},
        {"www.bbc.com",       "News Media"},
        {"www.wikipedia.org", "Knowledge Base"},
    };

    for (const auto& td : kTestDomains) {
        QElapsedTimer probe; probe.start();

        // ── DoH full-record query (CNAME + TTL + IPs) ────────────
        DohFullResult doh = G1G2G3Native::dohQueryFull(
            QString::fromUtf8(td.domain));

        // ── Local DNS (UDP, system resolver) ─────────────────────
        QString localUdpIp = DnsResolver::instance().resolve(
            QString::fromUtf8(td.domain), 3000);
        int localMs = static_cast<int>(probe.elapsed());

        // ── Score with multi-signal engine ───────────────────────
        DnsIntegrityResult ir = scoreDnsIntegrity(
            QString::fromUtf8(td.domain), td.description,
            doh, localUdpIp, localMs);

        // ── Handle DoH/local failures ────────────────────────────
        if (doh.aRecords.isEmpty()) {
            out.append(QStringLiteral("  %1 (%2) — DoH query failed, skipped")
                .arg(td.domain, td.description));
            pollutionErrors++;
        } else if (localUdpIp.isEmpty()) {
            out.append(QStringLiteral("  %1 (%2) — Local DNS failed, skipped")
                .arg(td.domain, td.description));
            pollutionErrors++;
        } else if (isPrivateIp(localUdpIp)) {
            // Private IP override — definitive hijacking regardless of score
            out.append(QStringLiteral("  %1 (%2) — Local=%3 → HIJACKED (private IP)")
                .arg(td.domain, td.description, localUdpIp));
            pollutionDetails.append(QStringLiteral("%1: local=%2 (private IP)")
                .arg(td.domain, localUdpIp));
            pollutionWarn++;
        } else {
            // ── Use multi-signal scored output ─────────────────
            for (const auto& line : ir.output)
                out.append(line);

            switch (ir.verdict) {
            case DnsIntegrityResult::Clean:
                pollutionClean++; break;
            case DnsIntegrityResult::Suspicious:
                pollutionSuspicious++; break;
            case DnsIntegrityResult::Polluted:
                pollutionWarn++;
                pollutionDetails.append(QStringLiteral("%1: score=%2%, DoH=%3, Local=%4")
                    .arg(td.domain).arg(ir.scorePercent)
                    .arg(ir.dohIps.join(','), ir.localUdpIp));
                break;
            case DnsIntegrityResult::Hijacked:
                pollutionWarn++;
                pollutionDetails.append(QStringLiteral("%1: HIJACKED (score=%2%)")
                    .arg(td.domain).arg(ir.scorePercent));
                break;
            }
        }
        out.append(QString());
    }

    // ═══════════════════════════════════════════════════════════════════
    // Combined verdict
    // ═══════════════════════════════════════════════════════════════════
    out.append(QStringLiteral("=================================================================="));
    out.append(QStringLiteral("Phase 1 (ISP Hijack):  %1 clean, %2 hijacked, %3 timeout")
        .arg(hijackClean).arg(hijackWarn).arg(hijackTimeout));
    {
        QString s = QStringLiteral("Phase 2 (DNS Pollution): %1 clean, %2 polluted, %3 errors")
            .arg(pollutionClean).arg(pollutionWarn).arg(pollutionErrors);
        if (pollutionSuspicious > 0)
            s += QStringLiteral(", %1 suspicious").arg(pollutionSuspicious);
        out.append(s);
    }

    bool hijackDetected = hijackWarn > 0;
    bool pollutionDetected = pollutionWarn > 0;
    bool phase2AllFailed = (pollutionErrors > 0 && pollutionClean == 0
                            && pollutionWarn == 0 && pollutionSuspicious == 0);

    if (hijackDetected && pollutionDetected) {
        out.append(QStringLiteral("Verdict: DNS HIJACKING + POLLUTION detected"));
        out.append(QString());
        out.append(QStringLiteral("  ISP Hijack IPs: %1").arg(hijackIPs.join(QStringLiteral(", "))));
        for (const auto& d : pollutionDetails)
            out.append(QStringLiteral("  • %1").arg(d));
        r.status = DiagStatus::Warning;
        r.summary = QStringLiteral("DNS: hijack + pollution");
    } else if (hijackDetected) {
        out.append(QStringLiteral("Verdict: ISP DNS HIJACKING detected"));
        out.append(QStringLiteral("  Hijack IPs: %1").arg(hijackIPs.join(QStringLiteral(", "))));
        r.status = DiagStatus::Warning;
        r.summary = QStringLiteral("DNS hijack: %1 IP(s)").arg(hijackIPs.size());
    } else if (pollutionDetected) {
        int phase2Total = pollutionWarn + pollutionClean + pollutionSuspicious + pollutionErrors;
        out.append(QStringLiteral("Verdict: DNS POLLUTION — %1/%2 domains affected")
            .arg(pollutionWarn).arg(phase2Total));
        r.status = DiagStatus::Warning;
        r.summary = QStringLiteral("DNS polluted: %1/%2 domains").arg(pollutionWarn)
            .arg(phase2Total);
    } else {
        int totalErrors = hijackTimeout + pollutionErrors;
        if (totalErrors > 0 && hijackClean + pollutionClean + pollutionSuspicious == 0) {
            out.append(QStringLiteral("Verdict: INCONCLUSIVE — all queries failed"));
            r.status = DiagStatus::Info;
            r.summary = QStringLiteral("DNS: all queries failed");
        } else if (phase2AllFailed) {
            out.append(QStringLiteral("Verdict: Phase 1 clean, Phase 2 inconclusive (all DoH queries failed)"));
            r.status = DiagStatus::Info;
            r.summary = QStringLiteral("DNS: hijack clean, pollution inconclusive");
        } else if (pollutionSuspicious > 0) {
            out.append(QStringLiteral("Verdict: SUSPICIOUS — %1 domain(s) need manual check")
                .arg(pollutionSuspicious));
            r.status = DiagStatus::Info;
            r.summary = QStringLiteral("DNS: %1 suspicious").arg(pollutionSuspicious);
        } else {
            out.append(QStringLiteral("Verdict: DNS CLEAN — no hijacking or pollution detected"));
            r.status = DiagStatus::Pass;
            r.summary = QStringLiteral("DNS clean");
        }
    }

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.durationMs = t.elapsed();
    return r;
}

} // namespace G1G2G3Native
