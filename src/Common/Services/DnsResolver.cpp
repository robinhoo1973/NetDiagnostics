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

    // Check cache
    {
        QMutexLocker locker(&m_mutex);
        if (m_cache.contains(host))
            return m_cache[host];
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
        if (!ipOut.isEmpty()) {
            QMutexLocker locker(&m_mutex);
            m_cache[host] = ipOut;
        }
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
    t.detach(); // thread keeps shared_ptr alive, auto-frees when done
    if (!st->done.load(std::memory_order_acquire))
        return {}; // timeout: thread still owns st via shared_ptr, safe
    {
        QMutexLocker locker(&m_mutex);
        if (!st->ip.isEmpty())
            m_cache[host] = st->ip;
    }
    return st->ip;
#endif
}
