// =============================================================================
// G3/Adapters.cpp — G3 Internet & DNS adapters (real implementations)
//
// Ported from the archived G3*.cpp behavioral code into the new adapter
// structure (RunContext signature, per-test contract).  All logic is
// self-contained: raw DNS over UDP (query build + parse), DoH over HTTPS
// (JSON), TLS certificate verification, GeoIP HTTPS providers, and a
// tiered HTTP download/upload speed test.  No libcurl required.
//
// Platforms per NEW-1: DnsServers/DnsIntegrity/GeoIPLoc/Connectivity = All;
// DnsCache = Desktop | Android.
// =============================================================================
#if defined(_WIN32)
#if !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0601   // GetAdaptersAddresses requires Vista+
#endif
#endif

#include "Common/Services/PlatformAdapter.h"
#include "Common/Services/DnsWire.h"
#include "Common/Model/DiagnosticMeta.h"
#include "Common/Model/DiagNames.h"
#include "Diagnostics/Model/GeoProbe.h"
#include "Diagnostics/Model/ProbeConfig.h"

#if defined(PLATFORM_ANDROID)
#include "Diagnostics/Model/G5/Platform/Android/NetworkDiagnostics.h"
#endif

#include <QProcess>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QUdpSocket>
#include <QHostAddress>
#include <QSslSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSslSocket>
#if defined(_WIN32)
// 5WHY (复核 2026-08-19 Windows 编译): probeNetskopeStatus 使用
// CreateToolhelp32Snapshot/PROCESSENTRY32W——仅 <tlhelp32.h> 声明（GBase.h
// 不在此 TU 包含链）。漏含在 MSVC 下 C3861/C2065 编译失败（Apple CI 不覆盖）。
#include <tlhelp32.h>
#endif
#include <QSslCertificate>
#include <QTcpSocket>
#include <QHostInfo>
#include <QHash>
#include <QCryptographicHash>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>

#include <thread>
#include <vector>
#include <cstring>
#include <algorithm>
#include <climits>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#endif
#if defined(__linux__)
#include <unistd.h>           // isatty (resolvectl privilege guard)
#endif
#if defined(PLATFORM_IOS) && defined(__APPLE__)
#include <resolv.h>           // res_ninit (iOS resolver state)
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

using namespace PlatformFlag;

namespace g3 {

// ── Result helper ──────────────────────────────────────────────────────────
static DiagnosticResult makeResult(DiagId id, DiagStatus status,
                                   const QString& summary,
                                   const QVector<ResultProperty>& props,
                                   const QString& details) {
    DiagnosticResult r;
    r.id = id; r.displayName = diagDisplayName(id); r.group = diagGroup(id);
    r.status = status; r.summary = summary; r.properties = props;
    r.details = details; r.rawOutput = details;
    r.timestamp = QDateTime::currentDateTime();
    return r;
}

// Raw-DNS wire helpers now live in the shared service DnsWire.h (R5-6)：
// dnsWire::udpQuery(domain, qtype, server) → dnsWire::Answer
// （aRecords/cnameChain/hasCname/minTtl/rcode/records/elapsedMs）。

// ── Synchronous HTTPS GET via blocking QSslSocket ────────────────────────
// 5WHY (SIGSEGV in pool threads): the previous QNetworkAccessManager +
// QEventLoop implementation created a QEventDispatcherWin32 inside raw
// std::threads (detectCountry / DoH workers).  Thread exit then raced Qt's
// lazy collator/locale initialization (crash in QCollator::sortKey →
// RtlExitUserThread).  Blocking QSslSocket waitFor* calls need NO event
// loop at all and are documented-safe in non-QThread-managed threads.
static QByteArray httpsGetSync(const QString& url, int timeoutMs,
                               const QByteArray& accept = QByteArray()) {
    const QUrl u(url);
    const int port = u.port(u.scheme() == QLatin1String("https") ? 443 : 80);
    QSslSocket sock;
    sock.setPeerVerifyMode(QSslSocket::VerifyNone);
    sock.connectToHostEncrypted(u.host(), (quint16)port);
    if (!sock.waitForEncrypted(timeoutMs)) return {};

    QByteArray req;
    req += "GET ";
    req += u.path(QUrl::FullyEncoded).toUtf8();
    if (u.hasQuery()) {
        req += '?';
        req += u.query(QUrl::FullyEncoded).toUtf8();
    }
    req += " HTTP/1.1\r\n";
    req += "Host: " + u.host().toUtf8() + "\r\n";
    if (!accept.isEmpty()) req += "accept: " + accept + "\r\n";
    req += "User-Agent: NetDiagnostics/1.0\r\n";
    req += "Connection: close\r\n\r\n";
    if (sock.write(req) < req.size()) return {};

    QByteArray all;
    QElapsedTimer t; t.start();
    while (t.elapsed() < timeoutMs) {
        if (!sock.waitForReadyRead(qMin<qint64>(300, timeoutMs - t.elapsed()))) break;
        all += sock.readAll();
    }
    sock.disconnectFromHost();
    const int hdrEnd = all.indexOf("\r\n\r\n");
    if (hdrEnd < 0) return {};
    return all.mid(hdrEnd + 4);
}

// ── DoH (trusted resolver) query — JSON API, Cloudflare → Google → AliDNS ─
static dnsWire::Answer dohQueryFull(const QString& domain, int timeoutMs = 4000) {
    dnsWire::Answer a;
    static const char* kUrls[] = {
        "https://1.1.1.1/dns-query?name=%1&type=A",
        "https://8.8.8.8/resolve?name=%1&type=A",
        "https://223.5.5.5/resolve?name=%1&type=A",   // AliDNS DoH (CN reachable)
    };
    for (const char* fmt : kUrls) {
        const QString url = QString::fromLatin1(fmt).arg(domain);
        QElapsedTimer t; t.start();
        const QByteArray body = httpsGetSync(url, timeoutMs,
            QByteArrayLiteral("application/dns-json"));
        a.elapsedMs = (int)t.elapsed();
        if (body.isEmpty()) continue;
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isObject()) continue;
        const QJsonArray answers = doc.object().value(QStringLiteral("Answer")).toArray();
        if (answers.isEmpty()) continue;
        bool hasA = false;
        for (const auto& v : answers) {
            if (!v.isObject()) continue;
            const QJsonObject o = v.toObject();
            const int type = o.value(QStringLiteral("type")).toInt();
            const int ttl  = o.value(QStringLiteral("TTL")).toInt();
            if (type == 1) {
                a.aRecords.append(o.value(QStringLiteral("data")).toString());
                if (a.minTtl < 0 || ttl < a.minTtl) a.minTtl = ttl;
                hasA = true;
            } else if (type == 5) {
                a.cnameChain.append(o.value(QStringLiteral("data")).toString());
                a.hasCname = true;
            }
        }
        if (hasA) return a;
    }
    return a;   // empty aRecords = query failed
}

// ── TLS certificate domain check (definitive hijack signal) ───────────────
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
    const auto sans = cert.subjectAlternativeNames();
    const auto sanValues = sans.values();
    for (const auto& san : sanValues) {
        if (san == domain || (san.startsWith(QLatin1String("*.")) && domain.endsWith(san.mid(1))))
            return {};
    }
    const auto cns = cert.subjectInfo(QSslCertificate::CommonName);
    for (const auto& cn : cns) {
        if (cn == domain || (cn.startsWith(QLatin1String("*.")) && domain.endsWith(cn.mid(1))))
            return {};
    }
    const QString actualCn = cns.isEmpty() ? QStringLiteral("unknown") : cns.first();
    return sanValues.isEmpty()
        ? QStringLiteral("TLS Cert CN=%1 ≠ %2").arg(actualCn, domain)
        : QStringLiteral("TLS Cert SAN=%1 / CN=%2 ≠ %3").arg(sanValues.first(), actualCn, domain);
}

