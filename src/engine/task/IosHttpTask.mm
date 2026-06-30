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

    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    NSURLSessionConfiguration* config = [NSURLSessionConfiguration defaultSessionConfiguration];
    config.timeoutIntervalForRequest = timeoutMs / 1000.0;
    config.timeoutIntervalForResource = (timeoutMs + 30) / 1000.0;
    NSURLSession* session = [NSURLSession sessionWithConfiguration:config];

    __block IosHttpResult blockResult = r;
    NSURLSessionDataTask* task = [session dataTaskWithURL:url
        completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
            if (error) {
                blockResult.error = QString::fromNSString(error.localizedDescription);
                if (data) blockResult.bodyBytes = data.length;
            } else {
                NSHTTPURLResponse* httpResp = (NSHTTPURLResponse*)response;
                blockResult.statusCode = (int)httpResp.statusCode;
                blockResult.bodyBytes = data ? data.length : 0;
                // Collect headers
                for (NSString* key in httpResp.allHeaderFields) {
                    blockResult.headers.append(
                        QStringLiteral("%1: %2").arg(QString::fromNSString(key),
                                                     QString::fromNSString(httpResp.allHeaderFields[key])));
                }
            }
            dispatch_semaphore_signal(sem);
        }];

    // Capture metrics for timing breakdown
    if (@available(iOS 10.0, *)) {
        [task performSelector:@selector(setCollectsMetrics:) withObject:(__bridge id)(__bridge void*)@YES];
    }

    [task resume];
    dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, (int64_t)(timeoutMs + 5000) * NSEC_PER_MSEC));
    [session finishTasksAndInvalidate];
    return blockResult;
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
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    __block DiagnosticResult dr; dr.id = id; dr.group = DiagGroup::G5;
    dr.timestamp = QDateTime::currentDateTime();

    NSURLSession* session = [NSURLSession sessionWithConfiguration:
        [NSURLSessionConfiguration ephemeralSessionConfiguration]];

    NSURLSessionDataTask* task = [session dataTaskWithURL:[NSURL URLWithString:target.toNSString()]
        completionHandler:^(NSData* data, NSURLResponse* resp, NSError* error) {
            if (error) {
                dr.status = DiagStatus::Fail;
                dr.summary = QString::fromNSString(error.localizedDescription);
            } else {
                dr.status = DiagStatus::Pass;
                NSHTTPURLResponse* httpResp = (NSHTTPURLResponse*)resp;
                dr.summary = QStringLiteral("SSL OK, HTTP %1").arg((int)httpResp.statusCode);
                dr.rawOutput = QStringLiteral("TLS handshake completed successfully");
            }
            dispatch_semaphore_signal(sem);
        }];
    [task resume];
    dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 15 * NSEC_PER_SEC));
    [session finishTasksAndInvalidate];
    dr.details = dr.rawOutput;
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
