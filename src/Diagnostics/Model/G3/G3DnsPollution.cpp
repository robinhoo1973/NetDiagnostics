#include "Diagnostics/Model/GHelpers.h"
#include "Common/Services/DnsResolver.h"

namespace G1G2G3Native {

// ── /24 subnet prefix — industry standard for IP reputation ─────────
// BIND RRL, GFW pollution research, and IP reputation systems all use
// /24 (256 IPs) as the optimal granularity: tight enough to distinguish
// different organizations, broad enough to handle CDN edge variations.
// /16 is too broad — 65,536 IPs may span multiple ASes.
static QString prefix24(const QString& ip) {
    int dot3 = ip.indexOf('.', ip.indexOf('.', ip.indexOf('.') + 1) + 1);
    return (dot3 > 0) ? ip.left(dot3) : ip;
}

static bool sharesPrefix(const QStringList& a, const QStringList& b) {
    for (const auto& ipA : a) {
        QString p = prefix24(ipA);
        for (const auto& ipB : b) {
            if (prefix24(ipB) == p) return true;
        }
    }
    return false;
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

    static const char* kFakeDomains[] = {
        "thisdomainshouldnotexist12345.com",
        "nonexistent-test-domain-98765.org",
        "definitely-not-real-domain-42.net",
    };

    for (const auto& domain : kFakeDomains) {
        QElapsedTimer probe; probe.start();
        QString ip = DnsResolver::instance().resolve(QString::fromUtf8(domain), 3000);
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
    out.append(QStringLiteral("Compares domestic (AliDNS, DNSPod) vs international (Google, Cloudflare)"));
    out.append(QStringLiteral("DoH resolvers.  Same /24 subnet = CDN geo-routing (not pollution)."));
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
        out.append(QStringLiteral("  DoH:    %1").arg(dohStr));
        out.append(QStringLiteral("  Local:  %1").arg(localStr));

        if (dohIps.isEmpty()) {
            out.append(QStringLiteral("  → Inconclusive — DoH query failed"));
            pollutionErrors++;
        } else if (localIp.isEmpty()) {
            out.append(QStringLiteral("  → Local DNS failed — cannot compare"));
            pollutionErrors++;
        } else if (sharesPrefix(dohIps, {localIp})) {
            out.append(QStringLiteral("  → Clean — local IP shares /24 with DoH consensus"));
            pollutionClean++;
        } else {
            out.append(QStringLiteral("  → POLLUTED — local IP (%1) not in DoH /24 range (%2)")
                .arg(localIp, dohIps.join(',')));
            pollutionDetails.append(QStringLiteral("%1: local=%2, DoH=%3")
                .arg(td.domain, localIp, dohIps.join(',')));
            pollutionWarn++;
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