// ═════════════════════════════════════════════════════════════════════════
// G3DnsServers — DNS server configuration per interface
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeDnsServers(DiagId id, const QString&, RunContext& ctx) {
    QVector<ResultProperty> props;
    QStringList dnsList;
    QVariantList serverList;

    auto appendServer = [&](const QString& source, const QString& ip) {
        props.append({source, ip});
        QVariantMap m;
        m[QStringLiteral("source")] = source;
        m[QStringLiteral("ip")] = ip;
        serverList.append(m);
        if (!dnsList.contains(ip)) dnsList.append(ip);
    };

#if defined(_WIN32)
    ULONG bufLen = 15000;
    QByteArray buf((int)bufLen, '\0');
    auto* adapters = (PIP_ADAPTER_ADDRESSES)buf.data();
    ULONG rc = GetAdaptersAddresses(AF_UNSPEC,
            GAA_FLAG_SKIP_UNICAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_ANYCAST,
            nullptr, adapters, &bufLen);
    if (rc == ERROR_BUFFER_OVERFLOW) {
        buf.resize((int)bufLen);
        adapters = (PIP_ADAPTER_ADDRESSES)buf.data();
        rc = GetAdaptersAddresses(AF_UNSPEC,
                GAA_FLAG_SKIP_UNICAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_ANYCAST,
                nullptr, adapters, &bufLen);
    }
    if (rc == NO_ERROR) {
        for (auto* a = adapters; a; a = a->Next) {
            if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
            const QString ifName = QString::fromWCharArray(a->FriendlyName);
            for (auto* dns = a->FirstDnsServerAddress; dns; dns = dns->Next) {
                char ip[64]; DWORD ipLen = sizeof(ip);
                if (WSAAddressToStringA(dns->Address.lpSockaddr, dns->Address.iSockaddrLength,
                        nullptr, ip, &ipLen) == 0)
                    appendServer(ifName, QString::fromLatin1(ip));
            }
        }
    }
#else
#if defined(PLATFORM_IOS) && defined(__APPLE__)
    struct __res_state res;
    std::memset(&res, 0, sizeof(res));
    if (res_ninit(&res) == 0) {
        for (int i = 0; i < res.nscount; ++i) {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &res.nsaddr_list[i].sin_addr, ip, sizeof(ip));
            appendServer(QStringLiteral("System DNS"), QString::fromLatin1(ip));
        }
        res_nclose(&res);
    }
#else
    QFile resolv(QStringLiteral("/etc/resolv.conf"));
    if (resolv.open(QIODevice::ReadOnly)) {
        QTextStream ts(&resolv);
        while (!ts.atEnd()) {
            if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
            const QString line = ts.readLine().trimmed();
            if (line.startsWith(QLatin1String("nameserver ")))
                appendServer(QStringLiteral("resolv.conf"), line.mid(11));
        }
    }
    if (QFile::exists(QStringLiteral("/run/systemd/resolve/resolv.conf")))
        appendServer(QStringLiteral("systemd-resolved"), QStringLiteral("(stub resolver active)"));
