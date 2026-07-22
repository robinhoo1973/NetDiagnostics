// =============================================================================
// G3DnsIntegrity.cpp — Multi-signal DNS integrity scoring engine
//
// Signals are deliberately IP-independent — CDN/Anycast/GeoDNS make IP
// comparisons unreliable.  Instead we analyze DNS response STRUCTURE:
//   · CNAME chain — pollution often returns bare A-record, skipping CNAME
//   · TTL — pollution injection often has TTL=0 or TTL=1
//   · Response timing — injected responses arrive near-instantly
// =============================================================================
#include "Diagnostics/Model/G3/G3DnsIntegrity.h"
#include "Diagnostics/Model/GHelpers.h"

namespace G1G2G3Native {

// ── Signal definitions ─────────────────────────────────────────────

static DnsSignal sigCnameAnomaly(bool dohHasCname) {
    // Pollution/great-firewall DNS injection typically returns a bare
    // A-record response without the CNAME chain that legitimate CDN
    // domains use (e.g. www.google.com → CNAME → google.com → A).
    return {5, dohHasCname ? false : true,
        dohHasCname ? QString()
                    : QStringLiteral("No CNAME chain — DoH returned bare A-record "
                                     "(legitimate CDN domains normally have CNAME indirection)")};
}

static DnsSignal sigTtlAnomaly(int dohMinTtl) {
    // Legitimate DNS TTLs are typically 60-3600 seconds.  Pollution
    // injection devices (GFW, ISP hijacking boxes) often set TTL=0
    // or TTL=1 to prevent caching of the fake response.
    bool low = (dohMinTtl > 0 && dohMinTtl < 10);
    return {3, low,
        low ? QStringLiteral("TTL=%1s abnormally low (normal: 60-3600s)")
                  .arg(dohMinTtl)
             : QString()};
}

static DnsSignal sigTimingAnomaly(int localMs) {
    // Legitimate DNS recursion involves network round-trips + resolver
    // processing → typically 30-200ms.  Injection responses come from
    // a local device on the network path → <15ms is suspicious.
    bool tooFast = (localMs > 0 && localMs < 15);
    return {2, tooFast,
        tooFast ? QStringLiteral("UDP response %1ms (suspiciously fast — "
                                 "legitimate recursion takes 30ms+)".arg(localMs)
                : QString()};
}

// ── Scoring engine ─────────────────────────────────────────────────

DnsIntegrityResult scoreDnsIntegrity(
    const QString& domain,
    const QString& description,
    const DohFullResult& doh,
    const QString& localUdpIp, int localUdpMs)
{
    DnsIntegrityResult r;
    r.dohIps     = doh.aRecords;
    r.localUdpIp = localUdpIp;
    r.localUdpMs = localUdpMs;
    r.dohMinTtl  = doh.minTtl;
    r.cnameChain = doh.cnameChain;
    r.hasCname   = doh.hasCname;

    // ── Collect signals (IP-independent, structure/metadata only) ──
    r.signals.append(sigCnameAnomaly(doh.hasCname));
    r.signals.append(sigTtlAnomaly(doh.minTtl));
    r.signals.append(sigTimingAnomaly(localUdpMs));

    // ── Weighted scoring ────────────────────────────────────────
    int totalWeight = 0, triggeredWeight = 0;
    QStringList triggeredSignals;
    for (const auto& s : r.signals) {
        totalWeight += s.weight;
        if (s.triggered) {
            triggeredWeight += s.weight;
            triggeredSignals.append(s.detail);
        }
    }
    r.scorePercent = totalWeight > 0 ? (triggeredWeight * 100 / totalWeight) : 0;

    // ── Verdict thresholds (total weight = 10) ──────────────────
    //   0-20% Clean, 21-40% Suspicious, 41-60% Polluted, >60% Hijacked
    if (r.scorePercent > 60)
        r.verdict = DnsIntegrityResult::Hijacked;
    else if (r.scorePercent > 40)
        r.verdict = DnsIntegrityResult::Polluted;
    else if (r.scorePercent > 20)
        r.verdict = DnsIntegrityResult::Suspicious;
    else
        r.verdict = DnsIntegrityResult::Clean;

    // ── Build output lines ──────────────────────────────────────
    QString label = QStringLiteral("  %1 (%2)").arg(domain, description);
    QString statusIcon;
    switch (r.verdict) {
        case DnsIntegrityResult::Clean:      statusIcon = QStringLiteral("Clean"); break;
        case DnsIntegrityResult::Suspicious: statusIcon = QStringLiteral("Suspicious"); break;
        case DnsIntegrityResult::Polluted:   statusIcon = QStringLiteral("POLLUTED"); break;
        case DnsIntegrityResult::Hijacked:   statusIcon = QStringLiteral("HIJACKED"); break;
    }

    // Per-domain header
    r.output.append(QStringLiteral("%1 — Score: %2% → %3")
        .arg(label).arg(r.scorePercent).arg(statusIcon));

    // Key metadata (not IP-based comparison)
    r.output.append(QStringLiteral("    DoH: %1 (TTL=%2, CNAME=%3)")
        .arg(r.dohIps.isEmpty() ? QStringLiteral("no response") : r.dohIps.join(','))
        .arg(doh.minTtl > 0 ? QStringLiteral("%1s").arg(doh.minTtl) : QStringLiteral("?"))
        .arg(doh.hasCname ? doh.cnameChain.join(QStringLiteral(" → ")) : QStringLiteral("none")));

    if (!localUdpIp.isEmpty())
        r.output.append(QStringLiteral("    Local UDP: %1 (%2ms)")
            .arg(localUdpIp).arg(localUdpMs));

    // Only show signals that triggered
    if (!triggeredSignals.isEmpty()) {
        r.output.append(QStringLiteral("    Anomalies (%1):").arg(triggeredSignals.size()));
        for (const auto& d : triggeredSignals)
            r.output.append(QStringLiteral("      · %1").arg(d));
    } else {
        r.output.append(QStringLiteral("    No anomalies detected."));
    }

    return r;
}

} // namespace G1G2G3Native