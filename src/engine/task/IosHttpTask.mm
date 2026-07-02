// =============================================================================
// IosHttpTask.mm — iOS HTTP diagnostics via NSURLSession (native, no libcurl)
//
// Eliminates the libcurl cross-compile dependency for iOS. NSURLSession provides:
// - Full HTTP/HTTPS support with system SSL (Secure Transport)
// - NSURLSessionTaskMetrics for timing (DNS, connect, TLS, TTFB)
// - Native caching, cookie handling, redirect following
// - Available since iOS 7.0
// =============================================================================
#include "engine/task/DiagnosticTask.h"
#include "models/DiagId.h"
#include <QUrl>
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

// ── Task creation helpers for TaskFactory ────────────────────────────────

// Returns a DiagnosticResult for iOS-native G5 HTTP diagnostics.
// Used by TaskFactory when PLATFORM_IOS is defined (no libcurl needed).
DiagnosticResult iosHttpDiagnostic(DiagId id, const QString& target) {
    switch (id) {
        case DiagId::G5HttpHeaders:    return iosHttpHeaders(id, target);
        case DiagId::G5CurlVerbose:    return iosCurlVerbose(id, target);
        case DiagId::G5SslCertificate: return iosSslCert(id, target);
        case DiagId::G5HttpRedirect:   return iosHttpRedirect(id, target);
        default:
            return DiagnosticResult::skipped(id, QStringLiteral("G5 test not implemented (iOS native)"));
    }
}
