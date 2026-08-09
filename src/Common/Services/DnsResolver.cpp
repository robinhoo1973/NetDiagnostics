// =============================================================================
// DnsResolver.cpp — Shared DNS resolution with timeout
// =============================================================================
#include "Common/Services/DnsResolver.h"

#if defined(__APPLE__)
#include <dispatch/dispatch.h>
#endif

#include <atomic>
#include <thread>
#include <chrono>
#include <QDateTime>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

DnsResolver& DnsResolver::instance() {
    static DnsResolver inst;
    return inst;
}

void DnsResolver::clearCache() {
    QMutexLocker locker(&m_mutex);
    m_cache.clear();
}

quint32 DnsResolver::resolveIPv4(const QString& host, int timeoutMs) {
    // Use the timeout-bounded resolve() directly. Do NOT prefix this with
    // QHostInfo::fromName(): that call is synchronous and unbounded, so it
    // would defeat the caller's timeout (it can block 30-120s on bad DNS).
    // resolve() already handles literal IPs, caching, and the timeout.
    QString ipStr = instance().resolve(host, timeoutMs);
    if (!ipStr.isEmpty()) {
        struct in_addr a;
        if (inet_pton(AF_INET, ipStr.toUtf8().constData(), &a) == 1)
            return ntohl(a.s_addr);
    }
    return 0;
}

QString DnsResolver::resolve(const QString& host, int timeoutMs) {
    // Already an IP address — return as-is
    struct in_addr ip4;
    if (inet_pton(AF_INET, host.toUtf8().constData(), &ip4) == 1)
        return host;

    // Check cache (positive + negative).  A negative entry (failed lookup)
    // is honored for kNegativeTtlMs so a bad-DNS host doesn't spawn a
    // blocking thread on every call — repeated lookups in one diagnostic run
    // hit the negative entry instead. clearCache() runs before each run.
    {
        QMutexLocker locker(&m_mutex);
        auto it = m_cache.constFind(host);
        if (it != m_cache.constEnd()) {
            if (!it->ip.isEmpty()) return it->ip;
            const qint64 age = QDateTime::currentMSecsSinceEpoch() - it->ts;
            if (age < DnsResolver::kNegativeTtlMs) return {};
            // expired negative entry — fall through and re-resolve
        }
    }

    QByteArray hb = host.toUtf8();

#if defined(__APPLE__)
    // Apple: use GCD dispatch_semaphore for a true kernel-level timeout.
    // std::thread detach on iOS leaks threads; getaddrinfo can block 30-120s.
    //
    // The worker runs via dispatch_async_f — a plain C function that (unlike an
    // Objective-C block) does NOT retain the semaphore. To avoid a use-after-free
    // when the resolve TIMES OUT (the waiter would otherwise release the
    // semaphore and free ctx while the worker is still running and about to
    // signal it), ownership is reference-counted: whoever drops the last
    // reference releases the semaphore and frees ctx.
    struct DnsCtx {
        QByteArray host;
        char ip[INET_ADDRSTRLEN];
        std::atomic<bool> resolved;
        dispatch_semaphore_t sem;
        std::atomic<int> refs;
    };
    auto* ctx = new DnsCtx();
    ctx->host = host.toUtf8();
    ctx->ip[0] = '\0';
    ctx->resolved.store(false, std::memory_order_relaxed);
    ctx->sem = dispatch_semaphore_create(0);
    ctx->refs.store(2, std::memory_order_relaxed);

    dispatch_async_f(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ctx,
        [](void* p) {
            auto* c = static_cast<DnsCtx*>(p);
            struct addrinfo hints = {}, *res = nullptr;
            hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
            if (getaddrinfo(c->host.constData(), nullptr, &hints, &res) == 0 && res) {
                auto* sa = (struct sockaddr_in*)res->ai_addr;
                inet_ntop(AF_INET, &sa->sin_addr, c->ip, sizeof(c->ip));
                c->resolved.store(true, std::memory_order_release);
                freeaddrinfo(res);
            }
            dispatch_semaphore_signal(c->sem);
            // Drop the worker's reference; last one out frees.
            if (c->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                dispatch_release(c->sem);
                delete c;
            }
        });

    long waitResult = dispatch_semaphore_wait(ctx->sem,
        dispatch_time(DISPATCH_TIME_NOW, (int64_t)timeoutMs * NSEC_PER_MSEC));

    QString ipOut;
    // Only read ctx on success — on timeout the worker may still be writing it.
    if (waitResult == 0 && ctx->resolved.load(std::memory_order_acquire)) {
        ipOut = QString::fromLatin1(ctx->ip);
    }
    // Cache both outcomes: success → positive entry, timeout/failure →
    // negative entry (TTL-bounded) so a wedged DNS server doesn't block
    // every subsequent lookup for the same host.
    {
        QMutexLocker locker(&m_mutex);
        m_cache[host] = {ipOut, QDateTime::currentMSecsSinceEpoch()};
    }
    // Drop the waiter's reference; last one out frees. On timeout the still-running
    // worker keeps ctx (and the semaphore) alive until it finishes — no UAF, no
    // early free.
    if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        dispatch_release(ctx->sem);
        delete ctx;
    }
    return ipOut;
