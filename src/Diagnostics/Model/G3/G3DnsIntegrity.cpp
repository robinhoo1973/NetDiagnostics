// =============================================================================
// G3DnsIntegrity.cpp — DNS Integrity diagnostic (scoring engine + entry point)
//
// Two-phase diagnostic:
//   Phase 1 — ISP DNS Hijacking: resolve fake domains; any IP returned = hijack
//   Phase 2 — Multi-signal cross-verification: DoH (trusted) vs local UDP
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
#include "Common/Services/DnsResolver.h"
#include <QCryptographicHash>
#include <QSslSocket>
#include <QSslCertificate>
#include <QFuture>
#include <QMutex>
#include <QMutexLocker>
#include <QtConcurrent/QtConcurrent>

namespace SystemDiagnostics {

// ── IP helper utilities ──────────────────────────────────────────────

// 5WHY: IP-based comparison helpers (prefix, sharesPrefix, differentOrg)
// were removed — the scoring engine uses structure/metadata signals
// (CNAME, TTL, timing, TLS cert) which are invariant under CDN/Anycast,
// making IP comparisons unreliable.  Only isPrivateIp remains for the
// definitive private-IP hijacking check (Phase 2).

static bool isPrivateIp(const QString& ip) {
    // RFC 1918 + loopback + CGNAT + special-use
    if (ip.startsWith("127.") || ip.startsWith("10.")) return true;
    if (ip.startsWith("192.168.")) return true;
    if (ip.startsWith("172.")) {
        int dot = ip.indexOf('.', 4);
        if (dot > 4) {
            int second = ip.mid(4, dot - 4).toInt();
            if (second >= 16 && second <= 31) return true;
        }
    }
    if (ip.startsWith("100.")) {
        int dot = ip.indexOf('.', 4);
        if (dot > 4) {
            int second = ip.mid(4, dot - 4).toInt();
            if (second >= 64 && second <= 127) return true;
        }
    }
    if (ip == "0.0.0.0") return true;
    return false;
}

// ── TLS certificate domain verification ──────────────────────────────

static QString tlsCheckCert(const QString& ip, const QString& domain, int timeoutMs = 3000) {
    QSslSocket socket;
    socket.setPeerVerifyMode(QSslSocket::VerifyNone);
    socket.connectToHostEncrypted(ip, 443, domain);
    if (!socket.waitForEncrypted(timeoutMs)) {
        socket.disconnectFromHost();
        return QStringLiteral("no TLS");
    }

    const auto certs = socket.peerCertificateChain();
    socket.disconnectFromHost();
    if (certs.isEmpty()) return {};

    const auto& cert = certs.first();

    // Check SAN (Subject Alternative Names)
    const auto sans = cert.subjectAlternativeNames();
    const auto sanValues = sans.values();
    for (const auto& san : sanValues) {
        if (san == domain || (san.startsWith("*.") && domain.endsWith(san.mid(1))))
            return {};
    }

    // Fall back to CN (Common Name)
    const auto cns = cert.subjectInfo(QSslCertificate::CommonName);
    for (const auto& cn : cns) {
        if (cn == domain || (cn.startsWith("*.") && domain.endsWith(cn.mid(1))))
            return {};
    }

    QString actualCn = cns.isEmpty() ? QStringLiteral("unknown") : cns.first();
    QString actualSan = sanValues.isEmpty() ? QString() : sanValues.first();
    return actualSan.isEmpty()
        ? QStringLiteral("TLS Cert CN=%1 ≠ %2").arg(actualCn, domain)
        : QStringLiteral("TLS Cert SAN=%1 / CN=%2 ≠ %3").arg(actualSan, actualCn, domain);
}

// ── Signal definitions ────────────────────────────────────────────────

static DnsIntegritySignal sigTlsCertMismatch(const QString& localIp, const QString& domain) {
    if (localIp.isEmpty()) return {5, false, {}};
    QString result = tlsCheckCert(localIp, domain, 3000);
    bool mismatch = !result.isEmpty() && result != QStringLiteral("no TLS");
    return {5, mismatch,
        mismatch ? result : QString()};
}

static DnsIntegritySignal sigCnameAnomaly(bool dohHasCname) {
    return {5, dohHasCname ? false : true,
        dohHasCname ? QString()
                    : QStringLiteral("No CNAME Chain - DoH Returned Bare A-Record (Legitimate CDN Domains Normally Have CNAME Indirection)")};
}

static DnsIntegritySignal sigTtlAnomaly(int dohMinTtl) {
    // 5WHY: minTtl sentinel was 86400 (collided with real 24h TTL)
    // then converted to 0 (= "no data").  Now uses -1 sentinel,
    // so TTL=0 unambiguously means real TTL=0 (pollution signal).
    bool low = (dohMinTtl >= 0 && dohMinTtl < 10);
    return {3, low,
        low ? QStringLiteral("TTL=%1s Abnormally Low (Normal: 60-3600s)")
                  .arg(dohMinTtl)
             : QString()};
}

static DnsIntegritySignal sigTimingAnomaly(int localMs) {
    bool tooFast = (localMs > 0 && localMs < 15);
    return {2, tooFast,
        tooFast ? QStringLiteral("UDP Response %1ms (Suspiciously Fast - Legitimate Recursion Takes 30ms+)").arg(localMs)
                : QString()};
}

// ── Scoring engine ────────────────────────────────────────────────────

DnsIntegrityResult scoreDnsIntegrity(
    const QString& domain,
    const QString& description,
    const DohDnsFullResult& doh,
    const QString& localUdpIp, int localUdpMs)
{
    DnsIntegrityResult r;
    r.dohIps     = doh.aRecords;
    r.localUdpIp = localUdpIp;
    r.localUdpMs = localUdpMs;
    r.dohMinTtl  = doh.minTtl;
    r.cnameChain = doh.cnameChain;
    r.hasCname   = doh.hasCname;

    // ── Collect signals ─────────────────────────────────────────────
    r.detectedSignals.append(sigTlsCertMismatch(localUdpIp, domain));
    r.detectedSignals.append(sigCnameAnomaly(doh.hasCname));
    r.detectedSignals.append(sigTtlAnomaly(doh.minTtl));
    r.detectedSignals.append(sigTimingAnomaly(localUdpMs));

    // ── Weighted scoring (total weight = 15) ────────────────────────
    int totalWeight = 0, triggeredWeight = 0;
    QStringList triggeredSignals;
    for (const auto& s : r.detectedSignals) {
        totalWeight += s.weight;
        if (s.triggered) {
            triggeredWeight += s.weight;
            triggeredSignals.append(s.detail);
        }
    }
    r.scorePercent = totalWeight > 0 ? (triggeredWeight * 100 / totalWeight) : 0;

    // ── Verdict thresholds ──────────────────────────────────────────
    if (r.scorePercent > 60)
        r.verdict = DnsIntegrityResult::Verdict::DNS_INTEGRITY_HIJACKED;
    else if (r.scorePercent > 33)
        r.verdict = DnsIntegrityResult::Verdict::DNS_INTEGRITY_TAMPERED;
    else if (r.scorePercent > 14)
        r.verdict = DnsIntegrityResult::Verdict::DNS_INTEGRITY_SUSPECT;
    else
        r.verdict = DnsIntegrityResult::Verdict::DNS_INTEGRITY_CLEAN;

    // ── Build output lines ──────────────────────────────────────────
    QString label = QStringLiteral("  %1 (%2)").arg(domain, description);
    QString statusIcon;
    switch (r.verdict) {
        case DnsIntegrityResult::Verdict::DNS_INTEGRITY_CLEAN:    statusIcon = QStringLiteral("Clean"); break;
        case DnsIntegrityResult::Verdict::DNS_INTEGRITY_SUSPECT:  statusIcon = QStringLiteral("Suspicious"); break;
        case DnsIntegrityResult::Verdict::DNS_INTEGRITY_TAMPERED: statusIcon = QStringLiteral("POLLUTED"); break;
        case DnsIntegrityResult::Verdict::DNS_INTEGRITY_HIJACKED: statusIcon = QStringLiteral("HIJACKED"); break;
    }

    r.output.append(QStringLiteral("%1 — Score: %2% → %3")
        .arg(label).arg(r.scorePercent).arg(statusIcon));

    r.output.append(QStringLiteral("    DoH: %1 (TTL=%2, CNAME=%3)")
        .arg(r.dohIps.isEmpty() ? QStringLiteral("No Response") : r.dohIps.join(','))
        .arg(doh.minTtl >= 0 ? QStringLiteral("%1s").arg(doh.minTtl) : QStringLiteral("?"))
        .arg(doh.hasCname ? doh.cnameChain.join(QStringLiteral(" → ")) : QStringLiteral("None")));

    if (!localUdpIp.isEmpty())
        r.output.append(QStringLiteral("    Local UDP: %1 (%2ms)")
            .arg(localUdpIp).arg(localUdpMs));

    if (!triggeredSignals.isEmpty()) {
        r.output.append(QStringLiteral("    Anomalies (%1):").arg(triggeredSignals.size()));
        for (const auto& d : triggeredSignals)
            r.output.append(QStringLiteral("      · %1").arg(d));
    } else {
        r.output.append(QStringLiteral("    No Anomalies Detected."));
    }

    return r;
}

// ── Diagnostic entry point ────────────────────────────────────────────

DiagnosticResult dnsIntegrity(DiagId id) {
    DiagnosticResult r; r.id = id; r.group = DiagGroup::G3;
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();
    QStringList out;

    out.append(QStringLiteral("DNS Integrity Check"));
    out.append(QStringLiteral("================================"));
    out.append(QString());

    int hijackWarn = 0, pollutionWarn = 0;
    int hijackClean = 0, pollutionClean = 0, pollutionSuspicious = 0;
    int hijackTimeout = 0, pollutionErrors = 0;
    QStringList hijackIPs, pollutionDetails;

    // ═══════════════════════════════════════════════════════════════════
    // Phase 1: ISP DNS Hijacking (NXDOMAIN hijack test)
    // ═══════════════════════════════════════════════════════════════════
    out.append(QStringLiteral("── Phase 1: ISP DNS Hijacking ──"));
    out.append(QStringLiteral("Fake Domains That Resolve to an IP Indicate ISP DNS Hijacking."));
    out.append(QString());

    // 5WHY: Date+hash-hex domains are unique per day per index, making
    // preemptive whitelisting impractical — any IP returned = hijacking.
    QString datePrefix = QDateTime::currentDateTime().toString("yyyyMMdd");
    QString domains[3];
    for (int i = 0; i < 3; ++i) {
        QByteArray seed = QStringLiteral("%1-%2-dns-hijack-test")
            .arg(datePrefix).arg(i).toUtf8();
        QString hex = QString::fromLatin1(
            QCryptographicHash::hash(seed, QCryptographicHash::Md5).toHex().left(4));
        static const char* tlds[] = {"com", "org", "net"};
        domains[i] = QStringLiteral("%1-%2-dns-test.%3")
            .arg(datePrefix, hex, tlds[i]);
    }

    for (const auto& domain : domains) {
        QElapsedTimer probe; probe.start();
        QString ip = DnsResolver::instance().resolve(domain, 3000);
        int elapsed = static_cast<int>(probe.elapsed());
        if (!ip.isEmpty()) {
            out.append(QStringLiteral("  %1 → HIJACKED: %2 (%3ms)").arg(domain, ip).arg(elapsed));
            hijackWarn++;
            if (!hijackIPs.contains(ip)) hijackIPs.append(ip);
        } else if (elapsed >= 3000) {
            out.append(QStringLiteral("  %1 → TIMEOUT (%2ms)").arg(domain).arg(elapsed));
            hijackTimeout++;
        } else {
            out.append(QStringLiteral("  %1 → Not Resolved").arg(domain));
            hijackClean++;
        }
    }
    out.append(QString());

    // ═══════════════════════════════════════════════════════════════════
    // Phase 2: DNS Integrity (multi-signal cross-verification)
    // ═══════════════════════════════════════════════════════════════════
    out.append(QStringLiteral("── Phase 2: DNS Integrity Check ──"));
    out.append(QStringLiteral("Multi-Signal Cross-Verification: CNAME Chain, TTL, Response Timing, and TLS Certificate."));
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

    // 5WHY: 5 test domains were analyzed SEQUENTIALLY — each domain's
    // DoH query + local DNS + scoring is fully independent (different
    // hosts, no shared state).  Parallelizing with QtConcurrent cuts
    // Phase 2 worst case from 5×max(per-domain) to max(per-domain).
    // With parallel DoH (#3) each domain completes in ~4s worst case,
    // so all 5 finish in ~4s instead of ~20s.
    static const int kDomCount = sizeof(kTestDomains) / sizeof(kTestDomains[0]);
    struct DomainResult {
        QStringList lines;
        enum Tag { Error, PrivateIp, Scored } tag = Error;
        DnsIntegrityResult::Verdict verdict = DnsIntegrityResult::Verdict::DNS_INTEGRITY_CLEAN;
        int scorePercent = 0;
        QString dohIps;
        QString localUdpIp;
        QString domain;
    };
    QFuture<DomainResult> domFutures[kDomCount];
    for (int i = 0; i < kDomCount; ++i) {
        domFutures[i] = QtConcurrent::run([td = kTestDomains[i]]() -> DomainResult {
            DomainResult dr;
            dr.domain = QString::fromUtf8(td.domain);
            DohDnsFullResult doh = dohQueryFull(dr.domain);
            QElapsedTimer probe; probe.start();
            QString localUdpIp = DnsResolver::instance().resolve(dr.domain, 3000);
            int localMs = static_cast<int>(probe.elapsed());
            dr.localUdpIp = localUdpIp;

            if (doh.aRecords.isEmpty()) {
                dr.lines.append(QStringLiteral("  %1 (%2) — DoH Query Failed, Skipped")
                    .arg(td.domain, td.description));
                dr.tag = DomainResult::Error;
            } else if (localUdpIp.isEmpty()) {
                dr.lines.append(QStringLiteral("  %1 (%2) — Local DNS Failed, Skipped")
                    .arg(td.domain, td.description));
                dr.tag = DomainResult::Error;
            } else if (isPrivateIp(localUdpIp)) {
                dr.lines.append(QStringLiteral("  %1 (%2) — Local=%3 → HIJACKED (Private IP)")
                    .arg(td.domain, td.description, localUdpIp));
                dr.tag = DomainResult::PrivateIp;
            } else {
                DnsIntegrityResult ir = scoreDnsIntegrity(
                    dr.domain, QString::fromUtf8(td.description),
                    doh, localUdpIp, localMs);
                dr.lines = ir.output;
                dr.verdict = ir.verdict;
                dr.scorePercent = ir.scorePercent;
                dr.dohIps = ir.dohIps.join(',');
                dr.tag = DomainResult::Scored;
            }
            dr.lines.append(QString());
            return dr;
        });
    }

    for (int i = 0; i < kDomCount; ++i) {
        DomainResult dr = domFutures[i].result();
        for (const auto& line : dr.lines)
            out.append(line);

        switch (dr.tag) {
        case DomainResult::Error:
            pollutionErrors++; break;
        case DomainResult::PrivateIp:
            pollutionDetails.append(QStringLiteral("%1: local=%2 (private IP)")
                .arg(dr.domain, dr.localUdpIp));
            pollutionWarn++; break;
        case DomainResult::Scored:
            switch (dr.verdict) {
            case DnsIntegrityResult::Verdict::DNS_INTEGRITY_CLEAN:
                pollutionClean++; break;
            case DnsIntegrityResult::Verdict::DNS_INTEGRITY_SUSPECT:
                pollutionSuspicious++; break;
            case DnsIntegrityResult::Verdict::DNS_INTEGRITY_TAMPERED:
                pollutionWarn++;
                pollutionDetails.append(QStringLiteral("%1: score=%2%, DoH=%3, Local=%4")
                    .arg(dr.domain).arg(dr.scorePercent)
                    .arg(dr.dohIps, dr.localUdpIp));
                break;
            case DnsIntegrityResult::Verdict::DNS_INTEGRITY_HIJACKED:
                pollutionWarn++;
                pollutionDetails.append(QStringLiteral("%1: HIJACKED (score=%2%)")
                    .arg(dr.domain).arg(dr.scorePercent));
                break;
            }
            break;
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // Combined verdict
    // ═══════════════════════════════════════════════════════════════════
    out.append(QStringLiteral("=================================================================="));
    out.append(QStringLiteral("Phase 1 (ISP Hijack):  %1 clean, %2 hijacked, %3 timeout")
        .arg(hijackClean).arg(hijackWarn).arg(hijackTimeout));
    {
        QString s = QStringLiteral("Phase 2 (DNS Integrity): %1 clean, %2 warned, %3 errors")
            .arg(pollutionClean).arg(pollutionWarn).arg(pollutionErrors);
        if (pollutionSuspicious > 0)
            s += QStringLiteral(", %1 suspicious").arg(pollutionSuspicious);
        out.append(s);
    }

    bool hijackDetected = hijackWarn > 0;
    bool pollutionDetected = pollutionWarn > 0;
    bool phase2AllFailed = (pollutionErrors > 0 && pollutionClean == 0
                            && pollutionWarn == 0 && pollutionSuspicious == 0);

    if (hijackDetected && pollutionDetected) {
        out.append(QStringLiteral("Verdict: DNS HIJACKING + POLLUTION detected"));
        out.append(QString());
        out.append(QStringLiteral("  ISP Hijack IPs: %1").arg(hijackIPs.join(QStringLiteral(", "))));
        for (const auto& d : pollutionDetails)
            out.append(QStringLiteral("  • %1").arg(d));
        r.status = DiagStatus::Warning;
        r.summary = QStringLiteral("DNS: hijack + pollution");
    } else if (hijackDetected) {
        out.append(QStringLiteral("Verdict: ISP DNS HIJACKING detected"));
        out.append(QStringLiteral("  Hijack IPs: %1").arg(hijackIPs.join(QStringLiteral(", "))));
        r.status = DiagStatus::Warning;
        r.summary = QStringLiteral("DNS hijack: %1 IP(s)").arg(hijackIPs.size());
    } else if (pollutionDetected) {
        int phase2Total = pollutionWarn + pollutionClean + pollutionSuspicious + pollutionErrors;
        out.append(QStringLiteral("Verdict: DNS POLLUTION — %1/%2 domains affected")
            .arg(pollutionWarn).arg(phase2Total));
        r.status = DiagStatus::Warning;
        r.summary = QStringLiteral("DNS polluted: %1/%2 domains").arg(pollutionWarn)
            .arg(phase2Total);
    } else {
        int totalErrors = hijackTimeout + pollutionErrors;
        if (totalErrors > 0 && hijackClean + pollutionClean + pollutionSuspicious == 0) {
            out.append(QStringLiteral("Verdict: INCONCLUSIVE — all queries failed"));
            r.status = DiagStatus::Info;
            r.summary = QStringLiteral("DNS: all queries failed");
        } else if (phase2AllFailed) {
            out.append(QStringLiteral("Verdict: Phase 1 clean, Phase 2 inconclusive (all DoH queries failed)"));
            r.status = DiagStatus::Info;
            r.summary = QStringLiteral("DNS: hijack clean, pollution inconclusive");
        } else if (pollutionSuspicious > 0) {
            out.append(QStringLiteral("Verdict: SUSPICIOUS — %1 domain(s) need manual check")
                .arg(pollutionSuspicious));
            r.status = DiagStatus::Info;
            r.summary = QStringLiteral("DNS: %1 suspicious").arg(pollutionSuspicious);
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

} // namespace SystemDiagnostics
