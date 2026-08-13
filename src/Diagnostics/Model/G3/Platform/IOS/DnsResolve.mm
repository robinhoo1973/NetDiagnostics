// =============================================================================
// IosDnsTask.mm �� iOS DNS resolution via CFHost (async, cancellable, native)
//
// CFHost provides proper iOS DNS resolution with system cache and timeout
// control. Available since iOS 2.0. Replaces generic getaddrinfo+GCD.
// =============================================================================

#if defined(PLATFORM_IOS)

// 8-18（5WHY）：DiagnosticTask 已重构为 DiagnosticBase（DiagnosticTask.h 已删），
// 此 include 为陈旧的平台分支残留，仅 iOS CI 能编译到；显式改为结果结构头。
#include "Common/Model/DiagnosticResult.h"
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

    // 5WHY: CFHostStartInfoResolution is ASYNCHRONOUS — the addresses become
    // available only after the host's run-loop callbacks fire.  The old code
    // dispatched a GCD block that called CFHostGetAddressing immediately (no
    // run-loop scheduling or pumping), so resolution NEVER completed: the
    // result was always empty and the test reported SERVFAIL in ~1ms (see
    // crashes/Weixin Image_20260807095048_133_1.jpg).  Correct usage:
    // schedule the host on THIS thread's run loop, then pump that loop until
    // resolution completes or the timeout expires.
    CFRunLoopRef rl = CFRunLoopGetCurrent();
    CFHostScheduleWithRunLoop(host, rl, kCFRunLoopCommonModes);

    QElapsedTimer t; t.start();
    QString result;
    while (t.elapsed() < timeoutMs) {
        Boolean resolved = false;
        CFArrayRef addrs = CFHostGetAddressing(host, &resolved);
        if (resolved && addrs && CFArrayGetCount(addrs) > 0) {
            for (CFIndex i = 0; i < CFArrayGetCount(addrs); i++) {
                CFDataRef Data = (CFDataRef)CFArrayGetValueAtIndex(addrs, i);
                if (!Data) continue;
                struct sockaddr_in* sa = (struct sockaddr_in*)CFDataGetBytePtr(Data);
                if (sa->sin_family == AF_INET) {
                    char ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip));
                    result = QString::fromLatin1(ip);
                    break;
                }
            }
            if (!result.isEmpty()) break;
        }
        // Pump the run loop so CFHost's async resolution can progress.  This
        // helper runs on a QtConcurrent worker thread; running ITS run loop is
        // what lets CFHost deliver the resolution callbacks.
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, true);
    }

    CFHostUnscheduleFromRunLoop(host, rl, kCFRunLoopCommonModes);
    CFHostCancelInfoResolution(host, kCFHostAddresses);
    CFRelease(host);
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

    QString host = G4RemoteHost::extractHostname(target).trimmed();
    if (host.isEmpty()) {
        // 5WHY: defensive guard — iosDnsResolve() resolves a HOSTNAME; callers with
        // no target (G3 tests) used to hit this with an empty host, producing a
        // meaningless SERVFAIL.  Report clearly instead of resolving an empty name.
        r.durationMs = t.elapsed();
        r.status = DiagStatus::Fail;
        r.summary = QStringLiteral("DNS Resolution Failed for empty host (test requires a target)");
        r.rawOutput = QStringLiteral(";; ERROR: empty hostname — this test requires a target.");
        r.details = r.rawOutput;
        return r;
    }
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
        r.summary = QStringLiteral("DNS Resolution Failed for %1").arg(host);
    }
    out << DiagnosticFormatter::formatDnsFooter(elapsed, "system resolver (CFHost)");
    r.rawOutput = out.join('\n');
    r.details = r.rawOutput;
    return r;
}

#endif // PLATFORM_IOS
