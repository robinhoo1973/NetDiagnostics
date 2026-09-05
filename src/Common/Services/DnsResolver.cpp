// =============================================================================
// DnsResolver.cpp — Shared DNS resolution with timeout
// =============================================================================
#include "Common/Services/DnsResolver.h"
#include "Common/Services/MonotonicClock.h"   // H4: 单调毫秒基准（与 DiagnosticBase/AppState 同源）

#if defined(__APPLE__)
#include <dispatch/dispatch.h>
#endif

#include <atomic>
#include <thread>
#include <chrono>
#include <cstring>
#include <limits>
#include <QDebug>
#include <QHostAddress>
#include <QAbstractSocket>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

// ── 共用辅助（非 Apple std::thread 路径）──────────────────────────────
// H4 (5WHY): 时间基准直接用 monotonicMsSinceAppStart()（MonotonicClock.h，
// 与 DiagnosticBase/AppState 同源），不自造第二个时钟源或别名。
namespace {

bool waitResolveDone(const std::atomic<bool>& done,
                     std::chrono::steady_clock::time_point start, int timeoutMs) {
    while (!done.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count() > timeoutMs)
            break;
    }
    return done.load(std::memory_order_acquire);
}

// 5WHY: 无条件 detach 会泄漏已完成线程的内核资源；join 已完成线程是即时的。
// 仅超时（getaddrinfo 可能仍阻塞 30-120s）时 detach，让 shared_ptr 保活。
void finishResolveThread(std::thread& t, const std::atomic<bool>& done) {
    if (done.load(std::memory_order_acquire))
        t.join();   // completed within timeout -- immediate cleanup
    else
        t.detach(); // still blocked in getaddrinfo; state freed by shared_ptr
}

// 5WHY (simplify 2026-08-17): resolve()/resolve6() 仅地址族不同——getaddrinfo
// 工作 lambda 与 EAGAIN 守卫重复两份。统一状态结构 + 受守卫的线程创建；
// 失败返回非 joinable 线程，调用方按失败处理。
struct LookupState { std::atomic<bool> done{false}; QString ip; };

std::thread spawnLookupThread(const std::shared_ptr<LookupState>& st,
                              const QByteArray& hb, int family) {
    std::thread t;
    try {
        t = std::thread([st, hb, family]() {
            struct addrinfo hints = {}, *res = nullptr;
            hints.ai_family = family; hints.ai_socktype = SOCK_STREAM;
            if (getaddrinfo(hb.constData(), nullptr, &hints, &res) == 0 && res) {
                const bool v6 = (family == AF_INET6);
                char ip[INET6_ADDRSTRLEN];
                void* src = v6
                    ? static_cast<void*>(&(reinterpret_cast<struct sockaddr_in6*>(res->ai_addr)->sin6_addr))
                    : static_cast<void*>(&(reinterpret_cast<struct sockaddr_in*>(res->ai_addr)->sin_addr));
                inet_ntop(family, src, ip, sizeof(ip));
                st->ip = QString::fromLatin1(ip);
                freeaddrinfo(res);
            }
            st->done.store(true, std::memory_order_release);
        });
    } catch (const std::system_error& e) {
        qWarning("DnsResolver: thread creation failed (%s)", e.what());
    }
    return t;
}

// 5WHY (review round 4): 三处负缓存收尾逐字复制——TOCTOU 修复也必须三处同步。
// 单一收尾：等待 → 收线程 → 单次 acquire 载 done。收线程后载入：
// 若 join 已发生则 done 必真；release/acquire 保证看到 done==true 时
// st->ip 的写入已发生，读取安全。
bool finishLookup(const std::shared_ptr<LookupState>& st, std::thread& t,
                  std::chrono::steady_clock::time_point start, int timeoutMs) {
    waitResolveDone(st->done, start, timeoutMs);
    finishResolveThread(t, st->done);
    return st->done.load(std::memory_order_acquire);
}

} // namespace

// 5WHY (2026-08-22 CP-2): 驱逐最旧条目（ts 最小）——近似 LRU。仅 DnsResolver
// 内部（持锁上下文）调用，条目量≤512 线性扫描开销可忽略。
void DnsResolver::pruneCache(QHash<QString, DnsEntry>& cache) {
    while (cache.size() > kCacheMaxEntries) {
        QString oldestKey;
        qint64 oldestTs = std::numeric_limits<qint64>::max();
        for (auto it = cache.cbegin(); it != cache.cend(); ++it) {
            if (it.value().ts < oldestTs) {
                oldestTs = it.value().ts;
                oldestKey = it.key();
            }
        }
        cache.remove(oldestKey);
    }
}