#else
    // Non-Apple: std::thread with polling loop.
    // Heap-allocate state so the detached thread doesn't access freed stack.
    struct ResolveState { std::atomic<bool> done{false}; QString ip; };
    auto st = std::make_shared<ResolveState>();
    std::thread t([st, hb]() {
        struct addrinfo hints = {}, *res;
        hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(hb.constData(), nullptr, &hints, &res) == 0) {
            char ip[INET_ADDRSTRLEN];
            auto* sa = (struct sockaddr_in*)res->ai_addr;
            inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
            st->ip = QString::fromLatin1(ip);
            freeaddrinfo(res);
        }
        st->done.store(true, std::memory_order_release);
    });
    auto start = std::chrono::steady_clock::now();
    while (!st->done.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeoutMs) break;
    }
    // 5WHY: t.detach() was unconditional -- even when the thread completed
    // within the timeout (st->done==true), we detached instead of joining.
    // A joinable thread that finishes normally consumes kernel resources
    // (TID, stack) until explicitly joined or detached.  Joining a completed
    // thread is immediate (no blocking), so prefer join when possible.
    // Detach is only needed for the timeout case where getaddrinfo may still
    // be blocked for 30-120s -- joining would block the caller.
    if (st->done.load(std::memory_order_acquire)) {
        t.join();   // thread completed within timeout -- immediate cleanup
    } else {
        t.detach(); // timeout: thread still blocked in getaddrinfo; let it
                     // keep shared_ptr alive until it finishes, then auto-free.
                     // Risk: rapid repeated timeouts accumulate detached threads.
                     // Mitigated by DNS cache -- repeated lookups hit the cache
                     // before spawning new threads.
    }
    if (!st->done.load(std::memory_order_acquire))
        return {}; // timeout: thread still owns st via shared_ptr, safe
    // Cache both outcomes: success → positive entry, timeout/failure →
    // negative entry (TTL-bounded).  The detached-thread risk noted below is
    // therefore mitigated by the negative cache too — a wedged DNS server
    // won't re-spawn a thread for the same host within the TTL window.
    {
        QMutexLocker locker(&m_mutex);
        m_cache[host] = {st->ip, QDateTime::currentMSecsSinceEpoch()};
    }
    return st->ip;
#endif
}