#endif
#endif

    DiagnosticResult r = makeResult(id, dnsList.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass,
        dnsList.isEmpty() ? QStringLiteral("No DNS servers found")
                          : QStringLiteral("DNS: %1").arg(dnsList.join(QStringLiteral(", "))),
        props, QStringLiteral("DNS servers: %1").arg(dnsList.join(QStringLiteral(", "))));
    r.data[QStringLiteral("servers")] = serverList;
    r.data[QStringLiteral("serverCount")] = dnsList.size();
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G3DnsCache — DNS client cache contents (honest: no fabricated counts)
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeDnsCache(DiagId id, const QString&, RunContext& ctx) {
    QStringList out;
    int cacheEntries = 0;
    bool hasCache = false;

#if defined(_WIN32)
    out.append(QStringLiteral("Windows IP Configuration — DNS Client Cache (ipconfig /displaydns)"));
    QProcess dnsProc;
    dnsProc.start(QStringLiteral("ipconfig"), QStringList() << QStringLiteral("/displaydns"));
    if (dnsProc.waitForFinished(10000)) {
        const QString dnsOut = QString::fromLocal8Bit(dnsProc.readAllStandardOutput());
        if (!dnsOut.trimmed().isEmpty()) {
            hasCache = true;
            for (const auto& line : dnsOut.split(QLatin1Char('\n'))) {
                const QString t = line.trimmed();
                if (t.startsWith(QLatin1String("Record Name"))) ++cacheEntries;
                if (!t.isEmpty()) out.append(t);
            }
        } else {
            out.append(QStringLiteral("  (DNS cache is empty)"));
        }
    } else {
        dnsProc.kill();
        dnsProc.waitForFinished(2000);   // R5-1: never destroy a running QProcess
        out.append(QStringLiteral("  (Unable to retrieve DNS cache — timeout)"));
    }
    out.append(QString());
    out.append(QStringLiteral("To flush: ipconfig /flushdns"));
#else
    out.append(QStringLiteral("DNS Cache Information"));
    // 5WHY: resolvectl needs polkit admin auth; only attempt when a human
    // can answer (interactive TTY, not headless/CI) — ported from archive.
    // Linux-only: macOS/iOS have no systemd-resolved (compile guard kept
    // the QProcess out of the iOS build in the archive).
    bool skipPrivileged = qEnvironmentVariableIsSet("ND_SKIP_RESOLVECTL")
                       || qEnvironmentVariableIsSet("ND_AUTO_TEST");
#if defined(__linux__)
    skipPrivileged = skipPrivileged || (::isatty(STDIN_FILENO) == 0);
#endif
    QByteArray data;
#if defined(__linux__)
    if (!skipPrivileged) {
        QProcess proc;
        proc.setStandardInputFile(QProcess::nullDevice());
        proc.start(QStringLiteral("resolvectl"), QStringList() << QStringLiteral("statistics"));
        if (!proc.waitForFinished(5000)) {
            proc.kill();
            proc.waitForFinished(2000);
            proc.start(QStringLiteral("systemd-resolve"), QStringList() << QStringLiteral("--statistics"));
            if (!proc.waitForFinished(5000)) {
                proc.kill();
                proc.waitForFinished(2000);
            }
        }
        data = proc.readAllStandardOutput();
    }
#endif
    if (!data.trimmed().isEmpty()) {
        hasCache = true;
        out.append(QStringLiteral("systemd-resolved Cache Statistics"));
        for (const auto& line : QString::fromLatin1(data).split(QLatin1Char('\n')))
            if (!line.trimmed().isEmpty()) out.append(QStringLiteral("    %1").arg(line.trimmed()));
    } else {
        out.append(QStringLiteral("DNS Resolution Configuration"));
        if (QFile::exists(QStringLiteral("/var/db/nscd/hosts")) ||
            QFile::exists(QStringLiteral("/var/cache/nscd/hosts")))
            out.append(QStringLiteral("    nscd: active"));
        if (QFile::exists(QStringLiteral("/var/lib/misc/dnsmasq.leases")))
            out.append(QStringLiteral("    dnsmasq: active"));
        QFile resolv(QStringLiteral("/etc/resolv.conf"));
        if (resolv.open(QIODevice::ReadOnly)) {
            QTextStream ts(&resolv);
            while (!ts.atEnd()) {
                if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
                const QString line = ts.readLine().trimmed();
                if (!line.isEmpty() && !line.startsWith(QLatin1Char('#')))
                    out.append(QStringLiteral("    %1").arg(line));
            }
        }
        QFile hosts(QStringLiteral("/etc/hosts"));
        int hostEntries = 0;
        if (hosts.open(QIODevice::ReadOnly)) {
            QTextStream ts(&hosts);
            while (!ts.atEnd()) {
                const QString line = ts.readLine().trimmed();
                if (!line.isEmpty() && !line.startsWith(QLatin1Char('#')) && line.contains(QLatin1Char(' ')))
                    ++hostEntries;
            }
        }
        if (hostEntries > 0)
            out.append(QStringLiteral("    /etc/hosts: %1 static mappings").arg(hostEntries));
    }
#endif

    DiagnosticResult r = makeResult(id, hasCache ? DiagStatus::Pass : DiagStatus::Info,
        hasCache ? QStringLiteral("Cache Active — see details above")
                 : QStringLiteral("No Local DNS Cache Detected"),
        {}, out.join(QLatin1Char('\n')));
    r.data[QStringLiteral("cacheActive")] = hasCache;
    r.data[QStringLiteral("cacheEntries")] = cacheEntries;
    r.data[QStringLiteral("cacheType")] =
#if defined(_WIN32)
        QStringLiteral("ipconfig");
#else
        hasCache ? QStringLiteral("systemd-resolved") : QStringLiteral("resolver-config");
#endif
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G3DnsIntegrity — two-phase hijack/pollution detection with scoring engine
// ═════════════════════════════════════════════════════════════════════════
static bool isPrivateIp(const QString& ip) {
    if (ip.startsWith(QLatin1String("127.")) || ip.startsWith(QLatin1String("10."))) return true;
    if (ip.startsWith(QLatin1String("192.168."))) return true;
    if (ip.startsWith(QLatin1String("172."))) {
        const int dot = ip.indexOf(QLatin1Char('.'), 4);
        if (dot > 4) {
            const int second = ip.mid(4, dot - 4).toInt();
            if (second >= 16 && second <= 31) return true;
        }
    }
    if (ip.startsWith(QLatin1String("100."))) {
        const int dot = ip.indexOf(QLatin1Char('.'), 4);
        if (dot > 4) {
            const int second = ip.mid(4, dot - 4).toInt();
            if (second >= 64 && second <= 127) return true;
        }
    }
    return ip == QLatin1String("0.0.0.0");
}

struct IntegritySignal {
    int     weight;
    bool    triggered;
    QString detail;
};

struct IntegrityResult {
    enum class Verdict { Clean, Suspect, Tampered, Hijacked };
    Verdict verdict = Verdict::Clean;
    int scorePercent = 0;
    QStringList output;
    QStringList dohIps;
    QString localUdpIp;
};

static IntegrityResult scoreIntegrity(const QString& domain, const QString& description,
                                      const dnsWire::Answer& doh,
                                      const QString& localIp, int localMs) {
    IntegrityResult r;
    r.dohIps = doh.aRecords;
    r.localUdpIp = localIp;

    QVector<IntegritySignal> sigVec;
    {   // TLS cert mismatch (weight 5) — definitive signal
        IntegritySignal s{5, false, {}};
        if (!localIp.isEmpty()) {
            const QString res = tlsCheckCert(localIp, domain, 3000);
            const bool mismatch = !res.isEmpty() && res != QLatin1String("no TLS");
            s.triggered = mismatch;
            if (mismatch) s.detail = res;
        }
        sigVec.append(s);
    }
    {   // CNAME anomaly (weight 5) — pollution returns bare A records
        const bool anomaly = !doh.hasCname;
        sigVec.append({5, anomaly, anomaly
            ? QStringLiteral("No CNAME Chain — DoH Returned Bare A-Record (Legitimate CDN Domains Normally Have CNAME Indirection)")
            : QString()});
    }
    {   // TTL anomaly (weight 3) — injection sets TTL≈0
        const bool low = (doh.minTtl >= 0 && doh.minTtl < 10);
        sigVec.append({3, low, low
            ? QStringLiteral("TTL=%1s Abnormally Low (Normal: 60-3600s)").arg(doh.minTtl)
            : QString()});
    }
    {   // Timing anomaly (weight 2) — injection arrives near-instantly
        const bool tooFast = (localMs > 0 && localMs < 15);
        sigVec.append({2, tooFast, tooFast
            ? QStringLiteral("UDP Response %1ms (Suspiciously Fast — Legitimate Recursion Takes 30ms+)").arg(localMs)
            : QString()});
    }

    int totalW = 0, triggeredW = 0;
    QStringList triggered;
    for (const auto& s : sigVec) {
        totalW += s.weight;
        if (s.triggered) { triggeredW += s.weight; triggered.append(s.detail); }
    }
    r.scorePercent = totalW > 0 ? triggeredW * 100 / totalW : 0;
    if (r.scorePercent > 60)      r.verdict = IntegrityResult::Verdict::Hijacked;
    else if (r.scorePercent > 33) r.verdict = IntegrityResult::Verdict::Tampered;
    else if (r.scorePercent > 14) r.verdict = IntegrityResult::Verdict::Suspect;
    else                          r.verdict = IntegrityResult::Verdict::Clean;

    const char* label = r.verdict == IntegrityResult::Verdict::Clean    ? "Clean"
                      : r.verdict == IntegrityResult::Verdict::Suspect  ? "Suspicious"
                      : r.verdict == IntegrityResult::Verdict::Tampered ? "POLLUTED"
                                                                        : "HIJACKED";
    r.output.append(QStringLiteral("  %1 (%2) — Score: %3% → %4")
        .arg(domain, description).arg(r.scorePercent).arg(QLatin1String(label)));
    r.output.append(QStringLiteral("    DoH: %1 (TTL=%2, CNAME=%3)")
        .arg(doh.aRecords.isEmpty() ? QStringLiteral("No Response") : doh.aRecords.join(QLatin1Char(',')))
        .arg(doh.minTtl >= 0 ? QStringLiteral("%1s").arg(doh.minTtl) : QStringLiteral("?"))
        .arg(doh.hasCname ? doh.cnameChain.join(QStringLiteral(" → ")) : QStringLiteral("None")));
    if (!localIp.isEmpty())
        r.output.append(QStringLiteral("    Local UDP: %1 (%2ms)").arg(localIp).arg(localMs));
    if (!triggered.isEmpty()) {
        r.output.append(QStringLiteral("    Anomalies (%1):").arg(triggered.size()));
        for (const auto& d : triggered)
            r.output.append(QStringLiteral("      · %1").arg(d));
    } else {
        r.output.append(QStringLiteral("    No Anomalies Detected."));
    }
    return r;
}

static DiagnosticResult probeDnsIntegrity(DiagId id, const QString&, RunContext& ctx) {
    QStringList out;
    out.append(QStringLiteral("DNS Integrity Check"));

    // First system DNS server = the resolver under test.
    QString testServer;
    {
#if defined(_WIN32)
        ULONG bufLen = 15000;
        QByteArray buf((int)bufLen, '\0');
        auto* adapters = (PIP_ADAPTER_ADDRESSES)buf.data();
        if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_UNICAST | GAA_FLAG_SKIP_MULTICAST
                | GAA_FLAG_SKIP_ANYCAST, nullptr, adapters, &bufLen) == NO_ERROR) {
            for (auto* a = adapters; a && testServer.isEmpty(); a = a->Next) {
                if (a->FirstDnsServerAddress) {
                    char ip[64]; DWORD ipLen = sizeof(ip);
                    if (WSAAddressToStringA(a->FirstDnsServerAddress->Address.lpSockaddr,
                            a->FirstDnsServerAddress->Address.iSockaddrLength,
                            nullptr, ip, &ipLen) == 0)
                        testServer = QString::fromLatin1(ip);
                }
            }
        }
#else
        QFile resolv(QStringLiteral("/etc/resolv.conf"));
        if (resolv.open(QIODevice::ReadOnly)) {
            QTextStream ts(&resolv);
            while (!ts.atEnd()) {
                const QString line = ts.readLine().trimmed();
                if (line.startsWith(QLatin1String("nameserver ")) && testServer.isEmpty())
                    testServer = line.mid(11);
            }
        }
#endif
    }
    out.append(QStringLiteral("Resolver under test: %1")
        .arg(testServer.isEmpty() ? QStringLiteral("(none found)") : testServer));

    // ══ Phase 1: ISP DNS Hijacking (NXDOMAIN hijack) ══
    int hijackClean = 0, hijackWarn = 0, hijackTimeout = 0;
    QStringList hijackIPs;
    out.append(QString());
    out.append(QStringLiteral("── Phase 1: ISP DNS Hijacking ──"));
    const QString datePrefix = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd"));
    for (int i = 0; i < 3; ++i) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        const QByteArray seed = QStringLiteral("%1-%2-dns-hijack-test").arg(datePrefix).arg(i).toUtf8();
        const QString hex = QString::fromLatin1(
            QCryptographicHash::hash(seed, QCryptographicHash::Md5).toHex().left(4));
        static const char* tlds[] = {"com", "org", "net"};
        const QString domain = QStringLiteral("%1-%2-dns-test.%3").arg(datePrefix, hex, tlds[i]);
        const dnsWire::Answer ans = dnsWire::udpQuery(domain, 1, testServer, 3000);
        if (!ans.aRecords.isEmpty()) {
            out.append(QStringLiteral("  %1 → HIJACKED: %2 (%3ms)")
                .arg(domain, ans.aRecords.first()).arg(ans.elapsedMs));
            ++hijackWarn;
            if (!hijackIPs.contains(ans.aRecords.first())) hijackIPs.append(ans.aRecords.first());
        } else if (ans.elapsedMs >= 3000) {
            out.append(QStringLiteral("  %1 → TIMEOUT (%2ms)").arg(domain).arg(ans.elapsedMs));
            ++hijackTimeout;
        } else {
            out.append(QStringLiteral("  %1 → Not Resolved").arg(domain));
            ++hijackClean;
        }
    }

    // ══ Phase 2: multi-signal cross-verification (DoH vs local UDP) ══
    out.append(QString());
    out.append(QStringLiteral("── Phase 2: DNS Integrity Check ──"));
    static const struct { const char* domain; const char* description; } kTestDomains[] = {
        {"www.google.com",    "Search Engine"},
        {"www.youtube.com",   "Video Platform"},
        {"www.telegram.org",  "Messaging"},
        {"www.bbc.com",       "News Media"},
        {"www.wikipedia.org", "Knowledge Base"},
    };
    static const int kDomCount = (int)(sizeof(kTestDomains) / sizeof(kTestDomains[0]));
    enum class Tag { Error, PrivateIp, Scored };
    struct DomainResult {
        Tag tag = Tag::Error;
        IntegrityResult::Verdict verdict = IntegrityResult::Verdict::Clean;
        int scorePercent = 0;
        QString dohIps, localUdpIp, domain;
        QStringList lines;
    };
    DomainResult domResults[kDomCount];
    std::vector<std::thread> threads;
    threads.reserve(kDomCount);
    for (int i = 0; i < kDomCount; ++i) {
        try {
            threads.emplace_back([i, &domResults, testServer]() {
                DomainResult dr;
                dr.domain = QString::fromUtf8(kTestDomains[i].domain);
                const QString desc = QString::fromUtf8(kTestDomains[i].description);
                const dnsWire::Answer doh = dohQueryFull(dr.domain);
                // ── local resolution: raw UDP probe to the resolver (real timing) ──
                dnsWire::Answer localAns;
                if (!testServer.isEmpty())
                    localAns = dnsWire::udpQuery(dr.domain, 1, testServer, 3000);
                const int localMs = localAns.elapsedMs;
                dr.localUdpIp = localAns.aRecords.value(0);
                const bool resolved = !dr.localUdpIp.isEmpty();

                if (doh.aRecords.isEmpty()) {
                    dr.lines.append(QStringLiteral("  %1 (%2) — DoH Query Failed, Skipped").arg(dr.domain, desc));
                    dr.tag = Tag::Error;
                } else if (!resolved) {
                    dr.lines.append(QStringLiteral("  %1 (%2) — Local DNS Failed, Skipped").arg(dr.domain, desc));
                    dr.tag = Tag::Error;
                } else if (isPrivateIp(dr.localUdpIp)) {
                    dr.lines.append(QStringLiteral("  %1 (%2) — Local=%3 → HIJACKED (Private IP)")
                        .arg(dr.domain, desc, dr.localUdpIp));
                    dr.tag = Tag::PrivateIp;
                } else {
                    IntegrityResult ir = scoreIntegrity(dr.domain, desc, doh, dr.localUdpIp, localMs);
                    dr.lines = ir.output;
                    dr.verdict = ir.verdict;
                    dr.scorePercent = ir.scorePercent;
                    dr.dohIps = ir.dohIps.join(QLatin1Char(','));
                    dr.tag = Tag::Scored;
                }
                dr.lines.append(QString());
                domResults[i] = dr;
            });
        } catch (const std::system_error&) {
            break;
        }
    }
    for (auto& th : threads) {
        try { if (th.joinable()) th.join(); } catch (...) {}
    }

    int pollutionClean = 0, pollutionWarn = 0, pollutionSuspicious = 0, pollutionErrors = 0;
    QStringList pollutionDetails;
    for (int i = 0; i < kDomCount; ++i) {
        const DomainResult& dr = domResults[i];
        for (const auto& line : dr.lines) out.append(line);
        switch (dr.tag) {
        case Tag::Error:          ++pollutionErrors; break;
        case Tag::PrivateIp:
            ++pollutionWarn;
            pollutionDetails.append(QStringLiteral("%1: local=%2 (private IP)").arg(dr.domain, dr.localUdpIp));
            break;
        case Tag::Scored:
            switch (dr.verdict) {
            case IntegrityResult::Verdict::Clean:    ++pollutionClean; break;
            case IntegrityResult::Verdict::Suspect:  ++pollutionSuspicious; break;
            case IntegrityResult::Verdict::Tampered:
                ++pollutionWarn;
                pollutionDetails.append(QStringLiteral("%1: score=%2%, DoH=%3, Local=%4")
                    .arg(dr.domain).arg(dr.scorePercent).arg(dr.dohIps, dr.localUdpIp));
                break;
            case IntegrityResult::Verdict::Hijacked:
                ++pollutionWarn;
                pollutionDetails.append(QStringLiteral("%1: HIJACKED (score=%2%)").arg(dr.domain).arg(dr.scorePercent));
                break;
            }
            break;
        }
    }

    // ══ Combined verdict ══
    out.append(QString());
    out.append(QStringLiteral("Phase 1 (ISP Hijack):  %1 clean, %2 hijacked, %3 timeout")
        .arg(hijackClean).arg(hijackWarn).arg(hijackTimeout));
    QString phase2 = QStringLiteral("Phase 2 (DNS Integrity): %1 clean, %2 warned, %3 errors")
        .arg(pollutionClean).arg(pollutionWarn).arg(pollutionErrors);
    if (pollutionSuspicious > 0) phase2 += QStringLiteral(", %1 suspicious").arg(pollutionSuspicious);
    out.append(phase2);

    const bool hijackDetected = hijackWarn > 0;
    const bool pollutionDetected = pollutionWarn > 0;
    const bool phase2AllFailed = (pollutionErrors > 0 && pollutionClean == 0
        && pollutionWarn == 0 && pollutionSuspicious == 0);

    DiagStatus status;
    QString summary;
    if (hijackDetected && pollutionDetected) {
        out.append(QStringLiteral("Verdict: DNS HIJACKING + POLLUTION detected"));
        status = DiagStatus::Warning; summary = QStringLiteral("DNS: hijack + pollution");
    } else if (hijackDetected) {
        out.append(QStringLiteral("Verdict: ISP DNS HIJACKING detected"));
        out.append(QStringLiteral("  Hijack IPs: %1").arg(hijackIPs.join(QStringLiteral(", "))));
        status = DiagStatus::Warning; summary = QStringLiteral("DNS hijack: %1 IP(s)").arg(hijackIPs.size());
    } else if (pollutionDetected) {
        const int total = pollutionWarn + pollutionClean + pollutionSuspicious + pollutionErrors;
        out.append(QStringLiteral("Verdict: DNS POLLUTION — %1/%2 domains affected")
            .arg(pollutionWarn).arg(total));
        status = DiagStatus::Warning;
        summary = QStringLiteral("DNS polluted: %1/%2 domains").arg(pollutionWarn).arg(total);
    } else {
        const int totalErrors = hijackTimeout + pollutionErrors;
        if (totalErrors > 0 && hijackClean + pollutionClean + pollutionSuspicious == 0) {
            out.append(QStringLiteral("Verdict: INCONCLUSIVE — all queries failed"));
            status = DiagStatus::Info; summary = QStringLiteral("DNS: all queries failed");
        } else if (phase2AllFailed) {
            out.append(QStringLiteral("Verdict: Phase 1 clean, Phase 2 inconclusive (all DoH queries failed)"));
            status = DiagStatus::Info;
            summary = QStringLiteral("DNS: hijack clean, pollution inconclusive");
        } else if (pollutionSuspicious > 0) {
            out.append(QStringLiteral("Verdict: SUSPICIOUS — %1 domain(s) need manual check").arg(pollutionSuspicious));
            status = DiagStatus::Info; summary = QStringLiteral("DNS: %1 suspicious").arg(pollutionSuspicious);
        } else {
            out.append(QStringLiteral("Verdict: DNS CLEAN — no hijacking or pollution detected"));
            status = DiagStatus::Pass; summary = QStringLiteral("DNS clean");
        }
    }

    DiagnosticResult r = makeResult(id, status, summary, {}, out.join(QLatin1Char('\n')));
    // overallScorePercent 驱动仪表盘：由最终裁决推导（含 Phase 1 劫持），
    // 不再用与裁决脱节的任意公式——仪表与结论必须一致。
    const int overall = (hijackDetected && pollutionDetected) ? 0
        : hijackDetected    ? 20
        : pollutionDetected ? 40
        : pollutionSuspicious > 0 ? 60
        : phase2AllFailed   ? 80
        : 100;
    r.data[QStringLiteral("phase1HijackDetected")] = hijackDetected;
    r.data[QStringLiteral("phase1Clean")] = hijackClean;
    r.data[QStringLiteral("phase1HijackWarn")] = hijackWarn;
    r.data[QStringLiteral("phase1Timeout")] = hijackTimeout;
    r.data[QStringLiteral("phase2Verdict")] =
        (hijackDetected && pollutionDetected) ? QStringLiteral("hijack+ pollution")
        : hijackDetected      ? QStringLiteral("hijack")
        : pollutionDetected   ? QStringLiteral("pollution")
        : pollutionSuspicious ? QStringLiteral("suspicious")
        : phase2AllFailed     ? QStringLiteral("inconclusive")
                              : QStringLiteral("clean");
    r.data[QStringLiteral("overallScorePercent")] = overall;
    // 摘要卡推导叙述：两阶段检测方法与结论依据（用户可复现判断链）
    r.narrative = QStringLiteral("Phase 1 (ISP hijack): %1 randomly-named test domains were resolved — "
        "%2 clean, %3 hijacked, %4 timeout. ")
        .arg(hijackClean + hijackWarn + hijackTimeout).arg(hijackClean).arg(hijackWarn).arg(hijackTimeout)
        + QStringLiteral("Phase 2 (pollution): %1 benchmark domains compared local UDP vs DoH — "
        "%2 clean, %3 polluted, %4 suspicious, %5 errors. ")
        .arg(pollutionClean + pollutionWarn + pollutionSuspicious + pollutionErrors)
        .arg(pollutionClean).arg(pollutionWarn).arg(pollutionSuspicious).arg(pollutionErrors)
        + (hijackDetected ? QStringLiteral("A hijack responder returned answers for non-existent domains (hijack IPs: %1). ")
            .arg(hijackIPs.join(QStringLiteral(", "))) : QStringLiteral("Non-existent domains did not resolve — no hijack responder found. "));
    if (pollutionDetected)
        r.narrative += QStringLiteral("Local answers diverged from DoH ground truth on %1 domain(s) — evidence of DNS pollution. ")
            .arg(pollutionWarn);
    r.narrative += QStringLiteral("Overall integrity score: %1/100.").arg(overall);
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G3GeoIPLoc — country/city/ISP via HTTPS + TCP-RTT VPN heuristic
// ═════════════════════════════════════════════════════════════════════════
static QString detectCountry(int timeoutMs = 5000) {
    static QString sCached;
    static QMutex sMutex;
    {
        QMutexLocker lock(&sMutex);
        if (!sCached.isEmpty() && sCached != QLatin1String("XX")) return sCached;
    }
    static const struct { const char* url; int parser; } providers[] = {
        {"https://ip-api.com/json/",         0},   // JSON countryCode
        {"https://ipapi.co/country/",        1},   // plain-text 2-letter
        {"https://ifconfig.co/country-iso",  1},
    };
    static const int kCount = (int)(sizeof(providers) / sizeof(providers[0]));
    QPair<int, QByteArray> results[kCount];
    std::vector<std::thread> threads;
    threads.reserve(kCount);
    for (int i = 0; i < kCount; ++i) {
        try {
            threads.emplace_back([i, timeout = timeoutMs, &results]() {
                results[i] = qMakePair(i, httpsGetSync(QString::fromUtf8(providers[i].url), timeout));
            });
        } catch (const std::system_error&) { break; }
    }
    for (auto& th : threads) {
        try { if (th.joinable()) th.join(); } catch (...) {}
    }
    for (int i = 0; i < kCount; ++i) {
        const auto& pr = results[i];
        const QByteArray body = pr.second;
        if (body.isEmpty()) continue;
        const QString text = QString::fromUtf8(body).trimmed();
        QString cc;
        if (providers[pr.first].parser == 0) {
            const QJsonDocument doc = QJsonDocument::fromJson(body);
            if (doc.isObject())
                cc = doc.object().value(QStringLiteral("countryCode")).toString();
        } else {
            if (text.size() == 2) cc = text.toUpper();
        }
        if (cc.size() == 2) {
            QMutexLocker lock(&sMutex);
            sCached = cc;
            return cc;
        }
    }
    QMutexLocker lock(&sMutex);
    sCached = QStringLiteral("XX");
    return sCached;
}

