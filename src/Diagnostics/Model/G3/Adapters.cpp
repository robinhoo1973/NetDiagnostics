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
#include "Common/Services/DnsResolver.h"   // v0.0.3 复刻 InternetConnectivity 最佳服务器 IP 解析
#include "Diagnostics/Model/GHelpers.h"   // readProcLines / cachedRunTool

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
    // 5WHY (2026-09-26 单一工厂): 委托 DiagnosticResult::makeResult——五份副本
    // 曾漂移（G1/G2/G3 不回填 errorOutput -> 失败结果错误区空白）。统一契约。
    return DiagnosticResult::makeResult(id, status, summary, props, details);
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
// 5WHY (2026-09-26 死共享栈收敛): GCommon.cpp 的共享 dohQueryFull（4 解析器
// 并行 + 2000ms）曾是零调用者死代码——5WHY 超时修正只落在死副本，活代码仍
// 4000ms×3 串行 = 最坏 12s/域。死栈已删；此处按同法收敛：三解析器并行
// （最坏 max(2000ms)），按优先级序取首个有 A 记录者——保持 Cloudflare →
// Google → AliDNS 偏好语义。
static dnsWire::Answer parseDohJsonBody(const QByteArray& body) {
    dnsWire::Answer a;
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) return a;
    const QJsonArray answers = doc.object().value(QStringLiteral("Answer")).toArray();
    if (answers.isEmpty()) return a;
    for (const auto& v : answers) {
        if (!v.isObject()) continue;
        const QJsonObject o = v.toObject();
        const int type = o.value(QStringLiteral("type")).toInt();
        const int ttl  = o.value(QStringLiteral("TTL")).toInt();
        if (type == 1) {
            a.aRecords.append(o.value(QStringLiteral("data")).toString());
            if (a.minTtl < 0 || ttl < a.minTtl) a.minTtl = ttl;
        } else if (type == 5) {
            a.cnameChain.append(o.value(QStringLiteral("data")).toString());
            a.hasCname = true;
        }
    }
    return a;
}

static dnsWire::Answer dohQueryFull(const QString& domain, int timeoutMs = 2000) {
    static const char* kUrls[] = {
        "https://1.1.1.1/dns-query?name=%1&type=A",
        "https://8.8.8.8/resolve?name=%1&type=A",
        "https://223.5.5.5/resolve?name=%1&type=A",   // AliDNS DoH (CN reachable)
    };
    constexpr int kCount = (int)(sizeof(kUrls) / sizeof(kUrls[0]));
    struct Partial { dnsWire::Answer ans; int elapsedMs = 0; };
    Partial results[kCount];
    std::vector<std::thread> threads;
    threads.reserve(kCount);
    for (int i = 0; i < kCount; ++i) {
        try {
            threads.emplace_back([i, &results, domain, timeoutMs]() {
                const QString url = QString::fromLatin1(kUrls[i]).arg(domain);
                QElapsedTimer t; t.start();
                const QByteArray body = httpsGetSync(url, timeoutMs,
                    QByteArrayLiteral("application/dns-json"));
                Partial pr;
                pr.elapsedMs = (int)t.elapsed();
                pr.ans = parseDohJsonBody(body);
                results[i] = pr;
            });
        } catch (const std::system_error&) { break; }
    }
    for (auto& th : threads) {
        try { if (th.joinable()) th.join(); } catch (...) {}
    }
    // 优先级序取首个有 A 记录者（与旧串行语义一致：先云后谷歌再阿里）
    for (int i = 0; i < kCount; ++i) {
        if (!results[i].ans.aRecords.isEmpty()) {
            results[i].ans.elapsedMs = results[i].elapsedMs;
            return results[i].ans;
        }
    }
    dnsWire::Answer empty;
    empty.elapsedMs = results[0].elapsedMs;   // 全败仍报首解析器耗时
    return empty;   // empty aRecords = query failed
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
    // 5WHY (复核 2026-08-21 procfs 收敛): 手写 QTextStream 循环 →
    // 共享 readProcLines（与 G1/G2/G5 同源）。
    for (const QString& raw : SystemDiagnostics::readProcLines(QStringLiteral("/etc/resolv.conf"))) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        const QString line = raw.trimmed();
        if (line.startsWith(QLatin1String("nameserver ")))
            appendServer(QStringLiteral("resolv.conf"), line.mid(11));
        // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): v0.0.3 表含
        // "search domains" 行（整行并入单行）——首版缺失。
        else if (line.startsWith(QLatin1String("search ")))
            props.append({QStringLiteral("search domains"), line.mid(7)});
    }
    // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): v0.0.3 桩行只入表不入
    // dnsList（摘要只列真实 IP）——曾经 appendServer 把 "(stub resolver
    // active)" 混进 summary "DNS: 127.0.0.53, (stub resolver active)"。
    if (QFile::exists(QStringLiteral("/run/systemd/resolve/resolv.conf")))
        props.append({QStringLiteral("systemd-resolved"), QStringLiteral("(stub resolver active)")});
#endif
#endif

    // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): details 曾 "DNS servers: …"
    // 平串——v0.0.3 为 "\nDNS Server Configuration (table mode):\n\n" +
    // 列对齐表 [Source 20/DNS Server 0]。
    QStringList out;
    out.append(QString());
    out.append(QStringLiteral("DNS Server Configuration (table mode):"));
    out.append(QString());
    if (!props.isEmpty()) {
        static const QVector<DiagnosticFormatter::ColSpec> kDnsCols = {
            {"Source",   20, false},
            {"DNS Server", 0, false},
        };
        QList<QStringList> dnsRows;
        for (const auto& p : props)
            dnsRows.append({ p.label, p.value });
        out.append(DiagnosticFormatter::formatTable(kDnsCols, dnsRows));
    }
    // 5WHY (复核 2026-08-21 v0.0.3 逐字复刻): v0.0.3 空态只出头块
    // （头 + 空行），无 "  No DNS servers found" 行——首版发明该行。
    DiagnosticResult r = makeResult(id, dnsList.isEmpty() ? DiagStatus::Warning : DiagStatus::Pass,
        dnsList.isEmpty() ? QStringLiteral("No DNS servers found")
                          : QStringLiteral("DNS: %1").arg(dnsList.join(QStringLiteral(", "))),
        props, out.join(QLatin1Char('\n')));
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
        for (const QString& raw : SystemDiagnostics::readProcLines(QStringLiteral("/etc/resolv.conf"))) {
            if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
            const QString line = raw.trimmed();
            if (!line.isEmpty() && !line.startsWith(QLatin1Char('#')))
                out.append(QStringLiteral("    %1").arg(line));
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
    // 5WHY (复核 2026-08-20 Apple 保留词): 枚举成员 Clean 触发
    // pre-commit 第 10 项 WARN（Apple SDK 保留词模式）——改 Intact，
    // 避免 Apple 平台宏冲突隐患，展示标签字符串 "Clean" 保持不变。
    enum class Verdict { Intact, Suspect, Tampered, Hijacked };
    Verdict verdict = Verdict::Intact;
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
    else                          r.verdict = IntegrityResult::Verdict::Intact;

    const char* label = r.verdict == IntegrityResult::Verdict::Intact   ? "Clean"
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
                            nullptr, ip, &ipLen) == 0) {
                        // 5WHY (2026-09-27 端口后缀): GetAdaptersAddresses 为 DNS
                        // 服务器地址填充 sin_port=53，WSAAddressToStringA 因而输出
                        // "a.b.c.d:53"/"[v6]:53"——QHostAddress 无法解析端口后缀，
                        // 曾致 testServer 为 Null，Windows 上全部 UDP 探针静默失败
                        // 并误报 INCONCLUSIVE。剥除端口与 IPv6 方括号后再使用。
                        QString ipStr = QString::fromLatin1(ip);
                        if (ipStr.endsWith(QLatin1String(":53"))) ipStr.chop(3);
                        if (ipStr.startsWith(QLatin1Char('[')) && ipStr.endsWith(QLatin1Char(']')))
                            ipStr = ipStr.mid(1, ipStr.size() - 2);
                        testServer = ipStr;
                    }
                }
            }
        }
#else
        for (const QString& raw : SystemDiagnostics::readProcLines(QStringLiteral("/etc/resolv.conf"))) {
            const QString line = raw.trimmed();
            if (line.startsWith(QLatin1String("nameserver ")) && testServer.isEmpty())
                testServer = line.mid(11);
        }