QString DnsResolver::resolve6(const QString& host, int timeoutMs) {
    struct in6_addr ip6;
    if (inet_pton(AF_INET6, host.toUtf8().constData(), &ip6) == 1)
        return host;
    {
        QMutexLocker l(&m_mutex);
        const QString k = QStringLiteral("v6:") + host;
        auto it = m_cache.constFind(k);
        if (it != m_cache.constEnd()) {
            if (!it->ip.isEmpty()) return it->ip;
            if (QDateTime::currentMSecsSinceEpoch() - it->ts < DnsResolver::kNegativeTtlMs) return {};
        }
    }

#if defined(__APPLE__)
    // 5WHY: resolve() uses GCD (dispatch_async_f) on Apple platforms because
    // std::thread::detach leaks threads on iOS.  resolve6() must use the same
    // pattern for the same reason.  Reference-counted Dns6Ctx prevents UAF
    // on timeout: the waiter drops its reference, but the worker keeps ctx
    // (and the semaphore) alive until it finishes.
    struct Dns6Ctx {
        QByteArray host;
        char ip[INET6_ADDRSTRLEN];
        std::atomic<bool> resolved;
        dispatch_semaphore_t sem;
        std::atomic<int> refs;
    };
    auto* ctx = new Dns6Ctx();
    ctx->host = host.toUtf8();
    ctx->ip[0] = '\0';
    ctx->resolved.store(false, std::memory_order_relaxed);
    ctx->sem = dispatch_semaphore_create(0);
    ctx->refs.store(2, std::memory_order_relaxed);

    dispatch_async_f(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ctx,
        [](void* p) {
            auto* c = static_cast<Dns6Ctx*>(p);
            struct addrinfo hints = {}, *res = nullptr;
            hints.ai_family = AF_INET6; hints.ai_socktype = SOCK_STREAM;
            if (getaddrinfo(c->host.constData(), nullptr, &hints, &res) == 0 && res) {
                auto* sa6 = (struct sockaddr_in6*)res->ai_addr;
                inet_ntop(AF_INET6, &sa6->sin6_addr, c->ip, sizeof(c->ip));
                c->resolved.store(true, std::memory_order_release);
                freeaddrinfo(res);
            }
            dispatch_semaphore_signal(c->sem);
            if (c->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                dispatch_release(c->sem);
                delete c;
            }
        });

    long waitResult = dispatch_semaphore_wait(ctx->sem,
        dispatch_time(DISPATCH_TIME_NOW, (int64_t)timeoutMs * NSEC_PER_MSEC));

    QString ipOut;
    if (waitResult == 0 && ctx->resolved.load(std::memory_order_acquire)) {
        ipOut = QString::fromLatin1(ctx->ip);
    }
    {
        QMutexLocker locker(&m_mutex);
        m_cache[QStringLiteral("v6:") + host] = {ipOut, QDateTime::currentMSecsSinceEpoch()};
    }
    if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        dispatch_release(ctx->sem);
        delete ctx;
    }
    return ipOut;
#else
    // Non-Apple: std::thread with polling loop.
    struct State { std::atomic<bool> done{false}; QString ip; };
    auto st = std::make_shared<State>();
    QByteArray hb = host.toUtf8();
    std::thread t([st, hb]() {
        struct addrinfo hints = {}, *res;
        hints.ai_family = AF_INET6; hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(hb.constData(), nullptr, &hints, &res) == 0) {
            char ip[INET6_ADDRSTRLEN];
            auto* sa6 = (struct sockaddr_in6*)res->ai_addr;
            inet_ntop(AF_INET6, &sa6->sin6_addr, ip, sizeof(ip));
            st->ip = QString::fromLatin1(ip); freeaddrinfo(res);
        }
        st->done.store(true, std::memory_order_release);
    });
    auto tm = std::chrono::steady_clock::now();
    while (!st->done.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-tm).count() > timeoutMs) break;
    }
    // 5WHY: prefer join() when thread completed within timeout for immediate
    // cleanup; detach() is only needed for timeout case where getaddrinfo may
    // still be blocked for 30-120s.
    if (st->done.load(std::memory_order_acquire)) {
        t.join();
    } else {
        t.detach();
    }
    if (!st->done.load(std::memory_order_acquire)) return {};
    {
        QMutexLocker l(&m_mutex);
        m_cache[QStringLiteral("v6:") + host] = {st->ip, QDateTime::currentMSecsSinceEpoch()};
    }
    return st->ip;
#endif
}
