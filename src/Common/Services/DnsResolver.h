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
    // from a real outage. Throttle instead: the entry carries this short TTL
    // (re-resolve retries once resource pressure eases, without a spawn storm
    // during exhaustion).
    static constexpr qint64 kSpawnFailTtlMs = 3'000;

    struct DnsEntry {
        QString ip;      // resolved IP; empty = failed lookup (negative entry)
        // H4 (5WHY): 原用 QDateTime::currentMSecsSinceEpoch()（墙钟）——
        // NTP 步进或用户手动校时导致缓存过早/过晚过期。改用进程启动单调
        // 毫秒（MonotonicClock.h，与 DiagnosticBase/AppState 同源），
        // 免疫系统时钟调整。
        qint64  ts = 0;     // cached-at time (ms since app start, MonotonicClock.h)
        // 5WHY (simplify 2026-09-04): TTL 存于条目而非读侧常量——负缓存
        // 按插入点语义各带过期时长（常规失败 kNegativeTtlMs / EAGAIN 节流
        // kSpawnFailTtlMs），读侧统一 now-ts < ttlMs；回拨时间戳算术与
        // "kNegativeTtlMs > kSpawnFailTtlMs"隐含不变量随之消失。正缓存
        // 条目不按 TTL 过期（仅 LRU 驱逐），ttlMs 保持 0。
        qint64  ttlMs = 0; // negative-entry TTL; 0 = positive (LRU-only expiry)
    };
    // 5WHY (2026-08-22 CP-2): 缓存无上限——多轮 run × 多键型（A/AAAA/PTR/
    // 负缓存）在长进程内单调增长。LRU 容量上限，超限驱逐最旧条目。
    static constexpr int kCacheMaxEntries = 512;
    // 5WHY (2026-08-22 CP-2): 驱逐最旧条目（ts 最小）——近似 LRU；
    // 条目量≤512 线性扫描开销可忽略，仅供本类内部调用。
    static void pruneCache(QHash<QString, DnsEntry>& cache);
    QHash<QString, DnsEntry> m_cache;
    QMutex m_mutex;
};
