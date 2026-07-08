// =============================================================================
// IosHttpTask.mm — iOS HTTP diagnostics via NSURLSession (native, no libcurl)
//
// Eliminates the libcurl cross-compile dependency for iOS. NSURLSession provides:
// - Full HTTP/HTTPS support with system SSL (Secure Transport)
// - NSURLSessionTaskMetrics for timing (DNS, connect, TLS, TTFB)
// - Native caching, cookie handling, redirect following
// - Available since iOS 7.0
// =============================================================================

#ifdef PLATFORM_IOS

#include "engine/task/DiagnosticTask.h"
#include "models/DiagId.h"
#include <QUrl>
#include <QElapsedTimer>
#include <QStringList>
#include <atomic>
#include <memory>
#import <Foundation/Foundation.h>

// ── Helper: synchronous HTTP GET with metrics ──────────────────────────
struct IosHttpResult {
    int statusCode = 0;
    QString error;
    qint64 dnsMs = 0, connectMs = 0, tlsMs = 0, firstByteMs = 0, totalMs = 0;
    qint64 bodyBytes = 0;
    QString redirectUrl;
    QStringList headers;
};

static IosHttpResult httpGetSync(NSString* urlStr, int timeoutMs, bool followRedirect) {
    IosHttpResult r;
    NSURL* url = [NSURL URLWithString:urlStr];
    if (!url) { r.error = QStringLiteral("Invalid URL"); return r; }

    // Reference-counted context: waiter and completion handler both hold a ref (2 total).
    // Whoever finishes last (waiter on timeout, or handler on response) releases the semaphore.
    struct HttpCtx {
        dispatch_semaphore_t sem;
        IosHttpResult result;
        std::atomic<int> refs;
    };
    auto ctx = std::make_shared<HttpCtx>();
    ctx->sem = dispatch_semaphore_create(0);
    ctx->result = r;
    ctx->refs.store(2, std::memory_order_relaxed);  // waiter + handler

    NSURLSessionConfiguration* config = [NSURLSessionConfiguration defaultSessionConfiguration];
    config.timeoutIntervalForRequest = timeoutMs / 1000.0;
    config.timeoutIntervalForResource = (timeoutMs + 30) / 1000.0;
    NSURLSession* session = [NSURLSession sessionWithConfiguration:config];

    NSURLSessionDataTask* task = [session dataTaskWithURL:url
        completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
            if (error) {
                ctx->result.error = QString::fromNSString(error.localizedDescription);
                if (data) ctx->result.bodyBytes = data.length;
            } else {
                NSHTTPURLResponse* httpResp = (NSHTTPURLResponse*)response;
                ctx->result.statusCode = (int)httpResp.statusCode;
                ctx->result.bodyBytes = data ? data.length : 0;
                // Collect headers
                for (NSString* key in httpResp.allHeaderFields) {
                    ctx->result.headers.append(
                        QStringLiteral("%1: %2").arg(QString::fromNSString(key),
                                                     QString::fromNSString(httpResp.allHeaderFields[key])));
                }
            }
            dispatch_semaphore_signal(ctx->sem);
            // Drop the handler's reference; last one out releases the semaphore.
            if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                dispatch_release(ctx->sem);
            }
        }];

    // NOTE: NSURLSessionTaskMetrics (DNS/connect/TLS/TTFB timing) requires a
    // session delegate implementing URLSession:task:didFinishCollectingMetrics:.
    // A synchronous semaphore-based request cannot use a delegate here, so the
    // fine-grained timing fields remain 0. (A previous version called a
    // non-existent -setCollectsMetrics: selector, which crashed with
    // "unrecognized selector sent to instance".)

    [task resume];
    long waited = dispatch_semaphore_wait(ctx->sem, dispatch_time(DISPATCH_TIME_NOW, (int64_t)(timeoutMs + 5000) * NSEC_PER_MSEC));
    [session finishTasksAndInvalidate];
    IosHttpResult result;
    // Only read result on success; on timeout the handler may still be writing it.
    if (waited == 0) {
        result = ctx->result;
    } else {
        result.error = QStringLiteral("Request timeout");
    }
    // Drop the waiter's reference; last one out releases the semaphore.
    if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        dispatch_release(ctx->sem);
    }
    return result;
}

