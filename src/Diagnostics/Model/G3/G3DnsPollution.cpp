#include "Diagnostics/Model/GHelpers.h"
#include "Common/Services/DnsResolver.h"

namespace G1G2G3Native {

// ── DoH endpoint configuration ──────────────────────────────────────
static const struct {
    const char* label;
    const char* url;
} kDohEndpoints[] = {
    {"AliDNS (CN)",     "https://dns.alidns.com/resolve"},
    {"DNSPod (CN)",     "https://doh.pub/dns-query"},
    {"Google (US)",     "https://dns.google/resolve"},
    {"Cloudflare (US)", "https://cloudflare-dns.com/dns-query"},
};

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
        struct ResolverResult { QString label; QStringList ips; bool ok; };
        QVector<ResolverResult> results;

        for (const auto& ep : kDohEndpoints) {
            ResolverResult rr;
            rr.label = QString::fromUtf8(ep.label);
            QString queryUrl = QStringLiteral("%1?name=%2&type=A")
                .arg(QString::fromUtf8(ep.url), QString::fromUtf8(td.domain));
            QByteArray body = G1G2G3Native::httpsGet(queryUrl, 4000);
            if (!body.isEmpty()) {
                QString json = QString::fromUtf8(body);
                int ansStart = json.indexOf(QStringLiteral("\"Answer\":["));
                if (ansStart >= 0) {
                    int pos = ansStart;
                    int sectionEnd = json.indexOf(']', ansStart);
                    while ((pos = json.indexOf(QStringLiteral("\"data\":\""), pos)) >= 0
                           && (sectionEnd < 0 || pos < sectionEnd)) {
                        pos += 8;
                        int end = json.indexOf('\"', pos);
                        if (end > pos) {
                            QString ip = json.mid(pos, end - pos);
                            if (!ip.isEmpty() && ip[0].isDigit())
                                rr.ips.append(ip);
                        }
                    }
                }
            }
            rr.ok = !rr.ips.isEmpty();
            results.append(rr);
        }

        QStringList consensusIps = G1G2G3Native::dohQuery(
            QString::fromUtf8(td.domain));

        QStringList domesticIps, internationalIps;
        for (const auto& rr : results) {
            bool isDomestic = rr.label.contains(QStringLiteral("CN)"));
            for (const auto& ip : rr.ips) {
                if (isDomestic) {
                    if (!domesticIps.contains(ip)) domesticIps.append(ip);
                } else {
                    if (!internationalIps.contains(ip)) internationalIps.append(ip);
                }
            }
        }

        // ── Per-domain table ─────────────────────────────────────────
        out.append(QStringLiteral("── %1 (%2) ──").arg(td.domain, td.description));
        out.append(QStringLiteral("  %1  %2  %3")
            .arg(QStringLiteral("Resolver"), -18)
            .arg(QStringLiteral("Result IPs"), -36)
            .arg(QStringLiteral("Status")));
        out.append(QStringLiteral("  %1  %2  %3")
            .arg(QString(18, '-')).arg(QString(36, '-')).arg(QString(10, '-')));

        for (const auto& rr : results) {
            out.append(QStringLiteral("  %1  %2  %3")
                .arg(rr.label, -18)
                .arg(rr.ips.isEmpty() ? QStringLiteral("(no response)") : rr.ips.join(QStringLiteral(", ")), -36)
                .arg(rr.ok ? QStringLiteral("OK") : QStringLiteral("FAIL")));
        }

        out.append(QStringLiteral("  %1  %2")
            .arg(QStringLiteral("Consensus (≥3/4):"), -20)
            .arg(consensusIps.isEmpty() ? QStringLiteral("(no consensus)") : consensusIps.join(QStringLiteral(", "))));

        // ── Pollution verdict (with /24 CDN disambiguation) ──────────
        if (domesticIps.isEmpty() && internationalIps.isEmpty()) {
            out.append(QStringLiteral("  → All resolvers failed — inconclusive"));
            pollutionErrors++;
        } else if (domesticIps.isEmpty()) {
            out.append(QStringLiteral("  → Domestic resolvers returned no results — possible block"));
            pollutionDetails.append(QStringLiteral("%1: domestic failed").arg(td.domain));
            pollutionWarn++;
        } else if (internationalIps.isEmpty()) {
            out.append(QStringLiteral("  → International resolvers unreachable — network restriction?"));
            pollutionErrors++;
        } else if (sharesPrefix(domesticIps, internationalIps)) {
            out.append(QStringLiteral("  → Clean — IPs share /24 subnet (CDN geo-routing, not pollution)"));
            out.append(QStringLiteral("     Domestic:      %1").arg(domesticIps.join(QStringLiteral(", "))));
            out.append(QStringLiteral("     International: %1").arg(internationalIps.join(QStringLiteral(", "))));
            pollutionClean++;
        } else {
            out.append(QStringLiteral("  → POLLUTED — Domestic IPs differ from international (no shared /24)"));
            out.append(QStringLiteral("     Domestic:      %1").arg(domesticIps.join(QStringLiteral(", "))));
            out.append(QStringLiteral("     International: %1").arg(internationalIps.join(QStringLiteral(", "))));
            pollutionDetails.append(QStringLiteral("%1: %2 vs %3")
                .arg(td.domain, domesticIps.join(','), internationalIps.join(',')));
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
