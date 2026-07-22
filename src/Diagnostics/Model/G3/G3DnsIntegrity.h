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
#include "Diagnostics/Model/GHelpers.h"

namespace SystemDiagnostics {

struct DnsIntegritySignal {
    int     weight;      // 1-5
    bool    triggered;   // anomaly detected?
    QString detail;      // human-readable
};

struct DnsIntegrityResult {
    enum class Verdict { DNS_INTEGRITY_CLEAN, DNS_INTEGRITY_SUSPECT, DNS_INTEGRITY_TAMPERED, DNS_INTEGRITY_HIJACKED };
    Verdict  verdict = Verdict::DNS_INTEGRITY_CLEAN;
    int      scorePercent = 0;   // 0-100
    QVector<DnsIntegritySignal> detectedSignals;
    QStringList output;          // formatted per-domain lines

    // Metadata extracted during analysis
    QStringList dohIps;
    QString     localUdpIp;
    int         localUdpMs = 0;
    int         dohMinTtl = -1;  // -1 = no TTL data (matches DohDnsFullResult sentinel)
    QStringList cnameChain;
    bool        hasCname = false;
};

// ── Scoring engine ─────────────────────────────────────────────────
// Signals (IP-independent, structure/metadata only):
//   TLS cert mismatch (weight=5): cert SAN/CN does not match domain
//   CNAME anomaly     (weight=5): DoH has CNAME chain → pollution returns bare A
//   TTL anomaly       (weight=3): TTL < 10s → injection often sets TTL=0
//   Timing anomaly    (weight=2): UDP < 15ms → injection is near-instant
// Total weight = 15. Thresholds: >14% Suspicious, >33% Polluted, >60% Hijacked
DnsIntegrityResult scoreDnsIntegrity(
    const QString& domain,
    const QString& description,
    const DohDnsFullResult& doh,
    const QString& localUdpIp, int localUdpMs);

} // namespace SystemDiagnostics