// TCP connect RTT to a host:port. Returns -1 when unreachable.
static int tcpConnectMs(const QString& host, int port, int timeoutMs = 3000) {
    QTcpSocket sock;
    QElapsedTimer t; t.start();
    sock.connectToHost(host, (quint16)port);
    if (!sock.waitForConnected(timeoutMs)) return -1;
    const int ms = (int)t.elapsed();
    sock.disconnectFromHost();
    return ms;
}

static DiagnosticResult probeGeoIPLoc(DiagId id, const QString&, RunContext& ctx) {
    QVector<ResultProperty> props;
    QStringList out;
    out.append(QStringLiteral("IP Geolocation (HTTPS providers)"));

    // ── Country / city / ISP via ip-api (JSON) ──
    QString cc, city, isp, asName, publicIp;
    double lat = 0.0, lon = 0.0;
    bool hasCoords = false;
    const QByteArray body = httpsGetSync(QStringLiteral("https://ip-api.com/json/?fields=status,country,countryCode,city,isp,as,query,lat,lon"), 5000);
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isObject() && doc.object().value(QStringLiteral("status")).toString() == QLatin1String("success")) {
        const QJsonObject o = doc.object();
        cc       = o.value(QStringLiteral("countryCode")).toString();
        city     = o.value(QStringLiteral("city")).toString();
        isp      = o.value(QStringLiteral("isp")).toString();
        asName   = o.value(QStringLiteral("as")).toString();
        publicIp = o.value(QStringLiteral("query")).toString();
        if (o.contains(QStringLiteral("lat")) && o.contains(QStringLiteral("lon"))) {
            lat = o.value(QStringLiteral("lat")).toDouble();
            lon = o.value(QStringLiteral("lon")).toDouble();
            hasCoords = true;
        }
    }
    if (cc.isEmpty()) cc = detectCountry(5000);
    props.append({QStringLiteral("Country Code"), cc.isEmpty() ? QStringLiteral("Unknown") : cc});
    out.append(QStringLiteral("  Country: %1").arg(cc.isEmpty() ? QStringLiteral("Unknown") : cc));
    if (!city.isEmpty())    { props.append({QStringLiteral("City"), city});     out.append(QStringLiteral("  City: %1").arg(city)); }
    if (!isp.isEmpty())     { props.append({QStringLiteral("ISP"), isp});       out.append(QStringLiteral("  ISP: %1").arg(isp)); }
    if (!asName.isEmpty())  { props.append({QStringLiteral("AS"), asName});     out.append(QStringLiteral("  AS: %1").arg(asName)); }
    if (hasCoords)          { props.append({QStringLiteral("Coordinates"),
        QStringLiteral("%1, %2").arg(lat, 0, 'f', 4).arg(lon, 0, 'f', 4)});
        out.append(QStringLiteral("  Coordinates: %1, %2").arg(lat, 0, 'f', 4).arg(lon, 0, 'f', 4)); }
    if (!publicIp.isEmpty()){ props.append({QStringLiteral("Public IP"), publicIp}); out.append(QStringLiteral("  Public IP: %1").arg(publicIp)); }

    // ── VPN heuristic（GeoProbe 管线：ProbeDatabase/Scheduler/Executor/Feedback）──
    // 138 台真实测速服务器库按国家选择 + 外国基线；管线失败回退固定表直连。
    QHash<QString, QVector<int>> rttByCc;
    QStringList serverLines;
    bool pipelineUsed = false;
    if (!(cc == QLatin1String("XX") || cc.isEmpty())) {
        ProbeConfig cfg;
        cfg.scope = ProbeConfig::ByCountry;
        cfg.scopeValue = cc;
        cfg.rounds = 2;
        cfg.topN = 6;
        cfg.aggregation = ProbeConfig::Aggregation::ByCountry;
        GeoProbe::instance().probe(cfg);
        const ProbeResult pr = GeoProbe::instance().getFeedback(cfg);
        for (const auto& s : pr.servers) {
            if (!s.ok || s.ttfbMs < 0) continue;
            const int ms = (int)s.ttfbMs;
            rttByCc[s.country].append(ms);
            serverLines.append(QStringLiteral("  [%1] %2:%3 — %4 ms")
                .arg(s.country, s.host).arg(s.port).arg(ms));
        }
        if (!rttByCc.isEmpty()) pipelineUsed = true;
        // 外国基线（3 台固定，ByServers 管线）
        ProbeConfig fcfg;
        fcfg.scope = ProbeConfig::ByServers;
        fcfg.serverHosts = {QStringLiteral("speedtest.choopa.net:8080"),
                            QStringLiteral("speedtest.sea01.softlayer.com:8080"),
                            QStringLiteral("speedtest.fra1.de.leaseweb.net:8080")};
        fcfg.rounds = 2;
        fcfg.topN = 3;
        GeoProbe::instance().probe(fcfg);
        const ProbeResult fpr = GeoProbe::instance().getFeedback(fcfg);
        for (const auto& s : fpr.servers) {
            if (!s.ok || s.ttfbMs < 0) continue;
            const int ms = (int)s.ttfbMs;
            rttByCc[s.country].append(ms);
            serverLines.append(QStringLiteral("  [%1] %2:%3 — %4 ms")
                .arg(s.country, s.host).arg(s.port).arg(ms));
        }
    }
    if (!pipelineUsed) {
        // 回退：固定服务器表直连扫描（诚实降级）
        static const struct { const char* cc; const char* host; int port; } kServers[] = {
            {"CN", "beijing.unicomtest.com", 8080},
            {"CN", "speedtest1.online.sh.cn", 8080},
            {"JP", "gisho.work.prod.hosts.ooklaserver.net", 8080},
            {"KR", "seoul.speedtest.gslnetworks.com", 8080},
            {"US", "speedtest.choopa.net", 8080},
            {"DE", "speedtest.fra1.de.leaseweb.net", 8080},
        };
        static const int kServerCount = (int)(sizeof(kServers) / sizeof(kServers[0]));
        for (int i = 0; i < kServerCount; ++i) {
            if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
            const int ms = tcpConnectMs(QLatin1String(kServers[i].host), kServers[i].port, 3000);
            const QString ccTag = QLatin1String(kServers[i].cc);
            if (ms < 0) {
                serverLines.append(QStringLiteral("  [%1] %2 — unreachable").arg(ccTag, QLatin1String(kServers[i].host)));
            } else {
                rttByCc[ccTag].append(ms);
                serverLines.append(QStringLiteral("  [%1] %2 — %3ms").arg(ccTag, QLatin1String(kServers[i].host)).arg(ms));
            }
        }
    }

    int inCountryMedian = -1, bestForeign = INT_MAX;
    bool vpnSuspected = false, vpnInconclusive = true;
    for (auto it = rttByCc.cbegin(); it != rttByCc.cend(); ++it) {
        QVector<int> v = it.value();
        std::sort(v.begin(), v.end());
        const int med = v[v.size() / 2];
        if (it.key() == cc) inCountryMedian = med;
        else if (med < bestForeign) bestForeign = med;
    }
    if (inCountryMedian > 0 && bestForeign < INT_MAX) {
        vpnInconclusive = false;
        vpnSuspected = inCountryMedian > bestForeign;
    } else if (rttByCc.size() >= 2 && !rttByCc.contains(cc)) {
        // All in-country servers unreachable but foreign servers reachable.
        vpnInconclusive = false;
        vpnSuspected = true;
        inCountryMedian = -1;
    }

    out.append(QString());
    out.append(QStringLiteral("── VPN Heuristic (TCP RTT by region) ──"));
    for (const auto& line : serverLines) out.append(line);
    QString vpnVerdict;
    if (cc == QLatin1String("XX") || cc.isEmpty()) {
        vpnVerdict = QStringLiteral("GeoIP providers unreachable — VPN check skipped");
    } else if (vpnInconclusive) {
        vpnVerdict = QStringLiteral("Inconclusive — not enough region samples");
    } else if (vpnSuspected) {
        vpnVerdict = QStringLiteral("Likely VPN/proxy — in-country RTT %1ms vs best foreign %2ms")
            .arg(inCountryMedian > 0 ? QString::number(inCountryMedian) : QStringLiteral("unreachable"))
            .arg(bestForeign);
    } else {
        vpnVerdict = QStringLiteral("No VPN signal — in-country RTT %1ms ≤ best foreign %2ms")
            .arg(inCountryMedian).arg(bestForeign);
    }
    out.append(QStringLiteral("  %1").arg(vpnVerdict));

    const bool geoSucceeded = !cc.isEmpty() && cc != QLatin1String("XX");
    DiagnosticResult r = makeResult(id, geoSucceeded ? DiagStatus::Pass : DiagStatus::Warning,
        geoSucceeded ? QStringLiteral("Country: %1 (%2)").arg(cc, city.isEmpty() ? isp : city)
                     : QStringLiteral("GeoIP lookup failed"),
        props, out.join(QLatin1Char('\n')));
    r.data[QStringLiteral("countryCode")] = cc;
    r.data[QStringLiteral("city")] = city;
    r.data[QStringLiteral("isp")] = isp;
    r.data[QStringLiteral("as")] = asName;
    r.data[QStringLiteral("publicIp")] = publicIp;
    if (hasCoords) {
        r.data[QStringLiteral("lat")] = lat;
        r.data[QStringLiteral("lon")] = lon;
    }
    r.data[QStringLiteral("vpnSuspected")] = vpnSuspected;
    r.data[QStringLiteral("vpnVerdict")] = vpnVerdict;
    // M5：vpnConfidence 按触发信号强度给出百分比（-1 = 未检查）
    int vpnConfidence = -1;
    if (!(cc == QLatin1String("XX") || cc.isEmpty())) {
        if (vpnInconclusive) vpnConfidence = 50;
        else if (vpnSuspected)
            vpnConfidence = inCountryMedian > 0
                ? qMin(90, 60 + (inCountryMedian - bestForeign)) : 90;
        else
            vpnConfidence = qMax(10, 40 - (bestForeign - inCountryMedian));
    }
    r.data[QStringLiteral("vpnConfidence")] = vpnConfidence;
    // 摘要卡推导叙述：出口 IP → 国家码/ASN → RTT 区域启发式 → VPN 结论
    r.narrative = geoSucceeded
        ? QStringLiteral("Egress IP %1 is geolocated to %2 (%3), ISP %4, AS %5. ")
            .arg(publicIp, cc, city.isEmpty() ? QStringLiteral("city unknown") : city, isp.isEmpty() ? QStringLiteral("unknown") : isp, asName.isEmpty() ? QStringLiteral("unknown") : asName)
          + QStringLiteral("VPN inference compares in-country vs foreign TCP RTT to regional endpoints: %1")
            .arg(vpnVerdict)
          + (vpnConfidence >= 0 ? QStringLiteral(" (confidence %1%).").arg(vpnConfidence) : QStringLiteral("."))
        : QStringLiteral("GeoIP providers were unreachable — country/VPN checks were skipped.");
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G3InternetConnectivity — connectivity check + tiered HTTP speed test
// ═════════════════════════════════════════════════════════════════════════
static int httpStatusSync(const QString& url, int timeoutMs, qint64* ttfbMs = nullptr) {
    const QUrl u(url);
    const int port = u.port(u.scheme() == QLatin1String("https") ? 443 : 80);
    QSslSocket sock;
    QElapsedTimer t; t.start();
    sock.setPeerVerifyMode(QSslSocket::VerifyNone);
    sock.connectToHostEncrypted(u.host(), (quint16)port);
    if (!sock.waitForEncrypted(timeoutMs)) { if (ttfbMs) *ttfbMs = -1; return 0; }
    QByteArray req;
    req += "GET " + u.path(QUrl::FullyEncoded).toUtf8() + " HTTP/1.1\r\n";
    req += "Host: " + u.host().toUtf8() + "\r\n";
    req += "User-Agent: NetDiagnostics/1.0\r\n";
    req += "Connection: close\r\n\r\n";
    if (sock.write(req) < req.size()) { if (ttfbMs) *ttfbMs = -1; return 0; }
    QByteArray all;
    while (t.elapsed() < timeoutMs) {
        if (!sock.waitForReadyRead(qMin<qint64>(300, timeoutMs - t.elapsed()))) break;
        all += sock.readAll();
        if (all.contains("\r\n\r\n")) break;   // headers complete = TTFB
    }
    if (ttfbMs) *ttfbMs = t.elapsed();
    sock.disconnectFromHost();
    const int hdrEnd = all.indexOf("\r\n\r\n");
    if (hdrEnd < 0) return 0;
    const QList<QByteArray> head = all.left(hdrEnd).split('\r');
    if (head.isEmpty()) return 0;
    const QList<QByteArray> statusParts = head.first().split(' ');
    if (statusParts.size() < 2) return 0;
    return statusParts[1].toInt();
}

