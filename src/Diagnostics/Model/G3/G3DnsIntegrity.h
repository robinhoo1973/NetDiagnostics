// =============================================================================
// G3DnsIntegrity.h — Multi-signal DNS integrity scoring engine
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
    QString     localTcpIp;
    int         localUdpMs = 0;
    int         localTcpMs = 0;
    int         dohMinTtl = 0;
    QStringList cnameChain;
    bool        hasCname = false;
};

// ── Scoring engine ─────────────────────────────────────────────────
DnsIntegrityResult scoreDnsIntegrity(
    const QString& domain,
    const QString& description,
    const DohFullResult& doh,
    const QString& localUdpIp, int localUdpMs,
    const QString& localTcpIp, int localTcpMs);

} // namespace G1G2G3Native