// =============================================================================
// DnsResolver.cpp — Shared DNS resolution with timeout
// =============================================================================
#include "util/DnsResolver.h"

#ifdef __APPLE__
#include <dispatch/dispatch.h>
#endif

#include <atomic>
#include <thread>
#include <chrono>

#ifdef _WIN32
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

#ifdef __APPLE__
    // Apple: use GCD dispatch_semaphore for true kernel-level timeout.
    // std::thread detach on iOS leaks threads; getaddrinfo can block 30-120s.
    struct DnsCtx {
        QByteArray host; char ip[INET_ADDRSTRLEN]; bool resolved; dispatch_semaphore_t sem;
    };
    auto* ctx = new DnsCtx{host.toUtf8(), {}, false, dispatch_semaphore_create(0)};
    dispatch_async_f(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ctx,
        [](void* p) {
            auto* c = (DnsCtx*)p;
            struct addrinfo hints = {}, *res;
            hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
            if (getaddrinfo(c->host.constData(), nullptr, &hints, &res) == 0) {
                auto* sa = (struct sockaddr_in*)res->ai_addr;
                inet_ntop(AF_INET, &sa->sin_addr, c->ip, sizeof(c->ip));
                c->resolved = true;
                freeaddrinfo(res);
            }
            dispatch_semaphore_signal(c->sem);
        });
    long waitResult = dispatch_semaphore_wait(ctx->sem,
        dispatch_time(DISPATCH_TIME_NOW, (int64_t)timeoutMs * NSEC_PER_MSEC));
    if (waitResult != 0) {
        dispatch_release(ctx->sem);
        return {}; // timeout — GCD task continues, ctx leaked (~200 bytes)
    }
    dispatch_release(ctx->sem);
    if (ctx->resolved) {
        QString ip = QString::fromLatin1(ctx->ip);
        QMutexLocker locker(&m_mutex);
        m_cache[host] = ip;
        delete ctx;
        return ip;
    }
    delete ctx;
    return {};
#else
    // Non-Apple: std::thread with polling loop
    struct ResolveState { std::atomic<bool> done{false}; QString ip; };
    ResolveState st;
    std::thread t([&st, hb]() {
        struct addrinfo hints = {}, *res;
        hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(hb.constData(), nullptr, &hints, &res) == 0) {
            char ip[INET_ADDRSTRLEN];
            auto* sa = (struct sockaddr_in*)res->ai_addr;
            inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
            st.ip = QString::fromLatin1(ip);
            freeaddrinfo(res);
        }
        st.done.store(true, std::memory_order_release);
    });
    auto start = std::chrono::steady_clock::now();
    while (!st.done.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeoutMs) break;
    }
    t.detach();
    if (!st.done.load(std::memory_order_acquire))
        return {};
    {
        QMutexLocker locker(&m_mutex);
        if (!st.ip.isEmpty())
            m_cache[host] = st.ip;
    }
    return st.ip;
#endif
}