// Measures transfer throughput for a fixed byte count. Returns Mbps or -1.
static double transferMbps(const QUrl& u, int bytes, bool upload, int timeoutMs) {
    const int port = u.port(u.scheme() == QLatin1String("https") ? 443 : 80);
    QSslSocket sock;
    QElapsedTimer t; t.start();
    sock.setPeerVerifyMode(QSslSocket::VerifyNone);
    sock.connectToHostEncrypted(u.host(), (quint16)port);
    if (!sock.waitForEncrypted(timeoutMs)) return -1;
    QByteArray req;
    if (upload) {
        req += "POST " + u.path(QUrl::FullyEncoded).toUtf8() + " HTTP/1.1\r\n";
        req += "Host: " + u.host().toUtf8() + "\r\n";
        req += "Content-Type: application/octet-stream\r\n";
        req += "Content-Length: " + QByteArray::number(bytes) + "\r\n\r\n";
        req += QByteArray(bytes, 'x');
    } else {
        req += "GET " + u.path(QUrl::FullyEncoded).toUtf8();
        if (u.hasQuery()) { req += '?'; req += u.query(QUrl::FullyEncoded).toUtf8(); }
        req += " HTTP/1.1\r\n";
        req += "Host: " + u.host().toUtf8() + "\r\n";
        req += "Connection: close\r\n\r\n";
    }
    if (sock.write(req) < req.size()) return -1;
    QByteArray all;
    while (t.elapsed() < timeoutMs) {
        if (!sock.waitForReadyRead(qMin<qint64>(300, timeoutMs - t.elapsed()))) break;
        all += sock.readAll();
    }
    sock.disconnectFromHost();
    const qint64 ms = t.elapsed();
    const int hdrEnd = all.indexOf("\r\n\r\n");
    if (hdrEnd < 0 || ms <= 0) return -1;
    const QList<QByteArray> head = all.left(hdrEnd).split('\r');
    if (head.isEmpty()) return -1;
    const QList<QByteArray> statusParts = head.first().split(' ');
    if (statusParts.size() < 2) return -1;
    const int status = statusParts[1].toInt();
    if (status < 200 || status >= 300) return -1;
    const qint64 body = all.size() - hdrEnd - 4;
    const qint64 payload = upload ? bytes : body;
    if (payload <= 0) return -1;
    return (payload * 8.0 / 1e6) / (ms / 1000.0);   // Mbps
}