#endif
    }
    out.append(QStringLiteral("Resolver under test: %1")
        .arg(testServer.isEmpty() ? QStringLiteral("(none found)") : testServer));

    // 5WHY (2026-08-23 报告样本一致性 D3, review/ui-ux-audit-plan §5.2): 无系统
    // 解析器时曾继续跑两阶段——Phase 1 全 Not Resolved、Phase 2 全 Skipped，
    // 产出 "(none found)" 的 Info 噪声卡（真实样本实证）。测量能力缺失 ≠ 网络
    // 结论：诚实早退并说明平台限制，不伪造阶段结果。
    if (testServer.isEmpty()) {
        out.append(QStringLiteral("No system DNS resolver configuration was exposed by this platform — "
            "hijack/pollution phases cannot run against a local resolver."));
        DiagnosticResult r = makeResult(id, DiagStatus::Info,
            QStringLiteral("Not measurable — no system resolver"), {}, out.join(QLatin1Char('\n')));
        r.narrative = QStringLiteral("This platform does not expose a system DNS resolver configuration "
            "(e.g. iOS sandboxes resolv.conf), so the hijack and pollution phases cannot be tested "
            "against the local resolver. No integrity verdict is produced rather than an unreliable one; "
            "encrypted DoH lookups remain covered by other tests.");
        r.data[QStringLiteral("narrativeKey")] = QStringLiteral("nDnsIntegrityNoResolver");
        r.data[QStringLiteral("narrativeArgs")] = QVariantList{};
        return r;
    }

    // ══ Phase 1: ISP DNS Hijacking (NXDOMAIN hijack) ══
    int hijackClean = 0, hijackWarn = 0, hijackTimeout = 0, hijackErrors = 0;
    QStringList hijackIPs;
    out.append(QString());
    out.append(QStringLiteral("── Phase 1: ISP DNS Hijacking ──"));
    const QString datePrefix = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd"));
    for (int i = 0; i < 3; ++i) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        // 5WHY (2026-09-27 探测名可预测): 曾按日期+i 确定性生成且域名含
        // "dns-test" 字面标记——劫持响应器/审查中间盒可预计算当日 3 个探针名
        // 白名单放行（假 DNS CLEAN Pass），且全球同日同 3 名使探测流量易指纹
        // 化。混入每轮随机数：名不可预测、无字面标记，NXDOMAIN 预期不变。
        const QByteArray seed = QStringLiteral("%1-%2-dns-hijack-test-%3")
            .arg(datePrefix).arg(i)
            .arg(QRandomGenerator::global()->generate64()).toUtf8();
        const QString hex = QString::fromLatin1(
            QCryptographicHash::hash(seed, QCryptographicHash::Md5).toHex().left(10));
        static const char* tlds[] = {"com", "org", "net"};
        const QString domain = QStringLiteral("%1-%2.%3").arg(datePrefix, hex, tlds[i]);
        const dnsWire::Answer ans = dnsWire::udpQuery(domain, 1, testServer, 2000);
        if (!ans.aRecords.isEmpty()) {
            out.append(QStringLiteral("  %1 → HIJACKED: %2 (%3ms)")
                .arg(domain, ans.aRecords.first()).arg(ans.elapsedMs));
            ++hijackWarn;
            if (!hijackIPs.contains(ans.aRecords.first())) hijackIPs.append(ans.aRecords.first());
        } else if (ans.rcode < 0) {
            // 5WHY (2026-09-27 超时分类): 曾以 elapsedMs >= 3000 判 TIMEOUT——
            // udpQuery 超时 2000ms，超时返回 ~2000ms 恒落 clean 分支（超时
            // 计"Not Resolved"假阴性；全 DNS 故障时 3×clean 绕过 INCONCLUSIVE
            // 报"DNS CLEAN" Pass）。rcode<0 = 未收到任何应答（超时/连接失败，
            // parseResponse 未运行），与 NXDOMAIN（rcode=3）语义分离。
            out.append(QStringLiteral("  %1 → TIMEOUT (%2ms)").arg(domain).arg(ans.elapsedMs));
            ++hijackTimeout;
        } else if (ans.rcode == 3 || ans.rcode == 0) {
            // NXDOMAIN（随机域权威否定=健康预期）与 NOERROR 空答均计入 clean。
            out.append(QStringLiteral("  %1 → Not Resolved").arg(domain));
            ++hijackClean;
        } else {
            // 5WHY (2026-09-27 错误桶): SERVFAIL(2)/REFUSED(5)/FORMERR(1) 等
            // 解析器错误态曾混入 clean——解析器报错不是"无劫持证据"，可贡献
            // 虚假的 DNS CLEAN Pass。独立错误桶并入无结论裁决。
            out.append(QStringLiteral("  %1 → ERROR (RCODE %2)").arg(domain).arg(ans.rcode));
            ++hijackErrors;
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
        IntegrityResult::Verdict verdict = IntegrityResult::Verdict::Intact;
        int scorePercent = 0;
        QString dohIps, localUdpIp, domain;
        QStringList lines;
    };
    DomainResult domResults[kDomCount];
    std::vector<std::thread> threads;
    threads.reserve(kDomCount);
    // 5WHY (2026-09-26 取消缺口): Phase2 线程曾不捕取消——取消后仍串行付满
    // DoH 4s + UDP 3s + TLS 3s ≈ 18s 才 join 返回。线程内逐相位响应取消：
    // 取消后立即短路（其余相位跳过），join 几乎即时。
    const std::atomic<bool>* cancelled = &ctx.cancelled;
    for (int i = 0; i < kDomCount; ++i) {
        try {
            threads.emplace_back([i, &domResults, testServer, cancelled]() {
                DomainResult dr;
                dr.domain = QString::fromUtf8(kTestDomains[i].domain);
                const QString desc = QString::fromUtf8(kTestDomains[i].description);
                if (cancelled->load(std::memory_order_relaxed)) { domResults[i] = dr; return; }
                const dnsWire::Answer doh = dohQueryFull(dr.domain);
                // ── local resolution: raw UDP probe to the resolver (real timing) ──
                dnsWire::Answer localAns;
                if (!testServer.isEmpty() && !cancelled->load(std::memory_order_relaxed))
                    localAns = dnsWire::udpQuery(dr.domain, 1, testServer, 3000);
                const int localMs = localAns.elapsedMs;
                dr.localUdpIp = localAns.aRecords.value(0);
                const bool resolved = !dr.localUdpIp.isEmpty();

                if (cancelled->load(std::memory_order_relaxed)) {
                    dr.lines.append(QStringLiteral("  %1 (%2) — Cancelled").arg(dr.domain, desc));
                    dr.tag = Tag::Error;
                } else if (doh.aRecords.isEmpty()) {
                    dr.lines.append(QStringLiteral("  %1 (%2) — DoH Query Failed, Skipped").arg(dr.domain, desc));
                    dr.tag = Tag::Error;
                } else if (!resolved) {
                    dr.lines.append(QStringLiteral("  %1 (%2) — Local DNS Failed, Skipped").arg(dr.domain, desc));
                    dr.tag = Tag::Error;
                } else if (isPrivateIp(dr.localUdpIp)) {
                    // 与裁决措辞同源：私有 IP 归入 pollutionWarn（裁决报
                    // "DNS POLLUTION"），逐域行不再自称 HIJACKED 以免同屏矛盾。
                    dr.lines.append(QStringLiteral("  %1 (%2) — Local=%3 → DIVERTED (Private IP)")
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
    // 5WHY (2026-09-27 线程创建失败计数): 曾按 kDomCount 全量计数——emplace_back
    // 抛 system_error 中断后，未启动下标仍是默认 Tag::Error，被计为真实查询
    // 失败，可伪造 AllFailed/MIXED 裁决。只计已启动线程数。
    for (int i = 0; i < (int)threads.size(); ++i) {
        const DomainResult& dr = domResults[i];
        for (const auto& line : dr.lines) out.append(line);
        switch (dr.tag) {
        case Tag::Error:          ++pollutionErrors; break;
        case Tag::PrivateIp:      ++pollutionWarn; break;
        case Tag::Scored:
            switch (dr.verdict) {
            case IntegrityResult::Verdict::Intact:   ++pollutionClean; break;
            case IntegrityResult::Verdict::Suspect:  ++pollutionSuspicious; break;
            case IntegrityResult::Verdict::Tampered: ++pollutionWarn; break;
            case IntegrityResult::Verdict::Hijacked: ++pollutionWarn; break;
            }
            break;
        }
    }

    // ══ Combined verdict ══
    // 5WHY (2026-09-27 v5 计数单源): 曾六处各自求和（叙述/参数/终端/属性行）
    // 且两轮修复仍漏终端与属性行——同屏矛盾反复。单一派生，四处消费。
    const int hijackFailed = hijackTimeout + hijackErrors;
    const int hijackTotal = hijackClean + hijackWarn + hijackFailed;
    const int pollutionTotal = pollutionClean + pollutionWarn + pollutionSuspicious + pollutionErrors;
    out.append(QString());
    out.append(QStringLiteral("Phase 1 (ISP Hijack):  %1 clean, %2 hijacked, %3 timeout/error")
        .arg(hijackClean).arg(hijackWarn).arg(hijackFailed));
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
    // 5WHY (2026-09-27 v6 裁决枚举化): 四条平行分类链（分支链/overall 三元/
    // p2verdict 三元/p1.value 三元）曾各自重推同一裁决且已漂移多轮——MIXED
    // 概念三处谓词互不一致。收敛为 DnsVerdict 枚举：分支链只求裁决与文案，
    // 评分/属性值/叙述键全部 switch 派生（与 MtuVerdict/statusDescriptor 同门）。
    enum class DnsVerdict { HijackPollution, Hijack, Pollution, Mixed, AllFailed,
                            Phase2Inconclusive, Suspicious, Intact };
    DnsVerdict verdict = DnsVerdict::Intact;
    if (hijackDetected && pollutionDetected) {
        out.append(QStringLiteral("Verdict: DNS HIJACKING + POLLUTION detected"));
        out.append(QStringLiteral("  Hijack IPs: %1").arg(hijackIPs.join(QStringLiteral(", "))));
        status = DiagStatus::Warning; summary = QStringLiteral("DNS: hijack + pollution");
        verdict = DnsVerdict::HijackPollution;
    } else if (hijackDetected) {
        out.append(QStringLiteral("Verdict: ISP DNS HIJACKING detected"));
        out.append(QStringLiteral("  Hijack IPs: %1").arg(hijackIPs.join(QStringLiteral(", "))));
        status = DiagStatus::Warning; summary = QStringLiteral("DNS hijack: %1 IP(s)").arg(hijackIPs.size());
        verdict = DnsVerdict::Hijack;
    } else if (pollutionDetected) {
        out.append(QStringLiteral("Verdict: DNS POLLUTION — %1/%2 domains affected")
            .arg(pollutionWarn).arg(pollutionTotal));
        status = DiagStatus::Warning;
        summary = QStringLiteral("DNS polluted: %1/%2 domains").arg(pollutionWarn).arg(pollutionTotal);
        verdict = DnsVerdict::Pollution;
    } else {
        const int totalErrors = hijackFailed + pollutionErrors;
        // 失败证据分支必须先行（曾 SUSPICIOUS/phase2AllFailed 先于 MIXED）。
        // 5WHY (2026-09-27 Phase2Inconclusive 死支): phase2AllFailed 蕴含
        // pollutionErrors>0 ⇒ totalErrors>0，曾排在通用 totalErrors 分支之后
        // 永不可达——Phase 1 全净 + Phase 2 全败被误报 MIXED。须先于通用
        // 分支，并加 hijackFailed==0 门（Phase 1 含超时/错误时 MIXED 更准）。
        if (totalErrors > 0 && hijackClean + pollutionClean + pollutionSuspicious == 0) {
            out.append(QStringLiteral("Verdict: INCONCLUSIVE — all queries failed"));
            status = DiagStatus::Info; summary = QStringLiteral("DNS: all queries failed");
            verdict = DnsVerdict::AllFailed;
        } else if (phase2AllFailed && hijackFailed == 0) {
            out.append(QStringLiteral("Verdict: Phase 1 clean, Phase 2 inconclusive (all DoH queries failed)"));
            status = DiagStatus::Info;
            summary = QStringLiteral("DNS: hijack clean, pollution inconclusive");
            verdict = DnsVerdict::Phase2Inconclusive;
        } else if (totalErrors > 0) {
            QStringList issueKinds;
            if (hijackTimeout > 0) issueKinds << QStringLiteral("%1 timeout(s)").arg(hijackTimeout);
            if (hijackErrors + pollutionErrors > 0)
                issueKinds << QStringLiteral("%1 error(s)").arg(hijackErrors + pollutionErrors);
            const QString mixedKinds = issueKinds.join(QStringLiteral(", "));
            out.append(QStringLiteral("Verdict: MIXED — %1 alongside clean results; integrity undetermined")
                .arg(mixedKinds));
            status = DiagStatus::Info;
            summary = QStringLiteral("DNS: mixed results (%1)").arg(mixedKinds);
            verdict = DnsVerdict::Mixed;
        } else if (pollutionSuspicious > 0) {
            out.append(QStringLiteral("Verdict: SUSPICIOUS — %1 domain(s) need manual check").arg(pollutionSuspicious));
            status = DiagStatus::Info; summary = QStringLiteral("DNS: %1 suspicious").arg(pollutionSuspicious);
            verdict = DnsVerdict::Suspicious;
        } else {
            out.append(QStringLiteral("Verdict: DNS CLEAN — no hijacking or pollution detected"));
            status = DiagStatus::Pass; summary = QStringLiteral("DNS clean");
            verdict = DnsVerdict::Intact;
        }
    }
    // 无分裁决集（单一来源——曾布尔就地置位与平行条件链并存）
    const bool inconclusiveVerdict =
        verdict == DnsVerdict::Mixed || verdict == DnsVerdict::AllFailed
        || verdict == DnsVerdict::Phase2Inconclusive;

    // overallScorePercent 驱动仪表盘：由最终裁决推导（含 Phase 1 劫持），
    // 不再用与裁决脱节的任意公式——仪表与结论必须一致。
    // 5WHY (2026-09-27 v2 无值契约): inconclusive 曾映射 80 分——「没测出来」
    // 妆成「80 分健康」，且 scoreInconclusive 布尔 + QML -1 哨兵逐面打补丁
    // 仍漏指标卡/属性行两个消费面。改为「无结论即无分」：键不下发，三个
    // 消费面经键不存在统一呈现缺失，无需任何特判。

    const int overall = verdict == DnsVerdict::HijackPollution ? 0
        : verdict == DnsVerdict::Hijack     ? 20
        : verdict == DnsVerdict::Pollution  ? 40
        : verdict == DnsVerdict::Suspicious ? 60
        : 100;   // 无分裁决不消费此值（5WHY v6）
    // 5WHY (2026-09-27 v6 链同源): p2verdict 由裁决枚举 switch 派生——曾
    // 平行三元链且 mixed 谓词是状态机子集（hijack 超时+phase2 全净时属性行
    // 仍显示 "Clean" 与裁决矛盾）。
    const QString p2verdict = verdict == DnsVerdict::HijackPollution ? QStringLiteral("hijack + pollution")
        : verdict == DnsVerdict::Hijack    ? QStringLiteral("hijack")
        : verdict == DnsVerdict::Pollution ? QStringLiteral("pollution")
        : verdict == DnsVerdict::Mixed     ? QStringLiteral("mixed")
        : verdict == DnsVerdict::Suspicious ? QStringLiteral("suspicious")
        : verdict == DnsVerdict::Phase2Inconclusive || verdict == DnsVerdict::AllFailed
                                           ? QStringLiteral("inconclusive")
                                           : QStringLiteral("clean");

    // 5WHY (2026-08-23 详情页信息前置): 两阶段计数与裁决曾只在 terminal/
    // narrative 散文里——提炼为分组属性行，terminal 折叠后仍可直读各阶段
    // 结果与总分。
    QVector<ResultProperty> dprops;
    {
        ResultProperty p1;
        p1.label = QStringLiteral("Phase 1 · Hijack Probe");
        // 5WHY (2026-09-27 v6): 由裁决枚举派生（曾平行三元）。
        // 5WHY (2026-09-27 属性行自相矛盾): 曾 AllFailed 也落 "Clean"（该裁决
        // 要求 hijackClean==0，行头却报 Clean），且 MIXED 全由 Phase 2 错误
        // 驱动时 Phase 1 行头误报 "Mixed"。按 Phase 1 实际证据派生。
        p1.value = verdict == DnsVerdict::Hijack || verdict == DnsVerdict::HijackPollution
                 ? QStringLiteral("HIJACKED")
                 : verdict == DnsVerdict::AllFailed ? QStringLiteral("All Failed")
                 : verdict == DnsVerdict::Mixed && hijackFailed > 0 ? QStringLiteral("Mixed")
                 : QStringLiteral("Clean");
        if (hijackDetected) p1.severity = ResultPropertySeverity::Warning;
        p1.children.append({QStringLiteral("Random domains tested"),
            QString::number(hijackTotal)});   // 5WHY v5: 含错误桶
        p1.children.append({QStringLiteral("Clean"), QString::number(hijackClean)});
        p1.children.append({QStringLiteral("Hijacked"), QString::number(hijackWarn)});
        p1.children.append({QStringLiteral("Timeout/Error"), QString::number(hijackFailed)});   // 5WHY v5: 超时+错误桶
        if (!hijackIPs.isEmpty())
            p1.children.append({QStringLiteral("Hijack IPs"),
                hijackIPs.join(QStringLiteral(", "))});
        dprops.append(p1);

        ResultProperty p2;
        p2.label = QStringLiteral("Phase 2 · Pollution Cross-check");
        p2.value = p2verdict;
        if (pollutionDetected) p2.severity = ResultPropertySeverity::Warning;
        p2.children.append({QStringLiteral("Benchmark domains"), QString::number(pollutionTotal)});   // 5WHY v5: 单源
        p2.children.append({QStringLiteral("Clean"), QString::number(pollutionClean)});
        p2.children.append({QStringLiteral("Polluted"), QString::number(pollutionWarn)});
        if (pollutionSuspicious > 0)
            p2.children.append({QStringLiteral("Suspicious"), QString::number(pollutionSuspicious)});
        p2.children.append({QStringLiteral("Errors"), QString::number(pollutionErrors)});
        dprops.append(p2);

        // 5WHY (2026-09-27 v2): 无结论时属性行同样缺失（曾显示 80/100）。
        if (!inconclusiveVerdict) {
            ResultProperty sc;
            sc.label = QStringLiteral("Integrity Score");
            sc.value = QStringLiteral("%1 / 100").arg(overall);
            if (overall < 50) sc.severity = ResultPropertySeverity::Error;
            else if (overall < 80) sc.severity = ResultPropertySeverity::Warning;
            dprops.append(sc);
        }
    }

    DiagnosticResult r = makeResult(id, status, summary, dprops, out.join(QLatin1Char('\n')));
    // 5WHY (2026-09-27 死载荷): 曾写入 5 个零消费键（phase1HijackDetected/
    // phase1Clean/phase1HijackWarn/phase1Timeout/phase2Verdict）——全仓无读者
    // （详情页读 dprops，ReportEngine 只读 narrativeKey/narrativeArgs），
    // 每轮结果序列化进报告/剪贴板纯属死重。移除。
    if (!inconclusiveVerdict)
        r.data[QStringLiteral("overallScorePercent")] = overall;
    // 摘要卡推导叙述：两阶段检测方法与结论依据（用户可复现判断链）
    r.narrative = QStringLiteral("Phase 1 (ISP hijack): %1 randomly-named test domains were resolved — "
        "%2 clean, %3 hijacked, %4 timeout/error. ")
        .arg(hijackTotal).arg(hijackClean).arg(hijackWarn).arg(hijackFailed)
        + QStringLiteral("Phase 2 (pollution): %1 benchmark domains compared local UDP vs DoH — "
        "%2 clean, %3 polluted, %4 suspicious, %5 errors. ")
        .arg(pollutionTotal)
        .arg(pollutionClean).arg(pollutionWarn).arg(pollutionSuspicious).arg(pollutionErrors)
        + (hijackDetected ? QStringLiteral("A hijack responder returned answers for non-existent domains (hijack IPs: %1). ")
            .arg(hijackIPs.join(QStringLiteral(", ")))
            : hijackClean == 0
                ? QStringLiteral("All hijack probe queries timed out or errored — no conclusion about a hijack responder can be drawn. ")
                : QStringLiteral("Non-existent domains did not resolve — no hijack responder found. "));
    if (pollutionDetected)
        r.narrative += QStringLiteral("Local answers diverged from DoH ground truth on %1 domain(s) — evidence of DNS pollution. ")
            .arg(pollutionWarn);
    // 5WHY (2026-09-27 v3): 无结论时叙述同样不得报分（曾无条件追加「总分
    // 100/100」——伪健康分在叙述层复活）。
    if (!inconclusiveVerdict)
        r.narrative += QStringLiteral("Overall integrity score: %1/100.").arg(overall);
    // 5WHY (2026-08-23 叙述多语言): key+args 模板（T.trNarrative 按语言格式化；
    // narrative EN 保留为回退/剪贴板源）。
    // 5WHY (2026-09-27 v3 无结论分键): 曾 nDnsIntegrity 恒键——无结论时分数
    // 槽传空，但全部模板无条件以「…%10/100」收尾，渲染「/100」缺口（模板
    // 渲染缺口，评审抓获）。无结论走无分句变体键（与 nMtuStd/nMtuLocal 分键
    // 同门），有结论仍用完整模板。
    r.data[QStringLiteral("narrativeKey")] = inconclusiveVerdict
        ? QStringLiteral("nDnsIntegrityNoScore")
        : QStringLiteral("nDnsIntegrity");
    r.data[QStringLiteral("narrativeArgs")] = QVariantList{
        QString::number(hijackTotal), QString::number(hijackClean),
        QString::number(hijackWarn), QString::number(hijackFailed),
        QString::number(pollutionTotal),
        QString::number(pollutionClean), QString::number(pollutionWarn),
        QString::number(pollutionSuspicious), QString::number(pollutionErrors),
        // 无结论时分数槽传空（完整模板已不再用于无结论路径；变体键不读该槽）
        inconclusiveVerdict ? QString() : QString::number(overall) };
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G3GeoIPLoc — country/city/ISP via HTTPS + TCP-RTT VPN heuristic
// ═════════════════════════════════════════════════════════════════════════
// 两字母码合理性门禁 (2026-08-23 review 残留④): 曾仅验 cc.size()==2——长度
// 区分不了真码与失败标记，提供商哨兵 "XX"/乱码两字符可入库缓存并被后续轮次
// 复用。改为大写拉丁字母校验并显式拒绝 "XX"；不做 kCountryMap 全表成员校验
// ——表缺 JE/GG/IM 等小 territory，成员校验会误杀合法码（与"确保能返回国别"
// 相悖），未知但形态合法的码交由显示层 countryFullName 原样透出。
static bool plausibleCountryCode(const QString& cc) {
    // 5WHY (review 2026-08-23): "ZZ" 是另一家常见"未知"哨兵（部分提供商失败
    // 时返回）——不拒则入库缓存并经 countryFullName 原样透出到 UI（与 XX
    // 同类泄漏）。ISO 3166 无正式 ZZ 分配，拒绝安全。
    if (cc.size() != 2 || cc == QLatin1String("XX") || cc == QLatin1String("ZZ")) return false;
    for (const QChar ch : cc) {
        if (!ch.isLetter() || !ch.isUpper() || ch.unicode() > 0x7F) return false;
    }
    return true;
}

// 5WHY (2026-09-26 陈旧国家缓存): 缓存曾为 detectCountry 函数级静态——跨运行
// 永不失效：VPN 切换/漫游后第二轮仍报上轮国家（同类缺陷已在 DnsResolver/
// GeoProbe 修复，漏此一处）。每轮 run 开始由 AppState 显式调
// clearDetectCountryCache()。文件级非平凡静态改 Meyer 惰性（同轮 SIOF 规则，
// 2026-09-26 简化收敛）。
struct CountryCacheState {
    QMutex mutex;
    QString cached;
};
CountryCacheState& countryCacheState() {
    static CountryCacheState s;
    return s;
}

void clearDetectCountryCache() {
    QMutexLocker lock(&countryCacheState().mutex);
    countryCacheState().cached.clear();
}

static QString detectCountry(int timeoutMs = 5000) {
    {
        QMutexLocker lock(&countryCacheState().mutex);
        const QString& c = countryCacheState().cached;
        if (!c.isEmpty() && c != QLatin1String("XX")) return c;
    }
    // 5WHY (2026-08-23 用户 "国别返回 XX"): 三家提供商任一超时/限流即
    // 整体 XX——单一故障点连锁。扩展为五家异构提供商（JSON/纯文本各半），
    // 首个有效两字母码即收敛；全败才 XX（UI 层仍映射为 Unknown，见
    // countryFullName）。
    static const struct { const char* url; int parser; } providers[] = {
        {"https://ip-api.com/json/",         0},   // JSON countryCode
        {"https://ipwho.is/",                2},   // JSON country_code/country
        {"https://ipinfo.io/json/",          2},   // JSON country
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
        } else if (providers[pr.first].parser == 2) {
            const QJsonDocument doc = QJsonDocument::fromJson(body);
            if (doc.isObject()) {
                const QJsonObject o = doc.object();
                // 5WHY (review 2026-08-23): 门禁须在归一化后判定——曾对原值
                // 校验，小写码（"us"）被 isUpper 误拒而误走 country 回退字段
                // （ipwho.is 该键是国家全名，必被再拒）→ 整家提供商白费。
                // toUpper 后再验：ipinfo.io 无 country_code 键、其 "country"
                // 存两字母码；ipwho.is 同名键为全名被拒——语义自洽。
                cc = o.value(QStringLiteral("country_code")).toString().toUpper();
                if (!plausibleCountryCode(cc))
                    cc = o.value(QStringLiteral("country")).toString().toUpper();
            }
        } else {
            if (text.size() == 2) cc = text.toUpper();
        }
        cc = cc.toUpper();   // JSON 路径原样取值，统一归一化大写（缓存键一致性）
        if (plausibleCountryCode(cc)) {
            QMutexLocker lock(&countryCacheState().mutex);
            countryCacheState().cached = cc;
            return cc;
        }
    }
    QMutexLocker lock(&countryCacheState().mutex);
    countryCacheState().cached = QStringLiteral("XX");
    return countryCacheState().cached;
}

// ── v0.0.3 复刻：Mann-Whitney U 精确置换检验 + Cliff's Delta ─────────
// 5WHY (复核 2026-08-21 用户 "Geo Location 返回错误数据"): 现版 VPN 判定
// 是自造的 RTT 中位数比较启发式——VPN 出口同国时把代理延迟当物理延迟，
// 结论常反。v0.0.3 的科学判据：GeoIP 国家 vs TTFB 最低 HL 物理国家两个
// 样本组的 Mann-Whitney U 置换检验（p<0.05）+ Cliff's Delta 效应量
// （|δ|≥0.33），决策矩阵四象限。逐字复刻（含 ranks 修正——曾传原始
// TTFB 值导致 p 值恒 1.0，VPN 检测成死代码）。
static double exactPermutationPValue(const QVector<double>& combined,
                                      int nA, int nB, double obsDev) {
    int N = nA + nB;
    double mu = nA * nB / 2.0;
    int extremeCount = 0, totalPerms = 0;
    unsigned maxMask = 1u << N;
    for (unsigned mask = 0; mask < maxMask; mask++) {
        int count = 0;
        for (int i = 0; i < N; i++) if (mask & (1u << i)) count++;
        if (count != nA) continue;
        totalPerms++;
        double rankSum = 0;
        for (int i = 0; i < N; i++)
            if (mask & (1u << i)) rankSum += combined[i];
        double U = rankSum - nA * (nA + 1.0) / 2.0;
        if (std::abs(U - mu) >= obsDev) extremeCount++;
    }
    return totalPerms > 0 ? (double)extremeCount / totalPerms : 1.0;
}

static double cliffDelta(double U, int nA, int nB) {
    if (nA <= 0 || nB <= 0) return 0;
    return 1.0 - 2.0 * U / (nA * nB);
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
    // 5WHY (review 2026-08-23): 主提供商（ip-api）返回值曾未过形态门禁——
    // 两字符乱码/失败标记会直接进展示链与 data。统一门禁：非合法形态即走
    // 多提供商冗余回退（detectCountry 内含同门禁，失败时返回 XX 由显示层
    // 映射 Unknown）。
    if (!plausibleCountryCode(cc)) cc = detectCountry(5000);
    // 5WHY (2026-08-23 用户 "国别应显示国家全名"): 属性卡曾显示裸两字母
    // 码（US/CN），全败时甚至显示内部哨兵 "XX"——码对用户不友好且 XX 是
    // 内部失败标记不应泄漏到 UI。显示层统一国家全名；码保留在 data 层
    // 供机器消费。
    const QString countryDisplay = SystemDiagnostics::countryFullName(cc);   // "XX"/空 → "Unknown"
    props.append({QStringLiteral("Country"), countryDisplay});
    out.append(QStringLiteral("  Country: %1").arg(countryDisplay));
    if (!city.isEmpty())    { props.append({QStringLiteral("City"), city});     out.append(QStringLiteral("  City: %1").arg(city)); }
    if (!isp.isEmpty())     { props.append({QStringLiteral("ISP"), isp});       out.append(QStringLiteral("  ISP: %1").arg(isp)); }
    if (!asName.isEmpty())  { props.append({QStringLiteral("AS"), asName});     out.append(QStringLiteral("  AS: %1").arg(asName)); }
    if (hasCoords)          { props.append({QStringLiteral("Coordinates"),
        QStringLiteral("%1, %2").arg(lat, 0, 'f', 4).arg(lon, 0, 'f', 4)});
        out.append(QStringLiteral("  Coordinates: %1, %2").arg(lat, 0, 'f', 4).arg(lon, 0, 'f', 4)); }
    if (!publicIp.isEmpty()){ props.append({QStringLiteral("Public IP"), publicIp}); out.append(QStringLiteral("  Public IP: %1").arg(publicIp)); }

    // ── v0.0.3 复刻：Phase 2/3/4 — TTFB 物理定位 + Mann-Whitney VPN 判决 ──
    // 5WHY (复核 2026-08-21 用户 "Geo Location 返回错误数据"): 现版把
    // GeoIP 提供商的出口国家当"所在地"、再用自造 RTT 中位数启发式判 VPN
    // ——VPN 出口与物理位置混淆，国家与 VPN 结论都可能是错的。v0.0.3 方法：
    // GeoIP 国家（countryA）只作参考；物理位置（countryB）由 138 台测速
    // 服务器库 TTFB 全局探测的最低 Hodges-Lehmann 国家给出；再对两组 TTFB
    // 样本做 Mann-Whitney U 精确置换检验 + Cliff's Delta 四象限判决。
    ProbeConfig cfg;
    cfg.scope = ProbeConfig::Global;
    cfg.rounds = 1;       // 单轮快速国家探测（与历史一致）
    cfg.aggregation = ProbeConfig::Aggregation::ByCountry;
    // 5WHY (2026-09-26 取消缺口): getFeedback 阻塞至 120s——曾无取消咨询点。
    // AppState::cancel() 现调 GeoProbe::clear()（代际变化令 waitForCompletion
    // 立即返回），此处把取消转换为 Cancelled 终态。
    if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
    GeoProbe::instance().probe(cfg);
    const ProbeResult result = GeoProbe::instance().getFeedback(cfg, &ctx.cancelled);
    if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));

    out.append(QString());
    out.append(QStringLiteral("[Phase 2/4] TTFB Probe Complete — %1 Reachable, %2 Countries")
        .arg(result.servers.size()).arg(result.countries.size()));

    // Step 3：最低 HL 国家 = 物理位置；聚合空时回退多数国家
    QString countryB = result.physicalCountry;
    out.append(QStringLiteral("Physical Location (Lowest HL): %1")
        .arg(SystemDiagnostics::countryFullName(countryB)));
    if (countryB == QLatin1String("XX") || result.countries.isEmpty()) {
        if (!result.servers.isEmpty()) {
            QHash<QString, int> countPerCountry;
            for (const auto& srv : result.servers)
                if (srv.ok && srv.ttfbMs > 0) countPerCountry[srv.country]++;
            int bestN = 0; QString bestCC;
            for (auto it = countPerCountry.begin(); it != countPerCountry.end(); ++it)
                if (it.value() > bestN) { bestN = it.value(); bestCC = it.key(); }
            if (bestN > 0) {
                countryB = bestCC;
                out.append(QStringLiteral("Physical Location (Fallback): %1 (%2 Reachable)")
                    .arg(SystemDiagnostics::countryFullName(countryB)).arg(bestN));
            }
        }
        if (countryB.isEmpty() || countryB == QLatin1String("XX")) {
            out.append(QStringLiteral("Status: Insufficient Data for VPN Analysis"));
            DiagnosticResult early = makeResult(id, DiagStatus::Warning,
                QStringLiteral("GeoIP: %1, Physical: Unknown").arg(SystemDiagnostics::countryFullName(cc)),
                props, out.join(QLatin1Char('\n')));
            early.data[QStringLiteral("countryCode")] = cc;
            early.data[QStringLiteral("countryName")] = SystemDiagnostics::countryFullName(cc);
            early.data[QStringLiteral("city")] = city;
            early.data[QStringLiteral("isp")] = isp;
            early.data[QStringLiteral("as")] = asName;
            early.data[QStringLiteral("publicIp")] = publicIp;
            early.data[QStringLiteral("vpnSuspected")] = false;
            early.data[QStringLiteral("vpnVerdict")] = QStringLiteral("Insufficient data for VPN analysis");
            early.data[QStringLiteral("vpnConfidence")] = 30;
            early.narrative = QStringLiteral("GeoIP country is %1, but the TTFB probe could not determine "
                "a physical location — VPN analysis skipped.").arg(SystemDiagnostics::countryFullName(cc));
            return early;
        }
    }

    // Top 5 物理位置表（按 HL 延迟升序）
    if (!result.countries.isEmpty()) {
        out.append(QString());
        out.append(QStringLiteral("Top 5 Physical Locations:"));
        out.append(QString());
        const int n = qMin(5, result.countries.size());
        static const QVector<DiagnosticFormatter::ColSpec> kLocCols = {
            {"Rank",              4, true},
            {"Country",          20, false},
            {"HL Latency (ms)", 15, true},
            {"Servers",           7, true},
        };
        QList<QStringList> rows;
        rows.reserve(n);
        for (int i = 0; i < n; ++i) {
            const auto& cr = result.countries[i];
            rows.append({
                QString::number(i + 1),
                SystemDiagnostics::countryFullName(cr.code),
                QStringLiteral("%1").arg(cr.hlMs, 0, 'f', 1),
                QString::number(cr.serverCount),
            });
        }
        out.append(DiagnosticFormatter::formatTable(kLocCols, rows));
    }

    const QString countryA = cc;
    if (countryA == QLatin1String("XX") || countryA.isEmpty()) {
        out.append(QStringLiteral("Status: Location Estimated as %1 (GeoIP Unreachable)")
            .arg(SystemDiagnostics::countryFullName(countryB)));
        DiagnosticResult early = makeResult(id, DiagStatus::Warning,
            QStringLiteral("Physical: %1 (GeoIP Unreachable)").arg(SystemDiagnostics::countryFullName(countryB)),
            props, out.join(QLatin1Char('\n')));
        early.data[QStringLiteral("countryCode")] = countryB;
        early.data[QStringLiteral("city")] = city;
        early.data[QStringLiteral("isp")] = isp;
        early.data[QStringLiteral("as")] = asName;
        early.data[QStringLiteral("publicIp")] = publicIp;
        early.data[QStringLiteral("vpnSuspected")] = false;
        early.data[QStringLiteral("vpnVerdict")] = QStringLiteral("GeoIP providers unreachable — physical location only");
        early.data[QStringLiteral("vpnConfidence")] = -1;
        early.narrative = QStringLiteral("GeoIP providers were unreachable. The physical location was "
            "estimated as %1 from the TTFB probe.").arg(SystemDiagnostics::countryFullName(countryB));
        return early;
    }

    // Step 4：VPN 检测 — 两组 TTFB 样本的 Mann-Whitney U + Cliff's Delta
    out.append(QStringLiteral("[Phase 3/4] VPN Detection — Permutation Test..."));

    QVector<double> samplesA, samplesB;
    for (const auto& cr : result.countries) {
        for (const auto& srv : cr.servers) {
            if (srv.country == countryA) samplesA.append(srv.ttfbMs);
            if (srv.country == countryB) samplesB.append(srv.ttfbMs);
        }
    }

    const int nA = samplesA.size(), nB = samplesB.size();
    if (nA < 3 || nB < 3) {
        out.append(QStringLiteral("GeoIP Country %1: %2 Samples, Physical Country %3: %4 Samples — Insufficient for VPN Test")
            .arg(SystemDiagnostics::countryFullName(countryA)).arg(nA)
            .arg(SystemDiagnostics::countryFullName(countryB)).arg(nB));
        DiagnosticResult early = makeResult(id, DiagStatus::Info,
            QStringLiteral("Physical: %1, GeoIP: %2 (Insufficient Data for VPN)")
                .arg(SystemDiagnostics::countryFullName(countryB), SystemDiagnostics::countryFullName(countryA)),
            props, out.join(QLatin1Char('\n')));
        early.data[QStringLiteral("countryCode")] = cc;
        early.data[QStringLiteral("city")] = city;
        early.data[QStringLiteral("isp")] = isp;
        early.data[QStringLiteral("as")] = asName;
        early.data[QStringLiteral("publicIp")] = publicIp;
        early.data[QStringLiteral("vpnSuspected")] = false;
        early.data[QStringLiteral("vpnVerdict")] = QStringLiteral("Insufficient samples for VPN test");
        early.data[QStringLiteral("vpnConfidence")] = 30;
        early.narrative = QStringLiteral("GeoIP country %1 (%2 samples) vs physical country %3 (%4 samples) — "
            "too few TTFB samples for a statistical VPN test.")
            .arg(SystemDiagnostics::countryFullName(countryA)).arg(nA)
            .arg(SystemDiagnostics::countryFullName(countryB)).arg(nB);
        return early;
    }

    // 组合样本排序 → 平均秩 → 组 A 秩和 → U / p / δ
    QVector<double> combined = samplesA + samplesB;
    const int N = nA + nB;
    QVector<std::pair<double, int>> indexed(N);
    for (int i = 0; i < N; i++) indexed[i] = {combined[i], i};
    std::sort(indexed.begin(), indexed.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

    QVector<double> ranks(N);
    for (int i = 0; i < N; ) {
        int j = i; while (j < N && indexed[j].first == indexed[i].first) j++;
        const double avgRank = (i + j - 1) / 2.0 + 1;   // 1-based 平均秩
        for (int k = i; k < j; k++) ranks[indexed[k].second] = avgRank;
        i = j;
    }
    double rankSumA = 0;
    for (int i = 0; i < nA; i++) rankSumA += ranks[i];
    const double U = rankSumA - nA * (nA + 1.0) / 2.0;
    const double mu = nA * nB / 2.0;
    const double obsDev = std::abs(U - mu);
    // 5WHY (v0.0.3 死代码修复): 传 ranks 而非原始 TTFB（原始值使
    // |U-mu| 对每个置换都成立 → p 恒 1.0 → VPN 检测恒不触发）。
    const double pValue = (N <= 20) ? exactPermutationPValue(ranks, nA, nB, obsDev) : 1.0;
    const double delta = cliffDelta(U, nA, nB);

    out.append(QStringLiteral("[Phase 4/4] Statistical Results:"));
    out.append(QStringLiteral("  GeoIP (%1): %2 Samples, Physical (%3): %4 Samples")
        .arg(SystemDiagnostics::countryFullName(countryA)).arg(nA)
        .arg(SystemDiagnostics::countryFullName(countryB)).arg(nB));
    out.append(QStringLiteral("  Mann-Whitney U = %1, p-value = %2, Cliff's Delta = %3")
        .arg(U, 0, 'f', 1).arg(pValue, 0, 'f', 4).arg(delta, 0, 'f', 3));

    // ── VPN 决策矩阵（v0.0.3 四象限）──
    bool vpnSuspected = false;
    int vpnConfidence = 30;
    QString vpnVerdict;
    DiagStatus status;
    QString summary;
    auto fmtLoc = [&]() { return QStringLiteral("GeoIP=%1, Physical=%2, p=%3, δ=%4")
        .arg(SystemDiagnostics::countryFullName(countryA), SystemDiagnostics::countryFullName(countryB))
        .arg(pValue, 0, 'f', 4).arg(delta, 0, 'f', 3); };

    if (countryA == countryB) {
        out.append(QStringLiteral("  Status: No VPN — GeoIP and Physical Both %1 (%2)")
            .arg(SystemDiagnostics::countryFullName(countryA)).arg(fmtLoc()));
        summary = QStringLiteral("Physical: %1, GeoIP: %2 → No VPN")
            .arg(SystemDiagnostics::countryFullName(countryB), SystemDiagnostics::countryFullName(countryA));
        status = DiagStatus::Pass;
        vpnSuspected = false;
        vpnConfidence = 10;
        vpnVerdict = QStringLiteral("No VPN — GeoIP and physical location agree (%1)")
            .arg(SystemDiagnostics::countryFullName(countryA));
    } else if (pValue < 0.05 && std::abs(delta) >= 0.33) {
        out.append(QStringLiteral("  Status: VPN DETECTED — %1").arg(fmtLoc()));
        summary = QStringLiteral("Physical: %1, GeoIP: %2 → VPN Detected")
            .arg(SystemDiagnostics::countryFullName(countryB), SystemDiagnostics::countryFullName(countryA));
        status = DiagStatus::Info;
        vpnSuspected = true;
        vpnConfidence = 90;
        vpnVerdict = QStringLiteral("VPN detected — GeoIP %1 vs physical %2 (p=%3, δ=%4)")
            .arg(SystemDiagnostics::countryFullName(countryA), SystemDiagnostics::countryFullName(countryB))
            .arg(pValue, 0, 'f', 4).arg(delta, 0, 'f', 3);
    } else if (pValue < 0.05 && std::abs(delta) < 0.33) {
        out.append(QStringLiteral("  Status: VPN Likely — Significant Latency Difference, Small Effect (%1)")
            .arg(fmtLoc()));
        summary = QStringLiteral("Physical: %1, GeoIP: %2 → VPN Likely")
            .arg(SystemDiagnostics::countryFullName(countryB), SystemDiagnostics::countryFullName(countryA));
        status = DiagStatus::Info;
        vpnSuspected = true;
        vpnConfidence = 70;
        vpnVerdict = QStringLiteral("VPN likely — significant latency difference (p=%1), small effect (δ=%2)")
            .arg(pValue, 0, 'f', 4).arg(delta, 0, 'f', 3);
    } else if (std::abs(delta) >= 0.33) {
        out.append(QStringLiteral("  Status: VPN Possible — Medium Effect, Inconclusive Significance (%1)")
            .arg(fmtLoc()));
        summary = QStringLiteral("Physical: %1, GeoIP: %2 → VPN Possible")
            .arg(SystemDiagnostics::countryFullName(countryB), SystemDiagnostics::countryFullName(countryA));
        status = DiagStatus::Info;
        vpnSuspected = true;
        vpnConfidence = 50;
        vpnVerdict = QStringLiteral("VPN possible — medium effect (δ=%1), inconclusive significance (p=%2)")
            .arg(delta, 0, 'f', 3).arg(pValue, 0, 'f', 4);
    } else {
        out.append(QStringLiteral("  Status: Inconclusive — %1").arg(fmtLoc()));
        summary = QStringLiteral("Physical: %1, GeoIP: %2 → Inconclusive")
            .arg(SystemDiagnostics::countryFullName(countryB), SystemDiagnostics::countryFullName(countryA));
        status = DiagStatus::Info;
        vpnSuspected = false;
        vpnConfidence = 30;
        vpnVerdict = QStringLiteral("Inconclusive — latency pattern shows no clear VPN signal");
    }

    // 5WHY (2026-08-23 详情页信息前置): Top 物理位置表与统计判决数字曾只在
    // terminal 散文里——提炼为分组属性行（PagePropertiesSection Kv 模式下
    // children 以 "· " 前缀缩进渲染，两态通用），p 值等统计量同时给出置信
    // 语言化标签，普通用户无需解读裸统计量。
    auto confidenceWord = [](int c) -> QString {
        return c >= 80 ? QStringLiteral("High")
             : c >= 50 ? QStringLiteral("Medium")
             : c > 0   ? QStringLiteral("Low")
                       : QStringLiteral("Unknown");
    };
    if (!result.countries.isEmpty()) {
        ResultProperty loc;
        loc.label = QStringLiteral("Top Physical Locations");
        const int nTop = qMin(5, result.countries.size());
        for (int i = 0; i < nTop; ++i) {
            const auto& cr = result.countries[i];
            loc.children.append({SystemDiagnostics::countryFullName(cr.code),
                QStringLiteral("%1 ms · %2 server(s)").arg(cr.hlMs, 0, 'f', 1).arg(cr.serverCount)});
        }
        props.append(loc);
    }
    {
        ResultProperty vpn;
        vpn.label = QStringLiteral("VPN Statistical Verdict");
        vpn.value = confidenceWord(vpnConfidence)
            + (vpnSuspected ? QStringLiteral(" — VPN suspected") : QString());
        vpn.severity = vpnSuspected ? ResultPropertySeverity::Warning : ResultPropertySeverity::Info;
        vpn.children.append({QStringLiteral("Samples"),
            QStringLiteral("%1 GeoIP vs %2 physical").arg(nA).arg(nB)});
        vpn.children.append({QStringLiteral("Mann-Whitney U"), QString::number(U, 'f', 1)});
        vpn.children.append({QStringLiteral("p-value"), QString::number(pValue, 'f', 4)});
        vpn.children.append({QStringLiteral("Cliff's δ"), QString::number(delta, 'f', 3)});
        vpn.children.append({QStringLiteral("Decision"), vpnVerdict});
        props.append(vpn);
    }

    DiagnosticResult r = makeResult(id, status, summary, props, out.join(QLatin1Char('\n')));
    r.data[QStringLiteral("countryCode")] = cc;
    r.data[QStringLiteral("countryName")] = SystemDiagnostics::countryFullName(cc);
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
    r.data[QStringLiteral("vpnConfidence")] = vpnConfidence;
    // 摘要卡叙述：GeoIP 出口 vs TTFB 物理定位 → 统计判决
    r.narrative = QStringLiteral("Egress IP %1 (GeoIP %2, ISP %3). Physical location from the TTFB probe: %4. ")
        .arg(publicIp.isEmpty() ? QStringLiteral("unknown") : publicIp,
             SystemDiagnostics::countryFullName(countryA),
             isp.isEmpty() ? QStringLiteral("unknown") : isp,
             SystemDiagnostics::countryFullName(countryB))
        + vpnVerdict
        + QStringLiteral(" (Mann-Whitney U = %1, p = %2, Cliff's δ = %3).")
            .arg(U, 0, 'f', 1).arg(pValue, 0, 'f', 4).arg(delta, 0, 'f', 3);
    r.data[QStringLiteral("narrativeKey")] = QStringLiteral("nGeoIp");
    r.data[QStringLiteral("narrativeArgs")] = QVariantList{
        publicIp.isEmpty() ? QStringLiteral("unknown") : publicIp,
        SystemDiagnostics::countryFullName(countryA),
        isp.isEmpty() ? QStringLiteral("unknown") : isp,
        SystemDiagnostics::countryFullName(countryB),
        vpnVerdict,
        QString::number(U, 'f', 1), QString::number(pValue, 'f', 4), QString::number(delta, 'f', 3) };
    return r;
}

// ═════════════════════════════════════════════════════════════════════════
// G3InternetConnectivity — v0.0.3 复刻：TTFB 全局探测选优服务器 + 分档测速
// ═════════════════════════════════════════════════════════════════════════
static DiagnosticResult probeInternetConnectivity(DiagId id, const QString&, RunContext& ctx) {
    // 5WHY (复核 2026-08-21 用户 "Internet Connectivity 与历史源码差距大"):
    // 现版固定打 Cloudflare 三端点 + speed.cloudflare.com 测速，未用 138 台
    // 测速服务器库与 TTFB 选优，检测方法与输出格式均与 v0.0.3 不同。逐字
    // 复刻 v0.0.3 流程：GeoProbe 全局 TTFB 探测（3 轮、HL 聚合）→ 物理
    // 位置 → Top 5 服务器表 → 最佳服务器详情 → DNS/TCP 预检 → 分档
    // （64KB/256KB/1MB）下载/上传测速表（全败重试一次）→ 结论。
    GeoProbe& gp = GeoProbe::instance();
    ProbeConfig cfg;
    cfg.scope = ProbeConfig::Global;
    cfg.rounds = 3;
    cfg.aggregation = ProbeConfig::Aggregation::ByCountry;
    // 5WHY (2026-09-26 取消缺口): 与 G3GeoIPLoc 同门——getFeedback 阻塞至
    // 120s，取消经 GeoProbe::clear() 代际解堵后此处转 Cancelled 终态。
    if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
    gp.probe(cfg);
    const ProbeResult result = gp.getFeedback(cfg, &ctx.cancelled);
    if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));

    // 服务器元数据查找表（key = host:port）
    struct Meta { QString name; QString sponsor; };
    QHash<QString, Meta> metaByKey;
    for (const auto& srv : GeoProbe::allServers()) {
        Meta m; m.name = srv.name; m.sponsor = srv.sponsor;
        metaByKey.insert(srv.host + QLatin1Char(':') + QString::number(srv.port), m);
    }
    QHash<QString, QString> ipCache;
    auto resolveIp = [&](const QString& host) -> QString {
        auto it = ipCache.find(host);
        if (it != ipCache.end()) return it.value();
        const QString ip = DnsResolver::instance().resolve(host, 3000);
        ipCache[host] = ip;
        return ip;
    };

    QStringList out;
    out.append(QStringLiteral("Internet Connectivity"));
    out.append(QStringLiteral("Method: TTFB Global Probe → 3-Round → HL Aggregation → Speed Test"));
    out.append(QString());

    // ── Phase 1: Location ──
    out.append(QStringLiteral("── Phase 1: Location ──"));
    out.append(QStringLiteral("Physical Location: %1").arg(SystemDiagnostics::countryFullName(result.physicalCountry)));
    out.append(QStringLiteral("Probed %1 Servers, %2 Countries Reachable")
        .arg(result.servers.size()).arg(result.countries.size()));
    out.append(QString());

    // ── Phase 2: Top 5 servers ──
    out.append(QStringLiteral("── Phase 2: Top 5 Servers ──"));
    int shown = 0;
    QList<QStringList> topRows;
    for (const auto& sr : result.servers) {
        if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
        if (shown >= 5) break;
        const QString key = sr.host + QLatin1Char(':') + QString::number(sr.port);
        auto mit = metaByKey.constFind(key);
        const QString name = (mit != metaByKey.cend()) ? mit->name : sr.host;
        const QString ip = resolveIp(sr.host);
        topRows.append({
            QString::number(shown + 1),
            name,
            SystemDiagnostics::countryCode3(sr.country),
            ip.isEmpty() ? sr.host : ip,
            QStringLiteral("%1ms").arg(sr.ttfbMs, 0, 'f', 1),
            QStringLiteral("±%1ms").arg(sr.ciHalf, 0, 'f', 1),
        });
        shown++;
    }
    if (!topRows.isEmpty()) {
        static const QVector<DiagnosticFormatter::ColSpec> kTopCols = {
            {"Rank",    4, true},
            {"Server", 28, false},
            {"CC",      3, false},
            {"IP",     16, false},
            {"TTFB",    8, true},
            {"95% CI",  8, true},
        };
        out.append(DiagnosticFormatter::formatTable(kTopCols, topRows));
    }
    out.append(QString());

    if (result.servers.isEmpty()) {
        out.append(QStringLiteral("No Reachable Server Found"));
        DiagnosticResult r = makeResult(id, DiagStatus::Fail,
            QStringLiteral("No Internet Connectivity"), {}, out.join(QLatin1Char('\n')));
        r.data[QStringLiteral("connected")] = false;
        r.data[QStringLiteral("latencyMs")] = -1;
        r.data[QStringLiteral("downloadMbps")] = 0.0;
        r.data[QStringLiteral("uploadMbps")] = 0.0;
        r.data[QStringLiteral("downloadMbpsBest")] = 0.0;
        r.data[QStringLiteral("uploadMbpsBest")] = 0.0;
        r.narrative = QStringLiteral("No speed-test server was reachable — the device appears to have no Internet connectivity.");
        r.data[QStringLiteral("narrativeKey")] = QStringLiteral("nInternetNoServer");
        r.data[QStringLiteral("narrativeArgs")] = QVariantList{};
        return r;
    }

    // ── Phase 3+4: 服务器 failover 循环（主备各跑一遍分档测速）──
    // 5WHY (复核 2026-08-21 用户 "下载测试出错"): GeoProbe 选优只看 TTFB
    // （连接+首字节），不测吞吐——首字节快但吞吐慢/停滞的服务器会当选，
    // 分档测速全部失败仍只报一个服务器的结果。业界惯例（Ookla 等）：
    // 主服务器测速失败时切换下一候选（failover ≤ 2）。
    bool anyDlOk = false, anyUlOk = false;
    double bestDlMbps = 0, bestUlMbps = 0;
    double chosenTtfb = 0;
    QString chosenName;
    int pingMs = -1;
    // 5WHY (2026-08-23 详情页信息前置): 服务器档案与分档测速结果曾只活在
    // terminal 文本里——候选循环中捕获为局部事实，循环结束后提炼成分组
    // 属性（Properties 卡直读），terminal 折叠后信息不丢失。
    QString chosenHostPort, chosenIp, chosenCountryFull, chosenSponsor;
    double chosenCiHalf = 0;
    int chosenRounds = 0;
    QVector<QPair<QString, QString>> dlFacts, ulFacts;
    const int kCandidates = qMin(2, result.servers.size());
    struct Tier { int bytes; const char* label; };
    const Tier kDlTiers[] = { { 64000, "64 KB" }, { 256000, "256 KB" }, { 1000000, "1 MB" } };
    const Tier kUlTiers[] = { { 64000, "64 KB" }, { 256000, "256 KB" }, { 1000000, "1 MB" } };
    static const QVector<DiagnosticFormatter::ColSpec> kSpeedCols = {
        {"Size",     7,  true},
        {"Speed",   10,  true},
        {"Time",     6,  true},
        {"Bytes",    8,  true},
        {"Status",  18, false},
    };
    for (int cand = 0; cand < kCandidates; ++cand) {
        const ServerResult& best = result.servers[cand];
        const QString bestKey = best.host + QLatin1Char(':') + QString::number(best.port);
        auto bestMeta = metaByKey.constFind(bestKey);
        const QString bestIp = resolveIp(best.host);
        chosenTtfb = best.ttfbMs;
        chosenName = bestMeta != metaByKey.cend() ? bestMeta->name : best.host;
        chosenHostPort = QStringLiteral("%1:%2").arg(best.host).arg(best.port);
        chosenIp = bestIp;
        chosenCountryFull = SystemDiagnostics::countryFullName(best.country);
        chosenSponsor = (bestMeta != metaByKey.cend()) ? bestMeta->sponsor : QString();
        chosenCiHalf = best.ciHalf;
        chosenRounds = best.rounds;

        out.append(QStringLiteral("── Phase 3: Best Server ──"));
        out.append(QStringLiteral("  Name:    %1").arg(chosenName));
        if (bestMeta != metaByKey.cend() && !bestMeta->sponsor.isEmpty())
            out.append(QStringLiteral("  Sponsor: %1").arg(bestMeta->sponsor));
        out.append(QStringLiteral("  Host:    %1:%2").arg(best.host).arg(best.port));
        out.append(QStringLiteral("  IP:      %1").arg(bestIp.isEmpty() ? QStringLiteral("(Unresolved)") : bestIp));
        out.append(QStringLiteral("  Country: %1").arg(SystemDiagnostics::countryFullName(best.country)));
        out.append(QStringLiteral("  TTFB:    %1ms (95% CI ±%2ms, %3 rounds)")
            .arg(best.ttfbMs, 0, 'f', 1).arg(best.ciHalf, 0, 'f', 1).arg(best.rounds));
        out.append(QString());

        // ── Phase 4: Speed Test ──
        out.append(QStringLiteral("── Phase 4: Speed Test ──"));
        out.append(QStringLiteral("Server: %1:%2").arg(best.host).arg(best.port));
        if (bestIp.isEmpty())
            out.append(QStringLiteral("  DNS:     ✗ Failed — Hostname Not Resolved"));
        else
            out.append(QStringLiteral("  DNS:     ✓ %1").arg(bestIp));
        pingMs = SystemDiagnostics::tcpPingMs(best.host, best.port);
        if (pingMs < 0)
            out.append(QStringLiteral("  Ping:    ✗ TCP Connect Failed"));
        else
            out.append(QStringLiteral("  Ping:    ✓ %1ms TCP Connect").arg(pingMs));
        out.append(QString());

        QStringList dlOut, ulOut;
        // 5WHY (v0.0.3): 网络抖动可能让全部档位首轮失败——单次重试后再报失败。
        for (int pass = 0; pass < 2; ++pass) {
            if (pass == 1) {
                out.append(QStringLiteral("  (Retry after all tiers failed)"));
                out.append(QString());
            }
            anyDlOk = false; anyUlOk = false;
            bestDlMbps = 0; bestUlMbps = 0;
            dlOut.clear(); ulOut.clear();
            dlFacts.clear(); ulFacts.clear();

            dlOut.append(QStringLiteral("  Download:"));
            dlOut.append(QString());
            {
                QList<QStringList> dlRows;
                for (const auto& tier : kDlTiers) {
                    if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
                    const QString dlUrl = QStringLiteral("http://%1:%2/download?size=%3")
                        .arg(best.host).arg(best.port).arg(tier.bytes);
                    const SystemDiagnostics::SpeedResult dl = SystemDiagnostics::httpDownload(dlUrl, tier.bytes, 15000);
                    if (dl.ok && dl.mbps > 0.01) {
                        dlRows.append({ QString::fromLatin1(tier.label),
                            QStringLiteral("%1 Mbps").arg(dl.mbps, 0, 'f', 1),
                            QStringLiteral("%1ms").arg(dl.durationMs),
                            QString::number(dl.bytes), QStringLiteral("✓") });
                        dlFacts.append({ QString::fromLatin1(tier.label),
                            QStringLiteral("%1 Mbps · %2ms").arg(dl.mbps, 0, 'f', 1).arg(dl.durationMs) });
                        anyDlOk = true;
                        if (dl.mbps > bestDlMbps) bestDlMbps = dl.mbps;
                    } else {
                        const QString err = dl.error.isEmpty() ? QStringLiteral("Unknown Error") : dl.error;
                        dlRows.append({ QString::fromLatin1(tier.label), QStringLiteral("—"),
                            QStringLiteral("—"), QStringLiteral("—"), err });
                    }
                }
                dlOut.append(DiagnosticFormatter::formatTable(kSpeedCols, dlRows));
            }
            dlOut.append(QString());

            ulOut.append(QStringLiteral("  Upload:"));
            ulOut.append(QString());
            {
                QList<QStringList> ulRows;
                for (const auto& tier : kUlTiers) {
                    if (ctx.cancelled.load()) return DiagnosticResult::cancelled(id, QStringLiteral("Cancelled"));
                    const QString ulUrl = QStringLiteral("http://%1:%2/upload").arg(best.host).arg(best.port);
                    const SystemDiagnostics::SpeedResult ul = SystemDiagnostics::httpUpload(ulUrl, tier.bytes, 15000);
                    if (ul.ok && ul.mbps > 0.01) {
                        ulRows.append({ QString::fromLatin1(tier.label),
                            QStringLiteral("%1 Mbps").arg(ul.mbps, 0, 'f', 1),
                            QStringLiteral("%1ms").arg(ul.durationMs),
                            QString::number(ul.bytes), QStringLiteral("✓") });
                        ulFacts.append({ QString::fromLatin1(tier.label),
                            QStringLiteral("%1 Mbps · %2ms").arg(ul.mbps, 0, 'f', 1).arg(ul.durationMs) });
                        anyUlOk = true;
                        if (ul.mbps > bestUlMbps) bestUlMbps = ul.mbps;
                    } else {
                        const QString err = ul.error.isEmpty() ? QStringLiteral("Unknown Error") : ul.error;
                        ulRows.append({ QString::fromLatin1(tier.label), QStringLiteral("—"),
                            QStringLiteral("—"), QStringLiteral("—"), err });
                    }
                }
                ulOut.append(DiagnosticFormatter::formatTable(kSpeedCols, ulRows));
            }
            ulOut.append(QString());

            if (anyDlOk || anyUlOk) break;
        }
        out.append(dlOut);
        out.append(ulOut);
        if (anyDlOk || anyUlOk) break;
        if (cand + 1 < kCandidates) {
            out.append(QString());
            out.append(QStringLiteral("  (Speed test failed — trying next server candidate)"));
            out.append(QString());
        }
    }

    // ── Summary（v0.0.3 语式）──
    DiagStatus status;
    QString summary;
    if (anyDlOk && anyUlOk) {
        summary = QStringLiteral("Connected — %1 (%2ms, ↓%3/↑%4 Mbps)")
            .arg(SystemDiagnostics::countryFullName(result.physicalCountry)).arg(chosenTtfb, 0, 'f', 0)
            .arg(bestDlMbps, 0, 'f', 1).arg(bestUlMbps, 0, 'f', 1);
        status = DiagStatus::Pass;
    } else if (anyDlOk) {
        summary = QStringLiteral("Connected — %1 (%2ms, ↓%3 Mbps, Upload N/A)")
            .arg(SystemDiagnostics::countryFullName(result.physicalCountry)).arg(chosenTtfb, 0, 'f', 0)
            .arg(bestDlMbps, 0, 'f', 1);
        status = DiagStatus::Warning;
    } else if (anyUlOk) {
        summary = QStringLiteral("Connected — %1 (%2ms, Download N/A, ↑%3 Mbps)")
            .arg(SystemDiagnostics::countryFullName(result.physicalCountry)).arg(chosenTtfb, 0, 'f', 0)
            .arg(bestUlMbps, 0, 'f', 1);
        status = DiagStatus::Warning;
    } else {
        summary = QStringLiteral("Connected — %1 (%2ms, Speed Test Failed)")
            .arg(SystemDiagnostics::countryFullName(result.physicalCountry)).arg(chosenTtfb, 0, 'f', 0);
        status = DiagStatus::Warning;
    }

    // 5WHY (2026-08-23 详情页信息前置): Best Server 档案 / Probe Coverage /
    // 分档测速轮次从 terminal 提炼为分组属性——terminal 折叠后（§5 方案）
    // 这些是用户读取主要结果的唯一入口。
    QVector<ResultProperty> cprops;
    {
        // 5WHY (review 2026-08-23): 候选集空（全站不可达）时 chosenName 空——
        // 曾照常产 Best Server 空行。有档案才产出该属性组。
        if (!chosenName.isEmpty()) {
        ResultProperty best;
        best.label = QStringLiteral("Best Server");
        best.value = chosenName;
        if (!chosenSponsor.isEmpty())
            best.children.append({QStringLiteral("Sponsor"), chosenSponsor});
        best.children.append({QStringLiteral("Endpoint"), chosenHostPort});
        if (!chosenIp.isEmpty())
            best.children.append({QStringLiteral("IP"), chosenIp});
        best.children.append({QStringLiteral("Country"), chosenCountryFull});
        best.children.append({QStringLiteral("TTFB"),
            QStringLiteral("%1 ms ± %2 (%3 rounds)")
                .arg(chosenTtfb, 0, 'f', 1).arg(chosenCiHalf, 0, 'f', 1).arg(chosenRounds)});
        if (pingMs >= 0)
            best.children.append({QStringLiteral("TCP Ping"), QStringLiteral("%1 ms").arg(pingMs)});
        cprops.append(best);
        }

        ResultProperty cov;
        cov.label = QStringLiteral("Probe Coverage");
        cov.value = QStringLiteral("%1 / %2 servers")
            .arg(result.servers.size()).arg(GeoProbe::allServers().size());
        cov.children.append({QStringLiteral("Countries reachable"),
            QString::number(result.countries.size())});
        cov.children.append({QStringLiteral("Physical location"),
            SystemDiagnostics::countryFullName(result.physicalCountry)});
        cprops.append(cov);

        if (!dlFacts.isEmpty()) {
            ResultProperty g;
            g.label = QStringLiteral("Download Rounds");
            g.value = QStringLiteral("%1 Mbps best").arg(bestDlMbps, 0, 'f', 1);
            for (const auto& f : dlFacts) g.children.append({f.first, f.second});
            cprops.append(g);
        }
        if (!ulFacts.isEmpty()) {
            ResultProperty g;
            g.label = QStringLiteral("Upload Rounds");
            g.value = QStringLiteral("%1 Mbps best").arg(bestUlMbps, 0, 'f', 1);
            for (const auto& f : ulFacts) g.children.append({f.first, f.second});
            cprops.append(g);
        }
    }

    DiagnosticResult r = makeResult(id, status, summary, cprops, out.join(QLatin1Char('\n')));
    r.data[QStringLiteral("connected")] = true;
    r.data[QStringLiteral("reachableEndpoints")] = result.servers.size();
    r.data[QStringLiteral("bestTtfbMs")] = chosenTtfb;
    r.data[QStringLiteral("edgeRttMs")] = pingMs;
    r.data[QStringLiteral("latencyMs")] = chosenTtfb;
    r.data[QStringLiteral("downloadMbps")] = bestDlMbps;
    r.data[QStringLiteral("uploadMbps")] = bestUlMbps;
    r.data[QStringLiteral("downloadMbpsBest")] = bestDlMbps;
    r.data[QStringLiteral("uploadMbpsBest")] = bestUlMbps;
    // 摘要卡叙述：可达性 → 最佳服务器 → 分档测速结论
    r.narrative = QStringLiteral("Physical location %1; %2/%3 servers reachable. Best server %4 (TTFB %5ms). ")
        .arg(SystemDiagnostics::countryFullName(result.physicalCountry))
        .arg(result.servers.size()).arg(GeoProbe::allServers().size())
        .arg(chosenName)
        .arg(chosenTtfb, 0, 'f', 0)
        + ((anyDlOk && anyUlOk)
            ? QStringLiteral("Speed test: download %1 Mbps, upload %2 Mbps.").arg(bestDlMbps, 0, 'f', 1).arg(bestUlMbps, 0, 'f', 1)
            : anyDlOk
                ? QStringLiteral("Speed test: download %1 Mbps, upload failed.").arg(bestDlMbps, 0, 'f', 1)
                : anyUlOk
                    ? QStringLiteral("Speed test: download failed, upload %1 Mbps.").arg(bestUlMbps, 0, 'f', 1)
                    : QStringLiteral("Speed test failed despite a reachable server."));
    const QVariantList connBase{
        SystemDiagnostics::countryFullName(result.physicalCountry),
        QString::number(result.servers.size()), QString::number(GeoProbe::allServers().size()),
        chosenName, QString::number(chosenTtfb, 'f', 0) };
    if (anyDlOk && anyUlOk) {
        r.data[QStringLiteral("narrativeKey")] = QStringLiteral("nInternetFull");
        r.data[QStringLiteral("narrativeArgs")] = connBase
            + QVariantList{ QString::number(bestDlMbps, 'f', 1), QString::number(bestUlMbps, 'f', 1) };
    } else if (anyDlOk) {
        r.data[QStringLiteral("narrativeKey")] = QStringLiteral("nInternetDlOnly");
        r.data[QStringLiteral("narrativeArgs")] = connBase + QVariantList{ QString::number(bestDlMbps, 'f', 1) };
    } else if (anyUlOk) {
        r.data[QStringLiteral("narrativeKey")] = QStringLiteral("nInternetUlOnly");
        r.data[QStringLiteral("narrativeArgs")] = connBase + QVariantList{ QString::number(bestUlMbps, 'f', 1) };
    } else {
        r.data[QStringLiteral("narrativeKey")] = QStringLiteral("nInternetSpeedFail");
        r.data[QStringLiteral("narrativeArgs")] = connBase;
    }
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
}
