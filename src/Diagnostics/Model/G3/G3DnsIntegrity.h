// =============================================================================
// G3DnsIntegrity.h — Multi-signal DNS integrity scoring engine
//
// Detection philosophy: IP-based comparisons are unreliable — CDN, Anycast,
// and GeoDNS make "different IPs from different sources" normal behavior.
// Instead, we analyze DNS response STRUCTURE and METADATA (CNAME chain, TTL,
// response timing) which are invariant under legitimate DNS operation and
// only break under pollution/injection.
// =============================================================================
#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

namespace G1G2G3Native {

struct DnsSignal {
    int     weight;      // 1-5
    bool    triggered;   // anomaly detected?
    QString detail;      // human-readable
};

struct DnsIntegrityResult {
    enum Verdict { Clean, Suspicious, Polluted, Hijacked };
    Verdict  verdict = Clean;
    int      scorePercent = 0;   // 0-100
    QVector<DnsSignal> signals;
    QStringList output;          // formatted per-domain lines

    // Metadata extracted during analysis
    QStringList dohIps;
    QString     localUdpIp;
    int         localUdpMs = 0;
    int         dohMinTtl = 0;
    QStringList cnameChain;
    bool        hasCname = false;
};

// ── Scoring engine ─────────────────────────────────────────────────
// Signals used (IP-independent, structure/metadata only):
//   CNAME anomaly  (weight=5): DoH has CNAME chain → pollution returns bare A
//   TTL anomaly    (weight=3): TTL < 10s → injection often sets TTL=0
//   Timing anomaly (weight=2): UDP < 15ms → injection is near-instant
// Total weight = 10. Thresholds: >20% Suspicious, >40% Polluted, >60% Hijacked
DnsIntegrityResult scoreDnsIntegrity(
    const QString& domain,
    const QString& description,
    const DohFullResult& doh,
    const QString& localUdpIp, int localUdpMs);

} // namespace G1G2G3Native