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

static DnsSignal sigCnameAnomaly(bool dohHasCname) {
    // Pollution/great-firewall DNS injection typically returns a bare
    // A-record response without the CNAME chain that legitimate CDN
    // domains use (e.g. www.google.com → CNAME → google.com → A).
    return {5, dohHasCname ? false : true,
        dohHasCname ? QString()
                    : QStringLiteral("No CNAME Chain — DoH Returned Bare A-Record "
                                     "(Legitimate CDN Domains Normally Have CNAME Indirection)")};
}

static DnsSignal sigTtlAnomaly(int dohMinTtl) {
    // Legitimate DNS TTLs are typically 60-3600 seconds.  Pollution
    // injection devices (GFW, ISP hijacking boxes) often set TTL=0
    // or TTL=1 to prevent caching of the fake response.
    bool low = (dohMinTtl > 0 && dohMinTtl < 10);
    return {3, low,
        low ? QStringLiteral("TTL=%1s Abnormally Low (Normal: 60-3600s)")
                  .arg(dohMinTtl)
             : QString()};
}

static DnsSignal sigTimingAnomaly(int localMs) {
    // Legitimate DNS recursion involves network round-trips + resolver
    // processing → typically 30-200ms.  Injection responses come from
    // a local device on the network path → <15ms is suspicious.
    bool tooFast = (localMs > 0 && localMs < 15);
    return {2, tooFast,
        tooFast ? QStringLiteral("UDP Response %1ms (Suspiciously Fast — "
                                 "Legitimate Recursion Takes 30ms+)".arg(localMs)
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
    r.signals.append(sigCnameAnomaly(doh.hasCname));
    r.signals.append(sigTtlAnomaly(doh.minTtl));
    r.signals.append(sigTimingAnomaly(localUdpMs));

    // ── Weighted scoring (total weight = 15) ─────────────────────
    //   TLS cert: 5   CNAME: 5   TTL: 3   Timing: 2
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

    // ── Verdict thresholds (total weight = 15) ──────────────────
    //   One signal(5/15)=33%→Polluted, two signals(10/15)=67%→Hijacked
    if (r.scorePercent > 60)
        r.verdict = DnsIntegrityResult::Verdict::Critical;
    else if (r.scorePercent > 33)
        r.verdict = DnsIntegrityResult::Verdict::Fail;
    else if (r.scorePercent > 14)
        r.verdict = DnsIntegrityResult::Verdict::Warning;
    else
        r.verdict = DnsIntegrityResult::Verdict::Pass;

    // ── Build output lines ──────────────────────────────────────
    QString label = QStringLiteral("  %1 (%2)").arg(domain, description);
    QString statusIcon;
    switch (r.verdict) {
        case DnsIntegrityResult::Verdict::Pass:       statusIcon = QStringLiteral("Clean"); break;
        case DnsIntegrityResult::Verdict::Warning: statusIcon = QStringLiteral("Suspicious"); break;
        case DnsIntegrityResult::Verdict::Fail:   statusIcon = QStringLiteral("POLLUTED"); break;
        case DnsIntegrityResult::Verdict::Critical:   statusIcon = QStringLiteral("HIJACKED"); break;
    }

    // Per-domain header
    r.output.append(QStringLiteral("%1 — Score: %2% → %3")
        .arg(label).arg(r.scorePercent).arg(statusIcon));

    // Key metadata
    r.output.append(QStringLiteral("    DoH: %1 (TTL=%2, CNAME=%3)")
        .arg(r.dohIps.isEmpty() ? QStringLiteral("No Response") : r.dohIps.join(','))
        .arg(doh.minTtl > 0 ? QStringLiteral("%1s").arg(doh.minTtl) : QStringLiteral("?"))
        .arg(doh.hasCname ? doh.cnameChain.join(QStringLiteral(" → ")) : QStringLiteral("None")));

    if (!localUdpIp.isEmpty())
        r.output.append(QStringLiteral("    Local UDP: %1 (%2ms)")
            .arg(localUdpIp).arg(localUdpMs));

    // Only show signals that triggered
    if (!triggeredSignals.isEmpty()) {
        r.output.append(QStringLiteral("    Anomalies (%1):").arg(triggeredSignals.size()));
        for (const auto& d : triggeredSignals)
            r.output.append(QStringLiteral("      · %1").arg(d));
    } else {
        r.output.append(QStringLiteral("    No Anomalies Detected."));
    }

    return r;
}

} // namespace G1G2G3Native
