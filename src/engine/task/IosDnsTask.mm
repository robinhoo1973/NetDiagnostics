// =============================================================================
// IosDnsTask.mm — iOS DNS resolution via CFHost (async, cancellable, native)
//
// CFHost provides proper iOS DNS resolution with system cache and timeout
// control. Available since iOS 2.0. Replaces generic getaddrinfo+GCD.
// =============================================================================
#include "engine/task/DiagnosticTask.h"
#include "engine/diagnostic/G4RemoteHost.h"
#include "util/DiagnosticFormatter.h"
#import <CFNetwork/CFNetwork.h>

static NSString* resolveCFHost(NSString* hostname, int timeoutMs) {
    CFHostRef host = CFHostCreateWithName(kCFAllocatorDefault, (__bridge CFStringRef)hostname);
    if (!host) return nil;
    CFStreamError err;
    Boolean ok = CFHostStartInfoResolution(host, kCFHostAddresses, &err);
    if (!ok) { CFRelease(host); return nil; }

    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    __block NSString* result = nil;

    // The worker block accesses `host` after this function may have already
    // timed out and returned. Give the block its own reference so `host`
    // stays valid until the block finishes, preventing a use-after-free.
    CFRetain(host);
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
        CFRelease(host);
        dispatch_semaphore_signal(sem);
    });

    long waited = dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW,
        (int64_t)timeoutMs * NSEC_PER_MSEC));
    // Release this function's own reference. On timeout the block still holds
    // its reference (released above), so `host` remains valid there.
    CFRelease(host);
    dispatch_release(sem); // MRC: balance dispatch_semaphore_create; block keeps its own ref
    // Only read `result` when the block actually completed; on timeout the
    // block may still be writing to it (data race), so return nil instead.
    return (waited == 0) ? result : nil;
}

// iOS-native DNS task — CFHost with dig-style output matching Windows/Linux format
DiagnosticResult iosDnsResolve(DiagId id, const QString& target, int timeoutMs) {
    DiagnosticResult r;
    r.id = id; r.group = diagGroup(id);
    r.timestamp = QDateTime::currentDateTime();
    QElapsedTimer t; t.start();

    QString host = G4RemoteHost::extractHostname(target);
    NSString* nsHost = host.toNSString();
    QByteArray hb = host.toUtf8();
    NSString* ip = resolveCFHost(nsHost, timeoutMs);
    qint64 elapsed = t.elapsed();
    r.durationMs = elapsed;

    // Dig-style output via shared DiagnosticFormatter
    QStringList out;
    out << DiagnosticFormatter::formatDnsHeader(host,
        ip ? "NOERROR" : "SERVFAIL",
        (uint16_t)(qHash(host) & 0xFFFF), ip ? 1 : 0);
    out.append(QStringLiteral(";; QUESTION SECTION:"));
    out.append(DiagnosticFormatter::formatDnsQuestion(host));
    out.append(QString());
    if (ip && ip.length > 0) {
        out.append(QStringLiteral(";; ANSWER SECTION:"));
        out.append(DiagnosticFormatter::formatDnsRecord(host, 0, "A", QString::fromNSString(ip)));
        out.append(QString());
        r.status = DiagStatus::Pass;
        r.summary = QStringLiteral("Resolved: %1").arg(QString::fromNSString(ip));
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
