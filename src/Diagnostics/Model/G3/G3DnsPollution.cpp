#include "Diagnostics/Model/GHelpers.h"
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
    if (ip.startsWith("127.") || ip.startsWith("10.")) return true;
    if (ip.startsWith("192.168.") || ip.startsWith("172.")) {
        // 172.16.0.0 - 172.31.255.255
        int second = ip.mid(4, ip.indexOf('.', 4) - 4).toInt();
        if (second >= 16 && second <= 31) return true;
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
    int hijackClean = 0, pollutionClean = 0;
    int hijackTimeout = 0, pollutionErrors = 0;
    QStringList hijackIPs, pollutionDetails;

    // ═══════════════════════════════════════════════════════════════════
    // Phase 1: ISP DNS Hijacking (NXDOMAIN hijack test)
    // ═══════════════════════════════════════════════════════════════════
    out.append(QStringLiteral("── Phase 1: ISP DNS Hijacking Check ──"));
    out.append(QStringLiteral("Tests whether non-existent domains resolve to IP addresses."));
    out.append(QStringLiteral("If they do, your ISP/DNS provider is hijacking NXDOMAIN."));
    out.append(QString());

    // 5WHY: Static fake domains could be whitelisted by hijacking ISPs
    // or cached by DNS servers.  Date+random-hex domains are unique per
    // run, guaranteeing no prior resolution exists — any IP returned is
    // definitive hijacking evidence.
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
    out.append(QStringLiteral("Test domains (generated fresh per run):"));
    for (int i = 0; i < 3; ++i)
        out.append(QStringLiteral("  · %1").arg(domains[i]));
    out.append(QString());

    for (const auto& domain : domains) {
        QElapsedTimer probe; probe.start();
        QString ip = DnsResolver::instance().resolve(domain, 3000);
        int elapsed = static_cast<int>(probe.elapsed());
        if (!ip.isEmpty()) {
            out.append(QStringLiteral("  %1 → RESOLVED: %2 (%3ms)")
                .arg(domain, ip).arg(elapsed));
            hijackWarn++;
            if (!hijackIPs.contains(ip)) hijackIPs.append(ip);
        } else if (elapsed >= 3000) {
            out.append(QStringLiteral("  %1 → TIMEOUT (%2ms)").arg(domain).arg(elapsed));
            hijackTimeout++;
        } else {
            out.append(QStringLiteral("  %1 → NXDOMAIN (%2ms)").arg(domain).arg(elapsed));
            hijackClean++;
        }
    }
    out.append(QString());

    // ═══════════════════════════════════════════════════════════════════
    // Phase 2: DNS Pollution (DoH multi-resolver comparison)
    // ═══════════════════════════════════════════════════════════════════
    out.append(QStringLiteral("── Phase 2: DNS Pollution Check (DoH) ──"));
    out.append(QStringLiteral("DoH results are trusted (encrypted, untampered). Detection tiers:"));
    out.append(QStringLiteral("  · Private/reserved IP → ISP hijacking"));
    out.append(QStringLiteral("  · Same /24 subnet    → CDN geo-routing (clean)"));
    out.append(QStringLiteral("  · Different /8 range  → Different org → polluted"));
    out.append(QStringLiteral("  · Same /8, no /24     → Suspicious (benefit of doubt)"));
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
        // ── DoH consensus (4 resolvers, majority vote) ──────────────
        QStringList dohIps = G1G2G3Native::dohQuery(
            QString::fromUtf8(td.domain));
        QString dohStr = dohIps.isEmpty() ? QStringLiteral("(no response)")
                        : dohIps.join(QStringLiteral(", "));

        // ── Local DNS (system resolver) ────────────────────────────
        QString localIp = DnsResolver::instance().resolve(
            QString::fromUtf8(td.domain), 3000);
        QString localStr = localIp.isEmpty() ? QStringLiteral("(no response)") : localIp;

        // ── Verdict ────────────────────────────────────────────────
        out.append(QStringLiteral("── %1 (%2) ──").arg(td.domain, td.description));
        out.append(QStringLiteral("  Trusted DoH Query Results: %1").arg(dohStr));
        out.append(QStringLiteral("  Local DNS Query Results:   %1").arg(localStr));

        if (dohIps.isEmpty()) {
            out.append(QStringLiteral("  → Inconclusive — DoH query failed"));
            pollutionErrors++;
        } else if (localIp.isEmpty()) {
            out.append(QStringLiteral("  → Local DNS failed — cannot compare"));
            pollutionErrors++;
        } else if (isPrivateIp(localIp)) {
            out.append(QStringLiteral("  → HIJACKED — local DNS returned private/reserved IP (%1)")
                .arg(localIp));
            pollutionDetails.append(QStringLiteral("%1: local=%2 (private IP)")
                .arg(td.domain, localIp));
            pollutionWarn++;
        } else if (sharesPrefix(dohIps, {localIp})) {
            out.append(QStringLiteral("  → Clean — local IP shares /24 with DoH (same CDN edge)"));
            pollutionClean++;
        } else if (differentOrg(dohIps, {localIp})) {
            out.append(QStringLiteral("  → POLLUTED — local IP (%1) in different /8 from DoH (%2)")
                .arg(localIp, dohIps.join(',')));
            pollutionDetails.append(QStringLiteral("%1: local=%2, DoH=%3")
                .arg(td.domain, localIp, dohIps.join(',')));
            pollutionWarn++;
        } else {
            out.append(QStringLiteral("  → Suspicious — same /8 but no /24 match (possible CDN, check ASN)"));
            out.append(QStringLiteral("     Local: %1, DoH: %2")
                .arg(localIp, dohIps.join(',')));
            pollutionClean++;  // give benefit of doubt
        }
        out.append(QString());
    }

    // ═══════════════════════════════════════════════════════════════════
    // Combined verdict
    // ═══════════════════════════════════════════════════════════════════
    out.append(QStringLiteral("=================================================================="));
    out.append(QStringLiteral("Phase 1 (ISP Hijack):  %1 clean, %2 hijacked, %3 timeout")
        .arg(hijackClean).arg(hijackWarn).arg(hijackTimeout));
    out.append(QStringLiteral("Phase 2 (DNS Pollution): %1 clean, %2 polluted, %3 errors")
        .arg(pollutionClean).arg(pollutionWarn).arg(pollutionErrors));

    bool hijackDetected = hijackWarn > 0;
    bool pollutionDetected = pollutionWarn > 0;

    if (hijackDetected && pollutionDetected) {
        out.append(QStringLiteral("Verdict: DNS HIJACKING + POLLUTION detected"));
        out.append(QString());
        out.append(QStringLiteral("  ISP Hijack IPs: %1").arg(hijackIPs.join(QStringLiteral(", "))));
        for (const auto& d : pollutionDetails)
            out.append(QStringLiteral("  Pollution: %1").arg(d));
        r.status = DiagStatus::Warning;
        r.summary = QStringLiteral("DNS: hijack + pollution");
    } else if (hijackDetected) {
        out.append(QStringLiteral("Verdict: ISP DNS HIJACKING detected"));
        out.append(QStringLiteral("  Redirect IPs: %1").arg(hijackIPs.join(QStringLiteral(", "))));
        r.status = DiagStatus::Warning;
        r.summary = QStringLiteral("DNS hijack: %1 IP(s)").arg(hijackIPs.size());
    } else if (pollutionDetected) {
        out.append(QStringLiteral("Verdict: DNS POLLUTION detected"));
        for (const auto& d : pollutionDetails)
            out.append(QStringLiteral("  • %1").arg(d));
        r.status = DiagStatus::Warning;
        r.summary = QStringLiteral("DNS polluted: %1/%2 domains").arg(pollutionWarn)
            .arg(pollutionWarn + pollutionClean + pollutionErrors);
    } else {
        int totalErrors = hijackTimeout + pollutionErrors;
        if (totalErrors > 0 && hijackClean + pollutionClean == 0) {
            out.append(QStringLiteral("Verdict: INCONCLUSIVE — all queries failed"));
            r.status = DiagStatus::Info;
            r.summary = QStringLiteral("DNS: all queries failed");
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
