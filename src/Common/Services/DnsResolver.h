// =============================================================================
// DnsResolver.h — Shared DNS resolution with timeout (singleton)
//
// Eliminates the duplicated resolveWithTimeout / resolveHostWithTimeout across
// G1G2G3Native.cpp, G4RemoteHost.cpp, and NetworkProbe.cpp.
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
    // Uses QHostInfo first, then falls back to resolve() with timeout.
    static quint32 resolveIPv4(const QString& host, int timeoutMs = 3000);

private:
    DnsResolver() = default;
    ~DnsResolver() = default;
    DnsResolver(const DnsResolver&) = delete;
    DnsResolver& operator=(const DnsResolver&) = delete;

    QHash<QString, QString> m_cache;
    QMutex m_mutex;
};