namespace G5WebsiteUrl {

// ── G5 HTTP Headers diagnostic ─────────────────────────────────────────
static DiagnosticResult iosHttpHeaders(DiagId id, const QString& target) {
    QUrl u(target);
    auto r = httpGetSync(target.toNSString(), 15000, false);
    DiagnosticResult dr; dr.id = id; dr.group = DiagGroup::G5;
    dr.timestamp = QDateTime::currentDateTime();
    if (!r.error.isEmpty()) { dr.status = DiagStatus::Fail; dr.summary = r.error; return dr; }
    dr.rawOutput = r.headers.join('\n');
    dr.details = dr.rawOutput;
    dr.status = DiagStatus::Pass;
    dr.summary = QStringLiteral("HTTP %1, %2 bytes").arg(r.statusCode).arg(r.bodyBytes);
    return dr;
}

// ── G5 Curl Verbose (GET full output) ───────────────────────────────────
static DiagnosticResult iosCurlVerbose(DiagId id, const QString& target) {
    auto r = httpGetSync(target.toNSString(), 60000, true);
    DiagnosticResult dr; dr.id = id; dr.group = DiagGroup::G5;
    dr.timestamp = QDateTime::currentDateTime();
    if (!r.error.isEmpty()) { dr.status = DiagStatus::Fail; dr.summary = r.error; return dr; }
    QStringList lines;
    lines.append(QStringLiteral("HTTP/%1 %2").arg(1.1).arg(r.statusCode));
    for (const auto& h : r.headers) lines.append(h);
    lines.append(QString());
    lines.append(QStringLiteral("Timing: DNS=%1ms Connect=%2ms TLS=%3ms TTFB=%4ms Total=%5ms")
        .arg(r.dnsMs).arg(r.connectMs).arg(r.tlsMs).arg(r.firstByteMs).arg(r.totalMs));
    dr.rawOutput = lines.join('\n'); dr.details = dr.rawOutput;
    dr.status = r.statusCode >= 200 && r.statusCode < 400 ? DiagStatus::Pass : DiagStatus::Warning;
    dr.summary = QStringLiteral("HTTP %1 (%2ms)").arg(r.statusCode).arg(r.totalMs);
    return dr;
}

// ── G5 SSL Certificate ──────────────────────────────────────────────────
static DiagnosticResult iosSslCert(DiagId id, const QString& target) {
    QUrl u(target);
    DiagnosticResult dr; dr.id = id; dr.group = DiagGroup::G5;
    dr.timestamp = QDateTime::currentDateTime();

    // Reference-counted context: waiter and completion handler both hold a ref (2 total).
    // NEVER write a __block C++ object from the handler and return it by value on timeout
    // — that races the return and corrupts QString refcounts. Store plain fields in ctx.
    struct SslCtx {
        dispatch_semaphore_t sem;
        int status;      // 0=unset, 1=pass, 2=fail
        QString summary;
        QString rawOutput;
        std::atomic<int> refs;
    };
    auto ctx = std::make_shared<SslCtx>();
    ctx->sem = dispatch_semaphore_create(0);
    ctx->status = 0;
    ctx->refs.store(2, std::memory_order_relaxed);

    NSURLSession* session = [NSURLSession sessionWithConfiguration:
        [NSURLSessionConfiguration ephemeralSessionConfiguration]];

    NSURLSessionDataTask* task = [session dataTaskWithURL:[NSURL URLWithString:target.toNSString()]
        completionHandler:^(NSData* data, NSURLResponse* resp, NSError* error) {
            if (error) {
                ctx->status = 2;
                ctx->summary = QString::fromNSString(error.localizedDescription);
            } else {
                ctx->status = 1;
                NSHTTPURLResponse* httpResp = (NSHTTPURLResponse*)resp;
                ctx->summary = QStringLiteral("SSL OK, HTTP %1").arg((int)httpResp.statusCode);
                ctx->rawOutput = QStringLiteral("TLS handshake completed successfully");
            }
            dispatch_semaphore_signal(ctx->sem);
            if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                dispatch_release(ctx->sem);
            }
        }];
    [task resume];
    long waited = dispatch_semaphore_wait(ctx->sem, dispatch_time(DISPATCH_TIME_NOW, 15 * NSEC_PER_SEC));
    [session finishTasksAndInvalidate];
    // Only read ctx fields on success; on timeout the handler may still be writing them.
    if (waited == 0 && ctx->status != 0) {
        dr.status = (ctx->status == 1) ? DiagStatus::Pass : DiagStatus::Fail;
        dr.summary = ctx->summary;
        dr.rawOutput = ctx->rawOutput;
    } else {
        dr.status = DiagStatus::Fail;
        dr.summary = QStringLiteral("SSL check timed out");
    }
    dr.details = dr.rawOutput;
    if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        dispatch_release(ctx->sem);
    }
    return dr;
}