static DiagnosticResult probeInternetConnectivity(DiagId id, const QString&, RunContext& ctx) {
    QStringList out;
    out.append(QStringLiteral("Internet Connectivity & Speed"));

    // ── Phase 1: connectivity (HTTPS 204 endpoints) ──
    static const char* kEndpoints[] = {
        "https://cp.cloudflare.com/",
        "https://www.gstatic.com/generate_204",
        "https://www.google.com/generate_204",
    };
    int reachable = 0, bestTtfb = INT_MAX;
    for (const char* ep : kEndpoints) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        qint64 ttfb = -1;
        const int status = httpStatusSync(QLatin1String(ep), 8000, &ttfb);
        if (status >= 200 && status < 400) {
            ++reachable;
            if (ttfb >= 0 && ttfb < bestTtfb) bestTtfb = (int)ttfb;
            out.append(QStringLiteral("  ✓ %1 — HTTP %2 (%3ms)").arg(QLatin1String(ep)).arg(status).arg(ttfb));
        } else {
            out.append(QStringLiteral("  ✗ %1 — unreachable").arg(QLatin1String(ep)));
        }
    }
    out.append(QStringLiteral("  Connectivity: %1/3 endpoints reachable").arg(reachable));
    if (reachable == 0) {
        DiagnosticResult r = makeResult(id, DiagStatus::Fail,
            QStringLiteral("No Internet Connectivity"), {}, out.join(QLatin1Char('\n')));
        r.data[QStringLiteral("connected")] = false;
        r.data[QStringLiteral("latencyMs")] = -1;
        r.data[QStringLiteral("downloadMbps")] = 0.0;
        r.data[QStringLiteral("uploadMbps")] = 0.0;
        r.data[QStringLiteral("downloadMbpsBest")] = 0.0;
        r.data[QStringLiteral("uploadMbpsBest")] = 0.0;
        r.narrative = QStringLiteral("Internet connectivity: 0/3 endpoints reachable — the device has no Internet access. "
            "Speed test was not run.");
        return r;
    }

    // ── Phase 2: latency (TCP connect to edge) ──
    const int tcpMs = tcpConnectMs(QStringLiteral("cp.cloudflare.com"), 443, 5000);
    if (tcpMs >= 0) out.append(QStringLiteral("  Edge RTT (TCP connect): %1ms").arg(tcpMs));

    // ── Phase 3: tiered speed test (Cloudflare edge) ──
    static const int kTiers[] = { 64000, 256000, 1000000 };
    static const char* kTierLabels[] = { "64 KB", "256 KB", "1 MB" };
    double bestDl = 0, bestUl = 0;
    for (int i = 0; i < 3; ++i) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        const double dl = transferMbps(QUrl(QStringLiteral("https://speed.cloudflare.com/__down?bytes=%1").arg(kTiers[i])),
                                       kTiers[i], false, 12000);
        if (dl > 0) {
            bestDl = qMax(bestDl, dl);
            out.append(QStringLiteral("  Download %1: %2 Mbps").arg(QLatin1String(kTierLabels[i])).arg(dl, 0, 'f', 1));
        } else {
            out.append(QStringLiteral("  Download %1: failed").arg(QLatin1String(kTierLabels[i])));
        }
        const double ul = transferMbps(QUrl(QStringLiteral("https://speed.cloudflare.com/__up")),
                                       kTiers[i], true, 12000);
        if (ul > 0) {
            bestUl = qMax(bestUl, ul);
            out.append(QStringLiteral("  Upload   %1: %2 Mbps").arg(QLatin1String(kTierLabels[i])).arg(ul, 0, 'f', 1));
        } else {
            out.append(QStringLiteral("  Upload   %1: failed").arg(QLatin1String(kTierLabels[i])));
        }
    }

    DiagStatus status = DiagStatus::Pass;
    if (bestDl <= 0 && bestUl <= 0) status = DiagStatus::Warning;
    else if (bestDl > 0 && bestDl < 1.0) status = DiagStatus::Warning;
    QString summary;
    if (bestDl > 0) summary = QStringLiteral("↓ %1 Mbps, ↑ %2 Mbps").arg(bestDl, 0, 'f', 1).arg(bestUl, 0, 'f', 1);
    else if (bestUl > 0) summary = QStringLiteral("Upload only: %1 Mbps").arg(bestUl, 0, 'f', 1);
    else summary = QStringLiteral("Speed test failed (connected)");

    DiagnosticResult r = makeResult(id, status, summary, {}, out.join(QLatin1Char('\n')));
    r.data[QStringLiteral("connected")] = true;
    r.data[QStringLiteral("reachableEndpoints")] = reachable;
    r.data[QStringLiteral("bestTtfbMs")] = bestTtfb < INT_MAX ? bestTtfb : -1;
    r.data[QStringLiteral("edgeRttMs")] = tcpMs;
    // M4：diag-g3 §2.5 契约键——latencyMs（TTFB）/ downloadMbps / uploadMbps
    r.data[QStringLiteral("latencyMs")] = bestTtfb < INT_MAX ? bestTtfb : (tcpMs >= 0 ? tcpMs : -1);
    r.data[QStringLiteral("downloadMbps")] = bestDl;
    r.data[QStringLiteral("uploadMbps")] = bestUl;
    r.data[QStringLiteral("downloadMbpsBest")] = bestDl;
    r.data[QStringLiteral("uploadMbpsBest")] = bestUl;
    // 摘要卡叙述：可达性结论 + TTFB/边缘 RTT + 测速结果
    r.narrative = QStringLiteral("Internet connectivity: %1/3 endpoints reachable. ")
        .arg(reachable)
        + (bestTtfb < INT_MAX
            ? QStringLiteral("Best endpoint TTFB %1ms. ").arg(bestTtfb)
            : (tcpMs >= 0 ? QStringLiteral("Edge RTT (TCP connect) %1ms. ").arg(tcpMs) : QStringLiteral("")))
        + (bestDl > 0
            ? QStringLiteral("Speed test: download %1 Mbps, upload %2 Mbps (best of three tiers).")
                .arg(bestDl, 0, 'f', 1).arg(bestUl, 0, 'f', 1)
            : bestUl > 0
                ? QStringLiteral("Speed test: download failed, upload %1 Mbps.").arg(bestUl, 0, 'f', 1)
                : QStringLiteral("Speed test failed despite reachable endpoints."));
    return r;
}

