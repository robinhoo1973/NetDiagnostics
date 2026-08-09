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

private:
    DnsResolver() = default;
    ~DnsResolver() = default;
    DnsResolver(const DnsResolver&) = delete;
    DnsResolver& operator=(const DnsResolver&) = delete;

    // Negative TTL: a failed/timeout lookup is remembered for this long so a
    // bad-DNS host doesn't spawn a blocking thread on every call.
    static constexpr qint64 kNegativeTtlMs = 30'000;

    struct DnsEntry {
        QString ip;      // resolved IP; empty = failed lookup
        qint64  ts = 0;  // cached-at time (msecs since epoch)
    };
    QHash<QString, DnsEntry> m_cache;
    QMutex m_mutex;
};