// ── G5 HTTP Redirect check ──────────────────────────────────────────────
static DiagnosticResult iosHttpRedirect(DiagId id, const QString& target) {
    auto r = httpGetSync(target.toNSString(), 15000, true);
    DiagnosticResult dr; dr.id = id; dr.group = DiagGroup::G5;
    dr.timestamp = QDateTime::currentDateTime();
    if (!r.error.isEmpty()) { dr.status = DiagStatus::Fail; dr.summary = r.error; return dr; }
    if (r.statusCode >= 300 && r.statusCode < 400) {
        dr.status = DiagStatus::Warning;
        dr.summary = QStringLiteral("Redirect %1").arg(r.statusCode);
    } else {
        dr.status = DiagStatus::Pass;
        dr.summary = QStringLiteral("No redirect (HTTP %1)").arg(r.statusCode);
    }
    dr.rawOutput = r.headers.join('\n'); dr.details = dr.rawOutput;
    return dr;
}

// ── G5 Security Headers ──────────────────────────────────────────────────
static DiagnosticResult iosSecurityHeaders(DiagId id, const QString& target) {
    auto r = httpGetSync(target.toNSString(), 15000, true);
    DiagnosticResult dr; dr.id = id; dr.group = DiagGroup::G5;
    dr.timestamp = QDateTime::currentDateTime();
    if (!r.error.isEmpty()) { dr.status = DiagStatus::Fail; dr.summary = r.error; return dr; }
    QStringList found;
    for (const auto& h : r.headers) {
        int colon = h.indexOf(':');
        if (colon > 0) found.append(h.left(colon).toLower().trimmed());
    }
    const QStringList required = {
        QStringLiteral("strict-transport-security"), QStringLiteral("content-security-policy"),
        QStringLiteral("x-frame-options"), QStringLiteral("x-content-type-options"),
        QStringLiteral("x-xss-protection"), QStringLiteral("referrer-policy"),
        QStringLiteral("permissions-policy")};
    QStringList missing;
    QStringList lines;
    lines << QStringLiteral("Security header audit for %1").arg(target) << QString();
    for (const auto& h : required) {
        bool ok = found.contains(h);
        if (!ok) missing << h;
        lines << QStringLiteral("  [%1] %2").arg(ok ? QStringLiteral("PASS") : QStringLiteral("MISS"), h);
    }
    dr.rawOutput = lines.join('\n'); dr.details = dr.rawOutput;
    dr.status = missing.isEmpty() ? DiagStatus::Pass
              : missing.size() <= 4 ? DiagStatus::Warning : DiagStatus::Fail;
    dr.summary = missing.isEmpty() ? QStringLiteral("All 7 present")
               : QStringLiteral("%1 of 7 missing").arg(missing.size());
    return dr;
}

