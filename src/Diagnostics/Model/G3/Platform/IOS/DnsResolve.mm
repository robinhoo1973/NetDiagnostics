// =============================================================================
// IosDnsTask.mm �� iOS DNS resolution via CFHost (async, cancellable, native)
//
// CFHost provides proper iOS DNS resolution with system cache and timeout
// control. Available since iOS 2.0. Replaces generic getaddrinfo+GCD.
// =============================================================================

#if defined(PLATFORM_IOS)

#include "Common/Services/DiagnosticTask.h"
#include "Diagnostics/Model/G4/G4RemoteHost.h"
#include "Diagnostics/Model/G3/Platform/IOS/DnsResolve.h" // 5WHY: own header for declaration checking
#include "Diagnostics/View/DiagnosticFormatter.h"
#include <QElapsedTimer>
#include <atomic>
#include <memory>
#import <Foundation/Foundation.h>
#import <CFNetwork/CFNetwork.h>
#import <sys/socket.h>
#import <netinet/in.h>
#import <arpa/inet.h>

static QString resolveCFHost(NSString* hostname, int timeoutMs) {
    CFHostRef host = CFHostCreateWithName(kCFAllocatorDefault, (__bridge CFStringRef)hostname);
    if (!host) return QString();
    CFStreamError err;
    Boolean ok = CFHostStartInfoResolution(host, kCFHostAddresses, &err);
    if (!ok) { CFRelease(host); return QString(); }

    // Reference-counted semaphore ownership: both waiter and worker hold a ref (2 total).
    // Whoever finishes last (waiter on timeout, or worker on success) releases the semaphore.
    //
    // CRITICAL: the resolved value is stored as a C++ QString, NEVER as an autoreleased
    // NSString. An autoreleased NSString created inside the GCD block is owned by that
    // block's autorelease pool and is freed when the block returns; reading it from the
    // waiter thread afterwards is a use-after-free that crashes in objc_msgSend. We
    // convert to QString *inside* the block (while the NSString is still valid) so no
    // Objective-C object ever crosses the thread boundary.
    struct dnsCtx {
        dispatch_semaphore_t sem;
        CFHostRef host;
        QString result;
        std::atomic<int> refs;
    };
    auto ctx = std::make_shared<dnsCtx>();
    ctx->sem = dispatch_semaphore_create(0);
    ctx->host = host;
    CFRetain(ctx->host);  // for the block's reference
    ctx->refs.store(2, std::memory_order_relaxed);  // waiter + worker

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        @autoreleasepool {
            Boolean resolved = false;
            CFArrayRef addrs = CFHostGetAddressing(ctx->host, &resolved);
            if (resolved && addrs && CFArrayGetCount(addrs) > 0) {
                for (CFIndex i = 0; i < CFArrayGetCount(addrs); i++) {
                    CFDataRef Data = (CFDataRef)CFArrayGetValueAtIndex(addrs, i);
                    if (!Data) continue;
                    struct sockaddr_in* sa = (struct sockaddr_in*)CFDataGetBytePtr(Data);
                    if (sa->sin_family == AF_INET) {
                        char ip[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
                        // Convert to QString HERE �� no Objective-C object escapes the block.
                        ctx->result = QString::fromLatin1(ip);
                        break;
                    }
                }
            }
            CFRelease(ctx->host);
            dispatch_semaphore_signal(ctx->sem);
            // drop the worker's reference; last one out releases the semaphore.
            if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                dispatch_release(ctx->sem);
            }
        }
    });

    long waited = dispatch_semaphore_wait(ctx->sem, dispatch_time(DISPATCH_TIME_NOW,
        (int64_t)timeoutMs * NSEC_PER_MSEC));
    CFRelease(host);
    QString result;
    // Only read result on success; on timeout the worker may still be writing it.
    if (waited == 0) {
        result = ctx->result;
    }
    // drop the waiter's reference; last one out releases the semaphore.
    if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        dispatch_release(ctx->sem);
    }
    return result;
}

// 5WHY: same LTO dead-strip risk as iosDhcpDiag �� this symbol is only
// referenced through a lambda in TaskFactory.cpp.
DiagnosticResult __attribute__((used)) iosDnsResolve(DiagId id, const QString& target, int timeoutMs) {
    DiagnosticResult r;
    r.id = id; r.group = diagGroup(id);  // 5WHY: DiagGroup(id) was a C-style cast
                                         // producing out-of-range enum; use the
                                         // canonical diagGroup() mapper instead.
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();

    QString host = G4RemoteHost::extractHostname(target);
    QString ip;
    // This runs on a QtConcurrent worker thread which has NO autorelease pool of its
    // own. Any Foundation call here (host.toNSString(), CFHost bridging) creates
    // autoreleased objects; without a pool they leak and Foundation logs warnings.
    // Wrap the Cocoa-touching work in an explicit @autoreleasepool as Apple requires
    // for secondary threads.
    @autoreleasepool {
        NSString* nsHost = host.toNSString();
        ip = resolveCFHost(nsHost, timeoutMs);
    }
    qint64 elapsed = t.elapsed();
    r.durationMs = elapsed;

    // dig-style output via shared DiagnosticFormatter
    QStringList out;
    out << DiagnosticFormatter::formatDnsHeader(host,
        !ip.isEmpty() ? "NOERROR" : "SERVFAIL",
        (uint16_t)(qHash(host) & 0xFFFF), !ip.isEmpty() ? 1 : 0);
    out.append(QStringLiteral(";; QUESTION SECTION:"));
    out.append(DiagnosticFormatter::formatDnsQuestion(host));
    out.append(QString());
    if (!ip.isEmpty()) {
        out.append(QStringLiteral(";; ANSWER SECTION:"));
        out.append(DiagnosticFormatter::formatDnsRecord(host, 0, "A", ip));
        out.append(QString());
        r.status = DiagStatus::Pass;
        r.summary = QStringLiteral("Resolved: %1").arg(ip);
    } else {
        out.append(QStringLiteral(";; ANSWER SECTION: (empty)"));
        out.append(QString());
        r.status = DiagStatus::Fail;
        r.summary = QStringLiteral("DNS resolution failed for %1").arg(host);
    }
    out << DiagnosticFormatter::formatDnsFooter(elapsed, "system resolver (CFHost)");
    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    return r;
}

#endif // PLATFORM_IOS