// IP 字面量 → sockaddr_storage（resolvePtr 反查用；返回 salen，0=无法解析）
int fillSockaddr(const QByteArray& ipb, struct sockaddr_storage& sa) {
    const QHostAddress addr(QString::fromUtf8(ipb));
    if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
        auto* s4 = reinterpret_cast<struct sockaddr_in*>(&sa);
        s4->sin_family = AF_INET;
        s4->sin_addr.s_addr = htonl(addr.toIPv4Address());
        return static_cast<int>(sizeof(*s4));
    }
    if (addr.protocol() == QAbstractSocket::IPv6Protocol) {
        auto* s6 = reinterpret_cast<struct sockaddr_in6*>(&sa);
        s6->sin6_family = AF_INET6;
        const Q_IPV6ADDR v6 = addr.toIPv6Address();
        std::memcpy(&s6->sin6_addr, &v6, sizeof(v6));
        return static_cast<int>(sizeof(*s6));
    }
    return 0;
}

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
            // 负缓存 TTL 存于条目（插入点语义），读侧统一 now-ts < ttlMs
            if (monotonicMsSinceAppStart() - it->ts < it->ttlMs) return {};
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
        m_cache[host] = {ipOut, monotonicMsSinceAppStart(),
                         ipOut.isEmpty() ? DnsResolver::kNegativeTtlMs : 0};
        pruneCache(m_cache);
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
    // 5WHY (simplify 2026-08-17): 工作 lambda + EAGAIN 守卫统一在
    // spawnLookupThread（与 resolve6 仅地址族不同）；失败返回非 joinable 线程。
    auto st = std::make_shared<LookupState>();
    std::thread t = spawnLookupThread(st, hb, AF_INET);
    if (!t.joinable()) {
        // 5WHY (verify 2026-08-17): 原修复把 EAGAIN（本地瞬时资源问题）按
        // DNS 失败写 30s 负缓存——健康主机被黑名单化整轮、与真实故障不可
        // 区分。改短窗口节流：条目自带 kSpawnFailTtlMs（压力缓解后重查
        // 成功），同时避免资源耗尽期的线程创建风暴。
        QMutexLocker locker(&m_mutex);
        m_cache[host] = {QString(), monotonicMsSinceAppStart(),
                         DnsResolver::kSpawnFailTtlMs};
        pruneCache(m_cache);
        return {};
    }
    // 5WHY: t.detach() was unconditional -- even when the thread completed
    // within the timeout (st->done==true), we detached instead of joining.
    // A joinable thread that finishes normally consumes kernel resources
    // (TID, stack) until explicitly joined or detached.  Joining a completed
    // thread is immediate (no blocking), so prefer join when possible.
    // Detach is only needed for the timeout case where getaddrinfo may still
    // be blocked for 30-120s -- joining would block the caller.
    // 5WHY (review round 3): 负缓存必须写空串——超时后 detach 的线程可能仍在
    // 写 st->ip（非原子 QString 并发读写 UB）。review round 4：done 在收线程
    // 后单次载入（旧快照存在 TOCTOU——线程在快照后完成会被 join 却仍写负缓存）。
    // 5WHY (2026-09-05 快速失败不落负缓存): 曾 `done ? 0 : kNegativeTtlMs`——
    // done 只表示"线程在超时内结束"，最常见的失败形态（NXDOMAIN/SERVFAIL
    // 快速返回）done==true 且 ip 空 → ttlMs=0 → 负缓存永不命中，坏主机每次
    // 查询都重新 spawn 线程。负缓存条件 = 无有效 IP 结果（含 done-with-empty）。
    const bool done = finishLookup(st, t, std::chrono::steady_clock::now(), timeoutMs);
    const bool resolved = done && !st->ip.isEmpty();
    {
        QMutexLocker locker(&m_mutex);
        m_cache[host] = {resolved ? st->ip : QString(), monotonicMsSinceAppStart(),
                         resolved ? 0 : DnsResolver::kNegativeTtlMs};
        pruneCache(m_cache);
    }
    return resolved ? st->ip : QString();
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
            if (monotonicMsSinceAppStart() - it->ts < it->ttlMs) return {};
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
        m_cache[QStringLiteral("v6:") + host] = {ipOut, monotonicMsSinceAppStart(),
                                                 ipOut.isEmpty() ? DnsResolver::kNegativeTtlMs : 0};
        pruneCache(m_cache);
    }
    if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        dispatch_release(ctx->sem);
        delete ctx;
    }
    return ipOut;
#else
    // Non-Apple: std::thread with polling loop（工作体统一在 spawnLookupThread）。
    auto st = std::make_shared<LookupState>();
    QByteArray hb = host.toUtf8();
    std::thread t = spawnLookupThread(st, hb, AF_INET6);
    if (!t.joinable()) {
        // 同 resolve()：EAGAIN 是本地瞬时问题，条目自带 kSpawnFailTtlMs 节流
        QMutexLocker locker(&m_mutex);
        m_cache[QStringLiteral("v6:") + host] = {QString(), monotonicMsSinceAppStart(),
                                                 DnsResolver::kSpawnFailTtlMs};
        pruneCache(m_cache);
        return {};
    }
    // 5WHY: prefer join() when thread completed within timeout for immediate
    // cleanup; detach() is only needed for timeout case where getaddrinfo may
    // still be blocked for 30-120s.
    // 5WHY (2026-09-05): 与 resolve() 同修——done 且 ip 空（快速 NXDOMAIN）
    // 必须落负缓存，否则坏主机每次查询都重新 spawn 线程。
    const bool done = finishLookup(st, t, std::chrono::steady_clock::now(), timeoutMs);
    const bool resolved = done && !st->ip.isEmpty();
    {
        QMutexLocker l(&m_mutex);
        m_cache[QStringLiteral("v6:") + host] = {resolved ? st->ip : QString(),
                                                 monotonicMsSinceAppStart(),
                                                 resolved ? 0 : DnsResolver::kNegativeTtlMs};
        pruneCache(m_cache);
    }
    return resolved ? st->ip : QString();
