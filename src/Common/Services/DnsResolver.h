// =============================================================================
// DnsResolver.h — Shared DNS resolution with timeout (singleton)
//
// Eliminates the duplicated resolveWithTimeout / resolveHostWithTimeout across
// SystemDiagnostics.cpp, G4RemoteHost.cpp, and NetworkProbe.cpp.
//
// Apple: uses dispatch_semaphore + dispatch_async_f (GCD thread pool, reliable
//        kernel-level timeout, no thread leak).
// Other: uses std::thread + polling loop.
// =============================================================================
#pragma once

#include <QString>
#include <QHash>
#include <QMutex>

class DnsResolver {
public:
    static DnsResolver& instance();

    // Resolve hostname to IPv4 string. Returns empty string on timeout/failure.
    // Thread-safe (internal mutex protects cache).
    QString resolve(const QString& host, int timeoutMs = 3000);

    // Clear the DNS cache (useful between diagnostic runs).
    void clearCache();

    // Convenience: resolve hostname → IPv4 in host byte order, 0 on failure.
    static quint32 resolveIPv4(const QString& host, int timeoutMs = 3000);

    // Resolve hostname to IPv6 string. Returns empty string on timeout/failure.
    // Uses the same thread/timeout mechanism as resolve() but with AF_INET6.
    QString resolve6(const QString& host, int timeoutMs = 3000);

    // Reverse lookup: IP literal → hostname (PTR via getnameinfo, NI_NAMEREQD).
    // Returns empty string on timeout/failure.  Same bounded mechanism as
    // resolve() (GCD on Apple, guarded std::thread + polling elsewhere) with
    // the same positive/negative caching.
    // 5WHY (simplify 2026-08-17): G4 traceroute previously called the
    // synchronous unbounded QHostInfo::fromName per hop (30-120s hangs on
    // broken reverse DNS) — the file's own H4 rule mandates this singleton.
    QString resolvePtr(const QString& ip, int timeoutMs = 2000);

private:
    DnsResolver() = default;
    ~DnsResolver() = default;
    DnsResolver(const DnsResolver&) = delete;
    DnsResolver& operator=(const DnsResolver&) = delete;

    // Negative TTL: a failed/timeout lookup is remembered for this long so a
    // bad-DNS host doesn't spawn a blocking thread on every call.
    static constexpr qint64 kNegativeTtlMs = 30'000;
    // 5WHY (verify 2026-08-17): thread-creation failure (EAGAIN) is a LOCAL
    // transient resource problem, not a DNS failure — caching it for the full
    // 30s poisons a healthy host for the rest of the run, indistinguishable
    // from a real outage. Throttle instead: entries are backdated so they
    // expire after this short window (re-resolve retries once resource
    // pressure eases, without a spawn storm during exhaustion).
    static constexpr qint64 kSpawnFailTtlMs = 3'000;

    struct DnsEntry {
        QString ip;      // resolved IP; empty = failed lookup
        qint64  ts = 0;  // cached-at time (msecs since epoch)
    };
    QHash<QString, DnsEntry> m_cache;
    QMutex m_mutex;
};