// ── G5 HTTP Compression ──────────────────────────────────────────────────
// NSURLSession transparently decompresses (and strips Content-Encoding) ONLY when
// it added the Accept-Encoding header itself. If WE set Accept-Encoding manually,
// the system leaves the response encoded and preserves Content-Encoding — letting
// us detect server-side compression.
static DiagnosticResult iosHttpCompression(DiagId id, const QString& target) {
    DiagnosticResult dr; dr.id = id; dr.group = DiagGroup::G5;
    dr.timestamp = QDateTime::currentDateTime();

    struct Ctx {
        dispatch_semaphore_t sem;
        int status;               // 0=unset,1=ok,2=err
        QString encoding, err;
        qint64 bytes;
        std::atomic<int> refs;
    };
    auto ctx = std::make_shared<Ctx>();
    ctx->sem = dispatch_semaphore_create(0);
    ctx->status = 0; ctx->bytes = 0;
    ctx->refs.store(2, std::memory_order_relaxed);

    NSMutableURLRequest* req = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:target.toNSString()]];
    [req setValue:@"gzip, deflate, br" forHTTPHeaderField:@"Accept-Encoding"];
    NSURLSession* session = [NSURLSession sessionWithConfiguration:
        [NSURLSessionConfiguration ephemeralSessionConfiguration]];
    NSURLSessionDataTask* task = [session dataTaskWithRequest:req
        completionHandler:^(NSData* data, NSURLResponse* resp, NSError* error) {
            if (error) { ctx->status = 2; ctx->err = QString::fromNSString(error.localizedDescription); }
            else {
                NSHTTPURLResponse* http = (NSHTTPURLResponse*)resp;
                NSString* enc = [http valueForHTTPHeaderField:@"Content-Encoding"];
                if (enc) ctx->encoding = QString::fromNSString(enc);
                ctx->bytes = data ? (qint64)data.length : 0;
                ctx->status = 1;
            }
            dispatch_semaphore_signal(ctx->sem);
            if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) dispatch_release(ctx->sem);
        }];
    [task resume];
    long waited = dispatch_semaphore_wait(ctx->sem, dispatch_time(DISPATCH_TIME_NOW, (int64_t)15 * NSEC_PER_SEC));
    [session finishTasksAndInvalidate];

    if (waited == 0 && ctx->status == 1) {
        bool compressed = !ctx->encoding.isEmpty();
        dr.status = DiagStatus::Info;
        dr.summary = compressed ? QStringLiteral("Compressed: %1").arg(ctx->encoding)
                                : QStringLiteral("Uncompressed");
        dr.rawOutput = QStringLiteral("HTTP compression check for %1\n\n  Content-Encoding: %2\n  Body bytes: %3")
            .arg(target, compressed ? ctx->encoding : QStringLiteral("(none)"))
            .arg(ctx->bytes);
    } else if (waited == 0 && ctx->status == 2) {
        dr.status = DiagStatus::Fail; dr.summary = ctx->err;
    } else {
        dr.status = DiagStatus::Fail; dr.summary = QStringLiteral("Request timed out");
    }
    dr.details = dr.rawOutput;
    if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) dispatch_release(ctx->sem);
    return dr;
}