#endif
}

QString DnsResolver::resolvePtr(const QString& ip, int timeoutMs) {
    // 反查 PTR（IP → 主机名；G4 traceroute 展示用）。与 resolve() 同机制：
    // Apple 走 GCD（内核级超时），其余走受守卫 std::thread + 轮询。
    // 5WHY (verify 2026-08-17): 只写正缓存——空结果分不出超时与 NXDOMAIN，
    // 展示性 0.5s 截止的负缓存会把慢但可解的 PTR 毒化 30s（run 1 超时 →
    // run 2 命中负条目，主机名被连续多轮吞掉）。PTR 频率低（每跳一次），
    // 不写负缓存只放弃节流、不损正确性。
    // 5WHY (simplify 2026-08-17): 原调用方同步无界 QHostInfo::fromName（违反
    // 本文件 H4 规则），且每跳新建局部事件循环（第 3 份有界等待机制）。
    {
        QMutexLocker locker(&m_mutex);
        const QString k = QStringLiteral("ptr:") + ip;
        auto it = m_cache.constFind(k);
        if (it != m_cache.constEnd()) {
            if (!it->ip.isEmpty()) return it->ip;
            if (monotonicMsSinceAppStart() - it->ts < it->ttlMs) return {};
        }
    }

#if defined(__APPLE__)
    struct PtrCtx {
        QByteArray ip;
        char name[NI_MAXHOST];
        std::atomic<bool> resolved;
        dispatch_semaphore_t sem;
        std::atomic<int> refs;
    };
    auto* ctx = new PtrCtx();
    ctx->ip = ip.toUtf8();
    ctx->name[0] = '\0';
    ctx->resolved.store(false, std::memory_order_relaxed);
    ctx->sem = dispatch_semaphore_create(0);
    ctx->refs.store(2, std::memory_order_relaxed);

    dispatch_async_f(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ctx,
        [](void* p) {
            auto* c = static_cast<PtrCtx*>(p);
            struct sockaddr_storage sa = {};
            const int salen = fillSockaddr(c->ip, sa);
            if (salen && getnameinfo(reinterpret_cast<struct sockaddr*>(&sa), salen,
                                     c->name, sizeof(c->name), nullptr, 0, NI_NAMEREQD) == 0)
                c->resolved.store(true, std::memory_order_release);
            dispatch_semaphore_signal(c->sem);
            if (c->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                dispatch_release(c->sem);
                delete c;
            }
        });

    long waitResult = dispatch_semaphore_wait(ctx->sem,
        dispatch_time(DISPATCH_TIME_NOW, (int64_t)timeoutMs * NSEC_PER_MSEC));

    QString nameOut;
    if (waitResult == 0 && ctx->resolved.load(std::memory_order_acquire))
        nameOut = QString::fromLatin1(ctx->name);
    // PTR 只写正缓存（见函数头注释）：空结果不落表，慢 PTR 不被毒化
    if (!nameOut.isEmpty()) {
        QMutexLocker locker(&m_mutex);
        m_cache[QStringLiteral("ptr:") + ip] = {nameOut, monotonicMsSinceAppStart()};
        pruneCache(m_cache);
    }
    if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        dispatch_release(ctx->sem);
        delete ctx;
    }
    return nameOut;
#else
    auto st = std::make_shared<LookupState>();
    const QByteArray ipb = ip.toUtf8();
    std::thread t;
    try {
        t = std::thread([st, ipb]() {
            struct sockaddr_storage sa = {};
            const int salen = fillSockaddr(ipb, sa);
            char host[NI_MAXHOST] = {};
            if (salen && getnameinfo(reinterpret_cast<struct sockaddr*>(&sa), salen,
                                     host, sizeof(host), nullptr, 0, NI_NAMEREQD) == 0)
                st->ip = QString::fromLatin1(host);
            st->done.store(true, std::memory_order_release);
        });
    } catch (const std::system_error& e) {
        qWarning("DnsResolver: thread creation failed (%s)", e.what());
        // PTR 不写负缓存（见函数头注释）——EAGAIN 直接返回，下轮自然重试
        return {};
    }
    const bool done = finishLookup(st, t, std::chrono::steady_clock::now(), timeoutMs);
    if (done && !st->ip.isEmpty()) {
        QMutexLocker l(&m_mutex);
        m_cache[QStringLiteral("ptr:") + ip] = {st->ip, monotonicMsSinceAppStart()};
        pruneCache(m_cache);
    }
    return done ? st->ip : QString();
#endif
}
