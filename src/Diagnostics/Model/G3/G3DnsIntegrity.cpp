// =============================================================================
// G3DnsIntegrity.cpp — Multi-signal DNS integrity scoring engine
// =============================================================================
#include "Diagnostics/Model/G3/G3DnsIntegrity.h"
#include "Diagnostics/Model/GHelpers.h"

namespace G1G2G3Native {

// ── Signal definitions ─────────────────────────────────────────────

static DnsSignal sigUdpVsTcp(const QString& localUdpIp, const QString& localTcpIp) {
    bool mismatch = !localUdpIp.isEmpty() && !localTcpIp.isEmpty()
                    && localUdpIp != localTcpIp;
    return {4, mismatch,
        mismatch ? QStringLiteral("UDP(%1) ≠ TCP(%2)").arg(localUdpIp, localTcpIp)
                 : QString()};
}

static DnsSignal sigDohVsUdp(const QStringList& dohIps, const QString& localIp) {
    if (dohIps.isEmpty() || localIp.isEmpty()) return {4, false, {}};
    bool differ = !dohIps.contains(localIp);
    return {4, differ,
        differ ? QStringLiteral("DoH(%1) ≠ Local(%2)")
                    .arg(dohIps.join(','), localIp)
               : QString()};
}

static DnsSignal sigCnameAnomaly(bool dohHasCname, bool localHasCname) {
    bool anomaly = dohHasCname && !localHasCname;
    return {3, anomaly,
        anomaly ? QStringLiteral("DoH has CNAME chain, local is bare A-record")
                : QString()};
}

static DnsSignal sigTtlAnomaly(int dohMinTtl, int localTtl) {
    bool dohTtlLow = (dohMinTtl > 0 && dohMinTtl < 10);
    bool localTtlLow = (localTtl > 0 && localTtl < 10);
    bool anomaly = dohTtlLow || localTtlLow;
    QString detail;
    if (dohTtlLow) detail = QStringLiteral("DoH TTL=%1s (abnormally low)").arg(dohMinTtl);
    if (localTtlLow) {
        if (!detail.isEmpty()) detail += "; ";
        detail += QStringLiteral("Local TTL=%1s (abnormally low)").arg(localTtl);
    }
    return {2, anomaly, anomaly ? detail : QString()};
}

static DnsSignal sigTimingAnomaly(int localMs) {
    bool tooFast = (localMs > 0 && localMs < 15);
    return {2, tooFast,
        tooFast ? QStringLiteral("UDP response %1ms (suspiciously fast — possible injection)")
                    .arg(localMs)
                : QString()};
}

// ── Scoring engine ─────────────────────────────────────────────────

DnsIntegrityResult scoreDnsIntegrity(
    const QString& domain,
    const QString& description,
    const DohFullResult& doh,
    const QString& localUdpIp, int localUdpMs,
    const QString& localTcpIp, int localTcpMs)
{
    DnsIntegrityResult r;
    r.dohIps    = doh.aRecords;
    r.localUdpIp = localUdpIp;
    r.localTcpIp = localTcpIp;
    r.localUdpMs = localUdpMs;
    r.localTcpMs = localTcpMs;
    r.dohMinTtl = doh.minTtl;
    r.cnameChain = doh.cnameChain;
    r.hasCname  = doh.hasCname;

    // ── Collect signals ──────────────────────────────────────────
    r.signals.append(sigUdpVsTcp(localUdpIp, localTcpIp));
    r.signals.append(sigDohVsUdp(doh.aRecords, localUdpIp));
    r.signals.append(sigCnameAnomaly(doh.hasCname, false)); // local CNAME N/A from getaddrinfo
    r.signals.append(sigTtlAnomaly(doh.minTtl, 0)); // local TTL N/A from getaddrinfo
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

    // ── Verdict thresholds ──────────────────────────────────────
    if (r.scorePercent > 60)
        r.verdict = DnsIntegrityResult::Hijacked;
    else if (r.scorePercent > 30)
        r.verdict = DnsIntegrityResult::Polluted;
    else if (r.scorePercent > 15)
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

    QString dohStr = r.dohIps.isEmpty() ? QStringLiteral("no response") : r.dohIps.join(',');
    QString line = QStringLiteral("%1 — DoH=%2").arg(label, dohStr);
    r.output.append(line);

    if (!localUdpIp.isEmpty())
        r.output.append(QStringLiteral("    UDP: %1 (%2ms, TTL=%3)")
            .arg(localUdpIp).arg(localUdpMs).arg(doh.minTtl > 0 ? QString::number(doh.minTtl) : "?"));
    if (!localTcpIp.isEmpty() && localTcpIp != localUdpIp)
        r.output.append(QStringLiteral("    TCP: %1 (%2ms)")
            .arg(localTcpIp).arg(localTcpMs));
    if (doh.hasCname)
        r.output.append(QStringLiteral("    CNAME: %1").arg(doh.cnameChain.join(QStringLiteral(" → "))));

    r.output.append(QStringLiteral("    Score: %1% → %2 (%3 signal(s))")
        .arg(r.scorePercent).arg(statusIcon).arg(triggeredSignals.size()));
    for (const auto& d : triggeredSignals)
        r.output.append(QStringLiteral("      · %1").arg(d));

    return r;
}

} // namespace G1G2G3Native