// =============================================================================
// G3DnsIntegrity.cpp — Multi-signal DNS integrity scoring engine
//
// Signals are deliberately IP-independent — CDN/Anycast/GeoDNS make IP
// comparisons unreliable.  Instead we analyze DNS response STRUCTURE:
//   · CNAME chain — pollution often returns bare A-record, skipping CNAME
//   · TTL — pollution injection often has TTL=0 or TTL=1
//   · Response timing — injected responses arrive near-instantly
//   · TLS certificate — definitive: cert SAN/CN must match the domain
// =============================================================================
#include "Diagnostics/Model/G3/G3DnsIntegrity.h"
#include "Diagnostics/Model/GHelpers.h"
#include <QSslSocket>
#include <QSslCertificate>

namespace G1G2G3Native {

// ── TLS certificate domain verification ───────────────────────────

static QString tlsCheckCert(const QString& ip, const QString& domain, int timeoutMs = 3000) {
    // Connect to the IP directly but verify the certificate against the
    // domain name via SNI.  If the IP was injected by DNS pollution, the
    // server at that IP won't have a valid certificate for the domain.
    // Returns the certificate CN on mismatch, empty string on success,
    // or "no TLS" if the connection fails (port 443 closed = not a signal).
    QSslSocket socket;
    socket.setPeerVerifyMode(QSslSocket::VerifyNone);  // don't abort on mismatch
    socket.connectToHostEncrypted(ip, 443, domain);     // IP to connect, domain for SNI
    if (!socket.waitForEncrypted(timeoutMs)) {
        socket.disconnectFromHost();
        return QStringLiteral("no TLS");  // connection failed → not a signal
    }

    const auto certs = socket.peerCertificateChain();
    socket.disconnectFromHost();
    if (certs.isEmpty()) return {};  // shouldn't happen after successful handshake

    const auto& cert = certs.first();

    // Check SAN (Subject Alternative Names) — modern certs use this
    const auto sans = cert.subjectAlternativeNames();
    const auto sanValues = sans.values();
    for (const auto& san : sanValues) {
        if (san == domain || (san.startsWith("*.") && domain.endsWith(san.mid(1))))
            return {};  // matched
    }

    // Fall back to CN (Common Name) — older certs
    const auto cns = cert.subjectInfo(QSslCertificate::CommonName);
    for (const auto& cn : cns) {
        if (cn == domain || (cn.startsWith("*.") && domain.endsWith(cn.mid(1))))
            return {};  // matched
    }

    // Mismatch — the certificate at this IP is NOT for the target domain
    QString actualCn = cns.isEmpty() ? QStringLiteral("unknown") : cns.first();
    QString actualSan = sanValues.isEmpty() ? QString() : sanValues.first();
    QString detail = actualSan.isEmpty()
        ? QStringLiteral("TLS cert CN=%1 ≠ %2").arg(actualCn, domain)
        : QStringLiteral("TLS cert SAN=%1 / CN=%2 ≠ %3").arg(actualSan, actualCn, domain);
    return detail;
}

// ── Signal definitions ─────────────────────────────────────────────

static DnsSignal sigTlsCertMismatch(const QString& localIp, const QString& domain) {
    if (localIp.isEmpty()) return {5, false, {}};
    QString result = tlsCheckCert(localIp, domain, 3000);
    bool mismatch = !result.isEmpty() && result != QStringLiteral("no TLS");
    return {5, mismatch,
        mismatch ? result : QString()};
}

static DnsSignal sigDnssecFailure(bool dnssecFailed) {
    // DNSSEC-validating DoH resolvers (Google, Cloudflare) set AD=true
    // when the response signature is verified.  If ≥2 resolvers return
    // SERVFAIL (Status=2) with AD=false, the DNSSEC chain is broken —
    // definitive tampering evidence.  Only triggers for DNSSEC-enabled
    // domains; for non-DNSSEC domains, dnssecFailed remains false.
    return {5, dnssecFailed,
        dnssecFailed ? QStringLiteral("DNSSEC validation failed — ≥2 resolvers returned SERVFAIL "
                                     "(signature verification broken, likely DNS tampering)")
                     : QString()};
}

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

    // ── Collect signals ──────────────────────────────────────────
    r.signals.append(sigTlsCertMismatch(localUdpIp, domain));
    r.signals.append(sigDnssecFailure(doh.dnssecFailed));
    r.signals.append(sigCnameAnomaly(doh.hasCname));
    r.signals.append(sigTtlAnomaly(doh.minTtl));
    r.signals.append(sigTimingAnomaly(localUdpMs));

    // ── Weighted scoring (total weight = 20) ─────────────────────
    //   TLS cert: 5   DNSSEC: 5   CNAME: 5   TTL: 3   Timing: 2
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

    // ── Verdict thresholds (total weight = 20) ──────────────────
    //   Signal alone(5/20)=25%→Suspicious, two signals(10/20)=50%→Polluted
    //   Three signals(15/20)=75%→Hijacked
    if (r.scorePercent > 50)
        r.verdict = DnsIntegrityResult::Hijacked;
    else if (r.scorePercent > 25)
        r.verdict = DnsIntegrityResult::Polluted;
    else if (r.scorePercent > 10)
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

    // Key metadata
    r.output.append(QStringLiteral("    DoH: %1 (TTL=%2, CNAME=%3, DNSSEC=%4)")
        .arg(r.dohIps.isEmpty() ? QStringLiteral("no response") : r.dohIps.join(','))
        .arg(doh.minTtl > 0 ? QStringLiteral("%1s").arg(doh.minTtl) : QStringLiteral("?"))
        .arg(doh.hasCname ? doh.cnameChain.join(QStringLiteral(" → ")) : QStringLiteral("none"))
        .arg(doh.dnssecFailed ? QStringLiteral("FAILED") :
             doh.adFlag      ? QStringLiteral("validated") : QStringLiteral("N/A")));

    if (doh.dnssecFailed)
        r.output.append(QStringLiteral("    ⚠ DNSSEC validation failed — DNS response tampered"));

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