// ── G5 HTTP Timing (NSURLSessionTaskMetrics via delegate) ─────────────────
@interface NDMetricsDelegate : NSObject <NSURLSessionTaskDelegate>
@property (atomic) NSTimeInterval dnsMs;
@property (atomic) NSTimeInterval connectMs;
@property (atomic) NSTimeInterval tlsMs;
@property (atomic) NSTimeInterval ttfbMs;
@property (atomic) NSTimeInterval totalMs;
@end
@implementation NDMetricsDelegate
- (void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task
    didFinishCollectingMetrics:(NSURLSessionTaskMetrics*)metrics {
    NSURLSessionTaskTransactionMetrics* m = metrics.transactionMetrics.lastObject;
    if (!m) return;
    auto ms = [](NSDate* a, NSDate* b) -> NSTimeInterval {
        if (!a || !b) return 0;
        return [b timeIntervalSinceDate:a] * 1000.0;
    };
    self.dnsMs     = ms(m.domainLookupStartDate, m.domainLookupEndDate);
    self.connectMs = ms(m.connectStartDate, m.connectEndDate);
    self.tlsMs     = ms(m.secureConnectionStartDate, m.secureConnectionEndDate);
    self.ttfbMs    = ms(m.requestStartDate, m.responseStartDate);
    self.totalMs   = ms(m.fetchStartDate, m.responseEndDate);
}
@end

static DiagnosticResult iosHttpTiming(DiagId id, const QString& target) {
    DiagnosticResult dr; dr.id = id; dr.group = DiagGroup::G5;
    dr.timestamp = QDateTime::currentDateTime();

    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    // Session retains the delegate until invalidated; autorelease balances alloc (MRC).
    NDMetricsDelegate* delegate = [[[NDMetricsDelegate alloc] init] autorelease];
    NSURLSession* session = [NSURLSession sessionWithConfiguration:
        [NSURLSessionConfiguration ephemeralSessionConfiguration]
        delegate:delegate delegateQueue:nil];

    __block int statusCode = 0; __block bool ok = false; __block QString err;
    QElapsedTimer wall; wall.start();
    NSURLSessionDataTask* task = [session dataTaskWithURL:[NSURL URLWithString:target.toNSString()]
        completionHandler:^(NSData* data, NSURLResponse* resp, NSError* error) {
            if (error) { err = QString::fromNSString(error.localizedDescription); }
            else { statusCode = (int)((NSHTTPURLResponse*)resp).statusCode; ok = true; }
            dispatch_semaphore_signal(sem);
        }];
    [task resume];
    long waited = dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, (int64_t)30 * NSEC_PER_SEC));
    // Metrics are delivered to the delegate as the task finishes; invalidating and
    // waiting flushes them before we read.
    [session finishTasksAndInvalidate];
    dispatch_release(sem); // MRC: balance dispatch_semaphore_create
    qint64 wallMs = wall.elapsed();

    if (waited != 0) { dr.status = DiagStatus::Fail; dr.summary = QStringLiteral("Request timed out"); return dr; }
    if (!ok) { dr.status = DiagStatus::Fail; dr.summary = err; return dr; }

    qint64 dns = (qint64)delegate.dnsMs, conn = (qint64)delegate.connectMs,
           tls = (qint64)delegate.tlsMs, ttfb = (qint64)delegate.ttfbMs;
    qint64 total = delegate.totalMs > 0 ? (qint64)delegate.totalMs : wallMs;
    QStringList lines;
    lines << QStringLiteral("HTTP timing for %1 (HTTP %2)").arg(target).arg(statusCode) << QString();
    lines << QStringLiteral("  DNS lookup        : %1 ms").arg(dns);
    lines << QStringLiteral("  TCP connect       : %1 ms").arg(conn);
    lines << QStringLiteral("  TLS handshake     : %1 ms").arg(tls);
    lines << QStringLiteral("  Time to first byte: %1 ms").arg(ttfb);
    lines << QStringLiteral("  Total             : %1 ms").arg(total);
    dr.rawOutput = lines.join('\n'); dr.details = dr.rawOutput;
    dr.status = total < 1000 ? DiagStatus::Pass : total < 3000 ? DiagStatus::Warning : DiagStatus::Fail;
    dr.summary = QStringLiteral("DNS=%1ms Connect=%2ms TTFB=%3ms Total=%4ms").arg(dns).arg(conn).arg(ttfb).arg(total);
    dr.durationMs = total;
    return dr;
}

// ── Task creation helpers for TaskFactory ────────────────────────────────

// Returns a DiagnosticResult for iOS-native G5 HTTP diagnostics.
// Used by TaskFactory when PLATFORM_IOS is defined (no libcurl needed).
DiagnosticResult iosHttpDiagnostic(DiagId id, const QString& target) {
    // Runs on a QtConcurrent worker thread with no autorelease pool of its own.
    // NSURL/NSURLSession/NSURLSessionConfiguration created below are autoreleased;
    // Apple requires each secondary thread that makes Cocoa calls to provide its own
    // @autoreleasepool, otherwise these objects leak. The DiagnosticResult returned is
    // a pure C++ value, so it is unaffected by the pool draining.
    @autoreleasepool {
        switch (id) {
            case DiagId::G5HttpHeaders:     return iosHttpHeaders(id, target);
            case DiagId::G5CurlVerbose:     return iosCurlVerbose(id, target);
            case DiagId::G5SslCertificate:  return iosSslCert(id, target);
            case DiagId::G5HttpRedirect:    return iosHttpRedirect(id, target);
            case DiagId::G5SecurityHeaders: return iosSecurityHeaders(id, target);
            case DiagId::G5HttpCompression: return iosHttpCompression(id, target);
            case DiagId::G5HttpTiming:      return iosHttpTiming(id, target);
            default:
                return DiagnosticResult::skipped(id, QStringLiteral("G5 test not implemented (iOS native)"));
        }
    }
}

} // namespace G5WebsiteUrl

#endif // PLATFORM_IOS
