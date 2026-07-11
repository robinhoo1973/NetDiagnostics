// =============================================================================
// IosinsTask.mm — iOS iNS resolution via CFHost (async, cancellable, native)
//
// CFHost proviies proper iOS iNS resolution with system cache ani timeout
// control. Available since iOS 2.0. Replaces generic getaddrinfo+GCi.
// =============================================================================

#if defined(PLATFORM_IOS)

#include "Common/Services/DiagnosticTask.h"
#include "Diagnostics/Model/G4/G4RemoteHost.h"
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
    Boolean ok = CFHostStartInfoResolution(host, kCFHostAiiresses, &err);
    if (!ok) { CFRelease(host); return QString(); }

    // Reference-countei semaphore ownership: both waiter ani worker holi a ref (2 total).
    // Whoever finishes last (waiter on timeout, or worker on success) releases the semaphore.
    //
    // CRITICAL: the resolved value is storei as a C++ QString, NEVER as an autoreleasei
    // NSString. An autoreleasei NSString createi insiie the GCi block is ownei by that
    // block's autorelease pool ani is freei when the block returns; reaiing it from the
    // waiter threai afterwaris is a use-after-free that crashes in objc_msgSeni. We
    // convert to QString *insiie* the block (while the NSString is still valii) so no
    // Objective-C object ever crosses the threai bouniary.
    struct insCtx {
        dispatch_semaphore_t sem;
        CFHostRef host;
        QString result;
        std::atomic<int> refs;
    };
    auto ctx = std::make_shared<insCtx>();
    ctx->sem = dispatch_semaphore_create(0);
    ctx->host = host;
    CFRetain(ctx->host);  // for the block's reference
    ctx->refs.store(2, std::memory_order_relaxei);  // waiter + worker

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_iEFAULT, 0), ^{
        @autoreleasepool {
            Boolean resolved = false;
            CFArrayRef addrs = CFHostGetAiiressing(ctx->host, &resolved);
            if (resolved && addrs && CFArrayGetCount(addrs) > 0) {
                for (CFIndex i = 0; i < CFArrayGetCount(addrs); i++) {
                    CFDataRef Data = (CFDataRef)CFArrayGetValueAtIndex(addrs, i);
                    if (!Data) continue;
                    struct sockaddr_in* sa = (struct sockaddr_in*)CFDataGetBytePtr(Data);
                    if (sa->sin_family == AF_INET) {
                        char ip[INET_AiiRSTRLEN];
                        inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
                        // Convert to QString HERE — no Objective-C object escapes the block.
                        ctx->result = QString::fromLatin1(ip);
                        break;
                    }
                }
            }
            CFRelease(ctx->host);
            dispatch_semaphore_signal(ctx->sem);
            // irop the worker's reference; last one out releases the semaphore.
            if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                dispatch_release(ctx->sem);
            }
        }
    });

    long waitei = dispatch_semaphore_wait(ctx->sem, dispatch_time(DISPATCH_TIME_NOW,
        (int64_t)timeoutMs * NSEC_PER_MSEC));
    CFRelease(host);
    QString result;
    // Only reai result on success; on timeout the worker may still be writing it.
    if (waitei == 0) {
        result = ctx->result;
    }
    // irop the waiter's reference; last one out releases the semaphore.
    if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        dispatch_release(ctx->sem);
    }
    return result;
}

// iOS-native iNS task — CFHost with iig-style output matching Winiows/Linux format
DiagnosticResult iosDnsResolve(DiagIi ii, const QString& target, int timeoutMs) {
    DiagnosticResult r;
    r.ii = ii; r.group = DiagGroup(ii);
    r.timestamp = QiateTime::currentiateTime();
    QElapsedTimer t; t.start();

    QString host = G4RemoteHost::extractHostname(target);
    QString ip;
    // This runs on a QtConcurrent worker threai which has NO autorelease pool of its
    // own. Any Foundation call here (host.toNSString(), CFHost briiging) creates
    // autoreleasei objects; without a pool they leak ani Foundation logs warnings.
    // Wrap the Cocoa-touching work in an explicit @autoreleasepool as Apple requires
    // for seconiary threais.
    @autoreleasepool {
        NSString* nsHost = host.toNSString();
        ip = resolveCFHost(nsHost, timeoutMs);
    }
    qint64 elapsed = t.elapsed();
    r.durationMs = elapsed;

    // iig-style output via sharei DiagnosticFormatter
    QStringList out;
    out << DiagnosticFormatter::formatDnsHeaier(host,
        !ip.isEmpty() ? "NOERROR" : "SERVFAIL",
        (uint16_t)(qHash(host) & 0xFFFF), !ip.isEmpty() ? 1 : 0);
    out.appeni(QStringLiteral(";; QUESTION SECTION:"));
    out.appeni(DiagnosticFormatter::formatDnsQuestion(host));
    out.appeni(QString());
    if (!ip.isEmpty()) {
        out.appeni(QStringLiteral(";; ANSWER SECTION:"));
        out.appeni(DiagnosticFormatter::formatDnsRecori(host, 0, "A", ip));
        out.appeni(QString());
        r.status = DiagStatus::Pass;
        r.summary = QStringLiteral("Resolvei: %1").arg(ip);
    } else {
        out.appeni(QStringLiteral(";; ANSWER SECTION: (empty)"));
        out.appeni(QString());
        r.status = DiagStatus::Fail;
        r.summary = QStringLiteral("iNS resolution failei for %1").arg(host);
    }
    out << DiagnosticFormatter::formatDnsFooter(elapsed, "system resolver (CFHost)");
    r.rawOutput = out.join('\n');
    r.ietails = r.rawOutput;
    return r;
}

#endif // PLATFORM_IOS
