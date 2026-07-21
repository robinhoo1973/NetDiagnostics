#include "Diagnostics/Model/GHelpers.h"

namespace G1G2G3Native {

// ── DoH endpoint configuration ──────────────────────────────────────
static const struct {
    const char* label;    // human-readable name
    const char* url;      // DoH JSON API endpoint
} kDohEndpoints[] = {
    {"AliDNS (CN)",     "https://dns.alidns.com/resolve"},
    {"DNSPod (CN)",     "https://doh.pub/dns-query"},
    {"Google (US)",     "https://dns.google/resolve"},
    {"Cloudflare (US)", "https://cloudflare-dns.com/dns-query"},
};

DiagnosticResult dnsPollution(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

    out.append(QStringLiteral("DNS Pollution Check (DoH Multi-Resolver)"));
    out.append(QStringLiteral("========================================="));
    out.append(QString());
    out.append(QStringLiteral("Queries known domains via 4 DNS-over-HTTPS resolvers:"));
    out.append(QStringLiteral("  Domestic:  AliDNS, DNSPod (Tencent)"));
    out.append(QStringLiteral("  International: Google DNS, Cloudflare DNS"));
    out.append(QStringLiteral("If domestic resolvers return different IPs than international,"));
    out.append(QStringLiteral("DNS pollution / GFW interference is likely."));
    out.append(QString());

    // ── Test domains: commonly blocked / redirected in certain regions ─
    static const struct {
        const char* domain;
        const char* description;
    } kTestDomains[] = {
        {"www.google.com",   "Google Search"},
        {"www.youtube.com",  "YouTube"},
        {"www.facebook.com", "Facebook"},
        {"www.twitter.com",  "Twitter"},
    };

    int polluted = 0, clean = 0, errors = 0;
    QStringList pollutionDetails;

    for (const auto& td : kTestDomains) {
        // ── Query all 4 DoH resolvers ───────────────────────────────
        struct ResolverResult { QString label; QStringList ips; bool ok; };
        QVector<ResolverResult> results;

        for (const auto& ep : kDohEndpoints) {
            ResolverResult rr;
            rr.label = QString::fromUtf8(ep.label);
            rr.ips = G1G2G3Native::dohQuery(
                QString::fromUtf8(ep.url), QString::fromUtf8(td.domain));
            rr.ok = !rr.ips.isEmpty();
            results.append(rr);
        }

        // ── Compare domestic vs international ───────────────────────
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

        // ── Output per-domain table ─────────────────────────────────
        out.append(QStringLiteral("── %1 (%2) ──").arg(td.domain, td.description));
        out.append(QStringLiteral("  %1  %2  %3")
            .arg(QStringLiteral("Resolver"), -18)
            .arg(QStringLiteral("Result IPs"), -36)
            .arg(QStringLiteral("Status")));
        out.append(QStringLiteral("  %1  %2  %3")
            .arg(QString(18, '-'))
            .arg(QString(36, '-'))
            .arg(QString(10, '-')));

        for (const auto& rr : results) {
            QString ips = rr.ips.isEmpty() ? QStringLiteral("(no response)")
                         : rr.ips.join(QStringLiteral(", "));
            QString status = rr.ok ? QStringLiteral("OK") : QStringLiteral("FAIL");
            out.append(QStringLiteral("  %1  %2  %3")
                .arg(rr.label, -18).arg(ips, -36).arg(status));
        }

        // ── Pollution verdict ──────────────────────────────────────
        if (domesticIps.isEmpty() && internationalIps.isEmpty()) {
            out.append(QStringLiteral("  → All resolvers failed — inconclusive"));
            errors++;
        } else if (domesticIps.isEmpty()) {
            out.append(QStringLiteral("  → Domestic resolvers returned no results — possible block"));
            pollutionDetails.append(QStringLiteral("%1: domestic failed").arg(td.domain));
            polluted++;
        } else if (internationalIps.isEmpty()) {
            out.append(QStringLiteral("  → International resolvers unreachable — network restriction?"));
            errors++;
        } else {
            // Compare IP sets
            bool mismatch = false;
            for (const auto& dip : domesticIps) {
                if (!internationalIps.contains(dip)) { mismatch = true; break; }
            }
            if (!mismatch) {
                for (const auto& iip : internationalIps) {
                    if (!domesticIps.contains(iip)) { mismatch = true; break; }
                }
            }
            if (mismatch) {
                out.append(QStringLiteral("  → MISMATCH — Domestic IPs differ from international"));
                out.append(QStringLiteral("     Domestic:      %1").arg(domesticIps.join(QStringLiteral(", "))));
                out.append(QStringLiteral("     International: %1").arg(internationalIps.join(QStringLiteral(", "))));
                pollutionDetails.append(QStringLiteral("%1: %2 vs %3")
                    .arg(td.domain, domesticIps.join(','), internationalIps.join(',')));
                polluted++;
            } else {
                out.append(QStringLiteral("  → Clean — all resolvers agree"));
                clean++;
            }
        }
        out.append(QString());
    }

    // ── Final verdict ───────────────────────────────────────────────
    out.append(QStringLiteral("=================================================================="));
    out.append(QStringLiteral("Summary: %1 clean, %2 polluted, %3 errors")
        .arg(clean).arg(polluted).arg(errors));

    if (polluted > 0) {
        out.append(QStringLiteral("Verdict: DNS POLLUTION DETECTED"));
        out.append(QString());
        out.append(QStringLiteral("Domestic DNS resolvers returned different IP addresses than"));
        out.append(QStringLiteral("international resolvers for the following domains:"));
        for (const auto& d : pollutionDetails)
            out.append(QStringLiteral("  • %1").arg(d));
        out.append(QString());
        out.append(QStringLiteral("This strongly suggests DNS poisoning / GFW interference."));
        r.status = DiagStatus::Warning;
        r.summary = QStringLiteral("DNS polluted: %1/%2 domains").arg(polluted)
            .arg(polluted + clean + errors);
    } else if (errors > 0 && clean == 0) {
        out.append(QStringLiteral("Verdict: INCONCLUSIVE — all queries failed (network restriction?)"));
        r.status = DiagStatus::Info;
        r.summary = QStringLiteral("DNS: all queries failed");
    } else if (errors > 0) {
        out.append(QStringLiteral("Verdict: LIKELY CLEAN — no pollution on resolvable domains (%1 errors)").arg(errors));
        r.status = DiagStatus::Info;
        r.summary = QStringLiteral("DNS: %1 clean, %2 errors").arg(clean).arg(errors);
    } else {
        out.append(QStringLiteral("Verdict: DNS CLEAN — no pollution detected across %1 domains").arg(clean));
        r.status = DiagStatus::Pass;
        r.summary = QStringLiteral("DNS clean (%1 domains)").arg(clean);
    }

    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    r.durationMs = t.elapsed();
    return r;
}

} // namespace G1G2G3Native