// ── G3NetskopeStatus（5WHY 复核 2026-08-19 v0.0.3 对等恢复）─────────────
// 移植自 v0.0.3 G3NetskopeStatus.cpp：扫描运行进程（Windows 快照 / /proc
// comm）中的安全代理客户端（nsproxy/zscaler/netskope/zsproxy），
// 检出=Pass、未检出=Info，终端转储呈现。
static DiagnosticResult probeNetskopeStatus(DiagId id, const QString&, RunContext& ctx) {
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("Security Proxy Status:"));
    out.append(QString());
    bool found = false;
#if defined(_WIN32)
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(hSnap, &pe)) {
            do {
                if (ctx.cancelled.load()) break;
                const QString name = QString::fromWCharArray(pe.szExeFile);
                if (name.contains(QStringLiteral("nsproxy"), Qt::CaseInsensitive)
                    || name.contains(QStringLiteral("zsproxy"), Qt::CaseInsensitive)
                    || name.contains(QStringLiteral("zscaler"), Qt::CaseInsensitive)
                    || name.contains(QStringLiteral("netskope"), Qt::CaseInsensitive)) {
                    out.append(QStringLiteral("  Found: %1 (PID %2)").arg(name).arg(pe.th32ProcessID));
                    found = true;
                }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }
#else
#if defined(__APPLE__) && !defined(PLATFORM_IOS)
    // 5WHY (复核 2026-08-20 macOS 假阴性): PF_Desktop 含 macOS——原 /proc
    // 分支在 macOS 上恒为空表、"No security proxy detected" 恒假阴性。
    // macOS 无 /proc：ps 单次快照（与 ActiveConnections 的 macOS 分支
    // 同模式）。
    // 5WHY (2026-08-20 CI): 必须排除 PLATFORM_IOS——iOS 也定义 __APPLE__，
    // 而 QtCore 在 iOS 上定义 QT_NO_PROCESS（无进程模型，QProcess 类未声明），
    // 裸 __APPLE__ 会让该分支在 iOS 编译并报 "unknown type name 'QProcess'"。
    // iOS 走下方 /proc 分支（编译安全，空结果 → Info）。
    QProcess ps;
    ps.start(QStringLiteral("ps"), QStringList() << QStringLiteral("-e") << QStringLiteral("-o") << QStringLiteral("comm="));
    if (ps.waitForFinished(3000)) {
        for (const auto& line : QString::fromLocal8Bit(ps.readAllStandardOutput()).split(QLatin1Char('\n'))) {
            if (ctx.cancelled.load()) break;
            const QString comm = line.trimmed();
            if (comm.isEmpty()) continue;
            if (comm.contains(QStringLiteral("nsproxy"), Qt::CaseInsensitive)
                || comm.contains(QStringLiteral("zsproxy"), Qt::CaseInsensitive)
                || comm.contains(QStringLiteral("zscaler"), Qt::CaseInsensitive)
                || comm.contains(QStringLiteral("netskope"), Qt::CaseInsensitive)) {
                out.append(QStringLiteral("  Found: %1").arg(comm));
                found = true;
            }
        }
    } else {
        ps.kill();
        ps.waitForFinished(2000);   // R5-1
    }
#else
    const QDir procDir(QStringLiteral("/proc"));
    const auto entries = procDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto& fi : entries) {
        if (ctx.cancelled.load()) break;
        bool ok = false;
        fi.fileName().toInt(&ok);
        if (!ok) continue;   // 非 PID 目录
        QFile cmdLine(fi.absoluteFilePath() + QStringLiteral("/comm"));
        if (cmdLine.open(QIODevice::ReadOnly)) {
            const QString comm = QString::fromLatin1(cmdLine.readAll().trimmed());
            if (comm.contains(QStringLiteral("nsproxy"), Qt::CaseInsensitive)
                || comm.contains(QStringLiteral("zsproxy"), Qt::CaseInsensitive)
                || comm.contains(QStringLiteral("zscaler"), Qt::CaseInsensitive)
                || comm.contains(QStringLiteral("netskope"), Qt::CaseInsensitive)) {
                out.append(QStringLiteral("  Found: %1 (PID %2)").arg(comm, fi.fileName()));
                found = true;
            }
        }
    }
