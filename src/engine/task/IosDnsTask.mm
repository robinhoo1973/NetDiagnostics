// =============================================================================
// IosDnsTask.mm — iOS DNS resolution via CFHost (async, cancellable, native)
//
// CFHost provides proper iOS DNS resolution with system cache and timeout
// control. Available since iOS 2.0. Replaces generic getaddrinfo+GCD.
// =============================================================================
#include "engine/task/DiagnosticTask.h"
#include "engine/diagnostic/G4RemoteHost.h"
#import <CFNetwork/CFNetwork.h>

static NSString* resolveCFHost(NSString* hostname, int timeoutMs) {
    CFHostRef host = CFHostCreateWithName(kCFAllocatorDefault, (__bridge CFStringRef)hostname);
    CFStreamError err;
    Boolean ok = CFHostStartInfoResolution(host, kCFHostAddresses, &err);
    if (!ok) return nil;

    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    __block NSString* result = nil;

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        Boolean resolved = false;
        CFArrayRef addrs = CFHostGetAddressing(host, &resolved);
        if (resolved && addrs && CFArrayGetCount(addrs) > 0) {
            for (CFIndex i = 0; i < CFArrayGetCount(addrs); i++) {
                CFDataRef data = (CFDataRef)CFArrayGetValueAtIndex(addrs, i);
                if (!data) continue;
                struct sockaddr_in* sa = (struct sockaddr_in*)CFDataGetBytePtr(data);
                if (sa->sin_family == AF_INET) {
                    char ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
                    result = [NSString stringWithUTF8String:ip];
                    break;
                }
            }
        }
        dispatch_semaphore_signal(sem);
    });

    dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW,
        (int64_t)timeoutMs * NSEC_PER_MSEC));
    CFRelease(host);
    return result;
}

// iOS-native DNS task — uses CFHost for proper system DNS resolution
static DiagnosticResult iosDnsResolve(DiagId id, const QString& target, int timeoutMs) {
    DiagnosticResult r;
    r.id = id; r.group = diagGroup(id);
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();

    QString host = G4RemoteHost::extractHostname(target);
    NSString* ip = resolveCFHost(host.toNSString(), timeoutMs);

    r.durationMs = t.elapsed();
    if (ip && ip.length > 0) {
        r.status = DiagStatus::Pass;
        r.summary = QStringLiteral("Resolved: %1").arg(QString::fromNSString(ip));
        r.rawOutput = QString::fromNSString(ip);
    } else {
        r.status = DiagStatus::Fail;
        r.summary = QStringLiteral("DNS resolution failed for %1").arg(host);
    }
    r.details = r.rawOutput;
    return r;
}