#endif
#endif   // closes #if defined(__APPLE__)
    // 5WHY (复核 2026-08-20 取消语义): 循环内 break 早停后曾直接落
    // makeResult——取消的扫描计为 Pass/Info 假阴性/假阳性（与
    // ActiveConnections 的循环后复查同源缺陷）。统一复查返回 Cancelled。
    if (ctx.cancelled.load())
        return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
    if (!found) out.append(QStringLiteral("  No security proxy process detected"));
    DiagnosticResult r = makeResult(id,
        found ? DiagStatus::Pass : DiagStatus::Info,
        found ? QStringLiteral("Security proxy detected")
              : QStringLiteral("No security proxy detected"),
        {}, out.join(QLatin1Char('\n')));
    r.data[QStringLiteral("detected")] = found;
    return r;
}

} // namespace g3

// ── Registration (NEW-1 platform masks) ───────────────────────────────────
void registerG3Adapters() {
    using namespace PlatformFlag;
#if defined(PLATFORM_ANDROID)
    // Android：DNS 服务器/缓存经 JNI（NetworkDiagnostics.cpp）
    AdapterRegistry::registerAdapters(DiagId::G3DnsServers, {
        {PF_Android, "Android", {}, [](DiagId i, const QString&, RunContext&) { return androidDnsServersDiag(i); }},
    });
    AdapterRegistry::registerAdapters(DiagId::G3DnsCache, {
        {PF_Android, "Android", {}, [](DiagId i, const QString&, RunContext&) { return androidDnsCacheDiag(i); }},
    });
#endif
    AdapterRegistry::registerAdapters(DiagId::G3DnsServers, {
        {PF_Desktop, "Desktop", {}, g3::probeDnsServers},
#if !defined(PLATFORM_ANDROID)
        {PF_IOS,     "iOS",     {}, g3::probeDnsServers},
#endif
    });
    AdapterRegistry::registerAdapters(DiagId::G3DnsCache, {
        {PF_Desktop, "Desktop", {}, g3::probeDnsCache},
    });
    AdapterRegistry::registerAdapters(DiagId::G3DnsIntegrity, {
        {PF_Desktop, "Desktop", {}, g3::probeDnsIntegrity},
        {PF_IOS,     "iOS",     {}, g3::probeDnsIntegrity},
        {PF_Android, "Android", {}, g3::probeDnsIntegrity},
    });
    AdapterRegistry::registerAdapters(DiagId::G3GeoIPLoc, {
        {PF_Desktop, "Desktop", {}, g3::probeGeoIPLoc},
        {PF_IOS,     "iOS",     {}, g3::probeGeoIPLoc},
        {PF_Android, "Android", {}, g3::probeGeoIPLoc},
    });
    AdapterRegistry::registerAdapters(DiagId::G3InternetConnectivity, {
        {PF_Desktop, "Desktop", {}, g3::probeInternetConnectivity},
        {PF_IOS,     "iOS",     {}, g3::probeInternetConnectivity},
        {PF_Android, "Android", {}, g3::probeInternetConnectivity},
    });
    // 5WHY (复核 2026-08-20 Android 恒假阴性): Android 的 SELinux/hidepid
    // 屏蔽他进程 /proc/<pid>/comm——探测只能读到自己，真实客户端在跑也
    // 恒报"未检出"。撤销 Android 注册（不运行比恒错结果诚实）；桌面三
    // 平台（Windows 快照/macOS ps/Linux /proc）保留。
    AdapterRegistry::registerAdapters(DiagId::G3NetskopeStatus, {
        {PF_Desktop, "Desktop", {}, g3::probeNetskopeStatus},
    });
}
