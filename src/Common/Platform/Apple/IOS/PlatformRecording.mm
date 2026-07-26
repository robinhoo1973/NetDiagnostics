// =============================================================================
// PlatformRecording.mm — iOS screen recording (RPScreenRecorder + AVAssetWriter)
// =============================================================================
#if defined(PLATFORM_IOS)

#include "Common/Platform/PlatformRecording.h"
#include <QString>
#include <QFileInfo>
#include <QDir>
#include <QtDebug>
#include <atomic>
#import <ReplayKit/ReplayKit.h>
#import <AVFoundation/AVFoundation.h>

// ── Serial state queue ───────────────────────────────────────────────────
// 5WHY: s_writer, s_input, s_lastError, s_startCb, and s_outputPath are
// ObjC object pointers / C++ std::function accessed from two threads:
//   (a) the calling thread (main thread via CaptureOrchestrator)
//   (b) the ReplayKit internal queue (capture/stop/completion handlers)
//
// std::atomic does not compose with ARC __strong ownership, and the
// ReplayKit queue is NOT the same as the main thread — a raw pointer
// read/write race on ARM64 can technically tear across cache lines.
//
// ReplayKit serialises its own callbacks (frame delivery, error, and
// stop-completion all fire on one internal serial queue), so handlers
// never run concurrently with each other.  The remaining race is:
//   Main thread reads s_lastError / s_writer / s_input
//       while
//   ReplayKit queue writes them (error handler or stop-completion)
//
// A dedicated serial dispatch queue serialises every cross-thread state
// mutation.  Frame-appending (CMSampleBuffer → AVAssetWriterInput) runs
// at 30/60 fps and does NOT go through this queue — it only reads
// s_recording (atomic) and s_input (stable pointer during normal recording);
// the ReplayKit queue guarantees errors won't fire mid-frame.
//
// Callbacks are invoked OUTSIDE the state queue to avoid deadlock:
//   s_stateQueue → dispatch_async(main) → callback
// rather than:
//   main → dispatch_sync(s_stateQueue) → dispatch_async(main) DEADLOCK
// =============================================================================
static dispatch_queue_t s_stateQueue() {
    static dispatch_queue_t q;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        q = dispatch_queue_create("netdiag.recording.state",
                                   DISPATCH_QUEUE_SERIAL);
    });
    return q;
}

// ── State ─────────────────────────────────────────────────────────────────
static RPScreenRecorder*   s_recorder    = nil;
static AVAssetWriter*      s_writer      = nil;
static AVAssetWriterInput* s_input       = nil;
static NSString*           s_outputPath  = nil;
static RecordingCallback   s_startCb;
// s_recording / s_stopping use std::atomic for fast lock-free checks on both
// the calling thread and the ReplayKit frame-delivery hot path.
static std::atomic<bool>   s_recording{false};
static std::atomic<bool>   s_stopping{false};
// Protected by s_stateQueue() — every read/write from a thread other than
// the exclusive writer's thread must use dispatch_sync(s_stateQueue(), …).
static NSString*           s_lastError   = nil;

// ═══════════════════════════════════════════════════════════════════════════
// iOS: RPScreenRecorder.startCapture → AVAssetWriter → .mp4
// ═══════════════════════════════════════════════════════════════════════════

void platformStartRecording(const QString& filePath, RecordingCallback callback) {
    if (s_recording) {
        if (callback) callback(false, QStringLiteral("Recording already in progress"));
        return;
    }

    // 5WHY: s_stopping may be left true from a previous cancel-during-
    // StartingRecording (platformStopRecording sets it to true when
    // !s_recording to prevent the frame handler from starting the
    // recording).  Reset it here so the new session's frame handler
    // doesn't discard all frames.
    s_stopping = false;

    // Clear any stale error from a previous recording session
    dispatch_sync(s_stateQueue(), ^{
        s_lastError = nil;
    });

    // Ensure output directory exists
    QString outPath = filePath;
    if (!outPath.endsWith(QStringLiteral(".mp4"), Qt::CaseInsensitive))
        outPath += QStringLiteral(".mp4");
    QDir().mkpath(QFileInfo(outPath).absolutePath());

    s_recorder = [RPScreenRecorder sharedRecorder];
    if (!s_recorder.available) {
        if (callback) callback(false, QStringLiteral("Screen recording not available on this device"));
        return;
    }

    s_outputPath = outPath.toNSString();
    s_startCb = callback;

    // Remove previous file
    [[NSFileManager defaultManager] removeItemAtPath:s_outputPath error:nil];

    // ── Configure AVAssetWriter ──
    NSError* err = nil;
    NSURL* url = [NSURL fileURLWithPath:s_outputPath];
    s_writer = [[AVAssetWriter alloc] initWithURL:url fileType:AVFileTypeMPEG4 error:&err];
    if (!s_writer || err) {
        if (s_startCb) {
            s_startCb(false, QString::fromNSString(err.localizedDescription));
            s_startCb = nullptr;
        }
        s_outputPath = nil;
        return;
    }

    // Video settings: H.264, screen native resolution, ~3 Mbps
    CGSize screenSize = UIScreen.mainScreen.bounds.size;
    CGFloat scale = UIScreen.mainScreen.scale;
    NSDictionary* videoSettings = @{
        AVVideoCodecKey: AVVideoCodecTypeH264,
        AVVideoWidthKey: @(screenSize.width * scale),
        AVVideoHeightKey: @(screenSize.height * scale),
        AVVideoCompressionPropertiesKey: @{
            AVVideoAverageBitRateKey: @(3000000),
            AVVideoProfileLevelKey: AVVideoProfileLevelH264HighAutoLevel,
        }
    };
    s_input = [[AVAssetWriterInput alloc] initWithMediaType:AVMediaTypeVideo
                                              outputSettings:videoSettings];
    s_input.expectsMediaDataInRealTime = YES;

    if (![s_writer canAddInput:s_input]) {
        if (s_startCb) {
            s_startCb(false, QStringLiteral("Cannot add video input to writer"));
            s_startCb = nullptr;
        }
        s_writer = nil;
        s_input = nil;
        s_outputPath = nil;
        return;
    }
    [s_writer addInput:s_input];

    // ── Start capture + writer ──
    BOOL started = [s_writer startWriting];
    if (!started) {
        qWarning() << "PlatformRecording: AVAssetWriter.startWriting failed — status:"
                    << (int)s_writer.status
                    << "error:" << (s_writer.error ? QString::fromNSString(s_writer.error.localizedDescription) : QStringLiteral("none"));
        if (s_startCb) {
            s_startCb(false, QStringLiteral("AVAssetWriter failed to start writing"));
            s_startCb = nullptr;
        }
        s_writer = nil;
        s_input = nil;
        s_outputPath = nil;
        return;
    }

    // 5WHY: The capture-handler and completion-handler error paths each
    // duplicated a ~16-line cleanup block.  Extract once — defined before
    // startCaptureWithHandler so it is alive when the handlers fire.
    void (^cleanupAfterError)(NSError*) = ^(NSError* err) {
        __block RecordingCallback cb;
        __block NSString* errDesc;
        dispatch_sync(s_stateQueue(), ^{
            if (s_startCb) {
                cb = s_startCb;
                s_startCb = nullptr;
                errDesc = err.localizedDescription;
            } else {
                s_lastError = err.localizedDescription;
            }
            s_writer = nil;
            s_input = nil;
            s_outputPath = nil;
            // 5WHY: Reset s_stopping inside the state queue so the
            // s_writer/s_input/s_outputPath=nil writes and the
            // s_stopping=false write are a single synchronised snapshot.
            // A concurrent platformStopRecording on the main thread sees a
            // consistent state: either all pre-cleanup or all post-cleanup.
            s_stopping = false;
        });
        if (cb) {
            dispatch_async(dispatch_get_main_queue(), ^{
                cb(false, QString::fromNSString(errDesc));
            });
        }
    };

    // 5WHY: startSessionAtSourceTime MUST be deferred until the first sample
    // arrives.  Calling it before any sample has been appended places the
    // writer in an inconsistent state (AVAssetWriterStatusUnknown → failed).

    [s_recorder startCaptureWithHandler:^(CMSampleBufferRef sampleBuffer,
                                           RPSampleBufferType bufferType,
                                           NSError* error) {
        if (error) {
            s_stopping = true;
            [s_writer cancelWriting];
            s_recording = false;
            cleanupAfterError(error);
            return;
        }

        if (bufferType != RPSampleBufferTypeVideo) return;
        if (s_stopping) return;

        // 5WHY: startSessionAtSourceTime MUST use the first frame's PTS,
        // not kCMTimeZero.  Using zero can cause the first few frames to
        // have negative decode timestamps, corrupting the output.
        // s_writer was set before startCaptureWithHandler and is only
        // mutated by the error/stop handlers on this same ReplayKit queue
        // — safe to read directly on the frame-delivery hot path.
        // 5WHY: After startWriting, the writer transitions to Writing status
        // (NOT Unknown).  The old check `== AVAssetWriterStatusUnknown` was
        // always false — the first-frame callback NEVER fired, the FSM stayed
        // stuck in StartingRecording, and all subsequent frames were silently
        // dropped.  Accept Writing status so the session actually begins.
        // 5WHY (round-31): If the writer transitions to Failed between
        // startWriting (returns YES) and first-frame arrival (e.g. disk full),
        // neither Writing nor Unknown matches — the first-frame handler
        // silently drops all frames and the FSM hangs forever in
        // StartingRecording with no timeout.  Treat Failed as a terminal
        // error: report it so the orchestrator transitions to Failed.
        if (!s_recording) {
            // 5WHY: Read s_writer.status once — AVAssetWriter.status is
            // atomic (non-nonatomic) so each access acquires an internal
            // spinlock.  The status cannot change between consecutive reads
            // on the same ReplayKit serial queue, so hoisting into a local
            // eliminates a redundant acquire/release and closes a TOCTOU
            // window between the Failed check and the Writing||Unknown check.
            AVAssetWriterStatus const st = s_writer.status;
            if (st == AVAssetWriterStatusFailed) {
                qWarning() << "PlatformRecording: AVAssetWriter failed before first frame — status:"
                           << (int)st
                           << "error:" << (s_writer.error ? QString::fromNSString(s_writer.error.localizedDescription) : QStringLiteral("none"));
                s_stopping = true;
                s_recording = false;
                [s_writer cancelWriting];
                cleanupAfterError(s_writer.error ?: [NSError errorWithDomain:@"PlatformRecording"
                                                              code:-1
                                                          userInfo:@{NSLocalizedDescriptionKey: @"Writer failed before first frame"}]);
                return;
            }
            if (st == AVAssetWriterStatusWriting
                || st == AVAssetWriterStatusUnknown) {
                CMTime firstPts = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
                qInfo() << "PlatformRecording: first video frame — starting session at PTS"
                         << firstPts.value << "/" << firstPts.timescale;
                [s_writer startSessionAtSourceTime:firstPts];
                s_recording = true;
                // 5WHY: s_startCb (std::function) read/write was unsynchronised
                // between the ReplayKit queue and the main thread via
                // platformStopRecording → dispatch_sync(s_stateQueue(), …).
                // Concurrent read/write of a non-trivial C++ object from two
                // threads is a data race (§[intro.races] UB).  Wrap access in
                // the state queue — this only executes once (first frame), so
                // the dispatch_sync cost is a one-time overhead of ~1-3 µs.
                __block RecordingCallback cb;
                dispatch_sync(s_stateQueue(), ^{
                    if (s_startCb) {
                        cb = std::move(s_startCb);
                        s_startCb = nullptr;
                    }
                });
                if (cb) {
                    dispatch_async(dispatch_get_main_queue(), ^{
                        cb(true, outPath);
                    });
                } else {
                    qWarning() << "PlatformRecording: no start callback — recording may be orphaned";
                }
            } else {
                // 5WHY: If the writer entered an unexpected status (Completed
                // or Cancelled) between startWriting and first-frame arrival,
                // we must not silently drop frames forever.  Treat as a
                // terminal error so the orchestrator transitions to Failed
                // (consistent with the AVAssetWriterStatusFailed path above).
                qWarning() << "PlatformRecording: AVAssetWriter in unexpected status before first frame — status:"
                           << (int)s_writer.status
                           << "error:" << (s_writer.error ? QString::fromNSString(s_writer.error.localizedDescription) : QStringLiteral("none"));
                s_stopping = true;
                s_recording = false;
                [s_writer cancelWriting];
                cleanupAfterError(s_writer.error ?: [NSError errorWithDomain:@"PlatformRecording"
                                                              code:-1
                                                          userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Writer in unexpected status %ld before first frame", (long)s_writer.status]}]);
                return;
            } // end if (s_writer.status == Writing || Unknown)
        } // end if (!s_recording)

        if (s_recording && s_input.readyForMoreMediaData) {
            [s_input appendSampleBuffer:sampleBuffer];
        }
    } completionHandler:^(NSError* error) {
        if (error) {
            s_recording = false;
            cleanupAfterError(error);
        }
    }];
}

void platformStopRecording(RecordingCallback callback) {
    // 5WHY: lock-free atomic check first — fast path for the common case
    // where recording was never started or already stopped.
    if (!s_recording || s_stopping) {
        // 5WHY: Set s_stopping=true FIRST — closes the TOCTOU window
        // between the outer !s_recording check above and the inner
        // !s_recording check below.  If the ReplayKit frame handler
        // fires on its queue between those two reads and starts the
        // recording, s_stopping=true causes the handler's guard
        // (if (s_stopping) return;) to prevent the session from
        // beginning — the recording never starts and the callback
        // (s_startCb) remains stored for delivery below.
        s_stopping = true;
        if (callback) {
            __block QString errMsg;
            __block bool hasError = false;
            dispatch_sync(s_stateQueue(), ^{
                if (s_lastError) {
                    errMsg = QString::fromNSString(s_lastError);
                    s_lastError = nil;  // delivered — clear for next session
                    hasError = true;
                }
            });
            if (hasError) {
                callback(false, errMsg);
            } else {
                callback(false, QStringLiteral("No recording in progress"));
            }
        }
        // 5WHY: if callback is nil, s_lastError stays in the queue for
        // the next caller (e.g. destructor safety net).
        //
        // 5WHY: s_stopping was already set to true at the top of this block
        // to close the TOCTOU window.  If recording hasn't started yet, also
        // clear s_startCb to prevent the first-frame callback from firing on
        // a stale session.
        if (!s_recording) {
            dispatch_sync(s_stateQueue(), ^{
                s_startCb = nullptr;
            });
        }
        return;
    }
    s_stopping = true;

    [s_recorder stopCaptureWithHandler:^(NSError* error) {
        if (error) {
            s_recording = false;
            s_stopping = false;
            if (callback) callback(false, QString::fromNSString(error.localizedDescription));
            dispatch_sync(s_stateQueue(), ^{
                s_writer = nil;
                s_input = nil;
                s_outputPath = nil;
            });
            return;
        }

        // 5WHY: A concurrent capture-handler error (from the ReplayKit
        // internal queue) may have already cleaned up s_input and s_writer
        // before this block executes.  Read under the state queue so we
        // see a consistent snapshot.
        __block BOOL tornDown = NO;
        __block QString tearDownMsg;
        dispatch_sync(s_stateQueue(), ^{
            if (!s_input || !s_writer) {
                tornDown = YES;
                tearDownMsg = s_lastError
                    ? QStringLiteral("Recording already stopped due to error: ") + QString::fromNSString(s_lastError)
                    : QStringLiteral("Recording already stopped due to error");
                s_lastError = nil;
                s_writer = nil;
                s_input = nil;
                s_outputPath = nil;
            }
        });
        if (tornDown) {
            s_recording = false;
            s_stopping = false;
            if (callback) callback(false, tearDownMsg);
            return;
        }

        [s_input markAsFinished];
        [s_writer finishWritingWithCompletionHandler:^{
            s_recording = false;
            s_stopping = false;

            __block BOOL ok;
            __block NSString* outPath;
            __block NSString* finalError;
            __block AVAssetWriterStatus finalStatus;
            dispatch_sync(s_stateQueue(), ^{
                ok = (s_writer.status == AVAssetWriterStatusCompleted);
                finalStatus = s_writer.status;
                finalError = s_writer.error.localizedDescription;
                outPath = s_outputPath;
                s_writer = nil;
                s_input = nil;
                s_outputPath = nil;
            });

            if (outPath) {
                if (ok && callback) {
                    callback(true, QString::fromNSString(outPath));
                } else if (callback) {
                    callback(false, finalStatus == AVAssetWriterStatusFailed
                        ? QString::fromNSString(finalError)
                        : QStringLiteral("Writer did not complete"));
                }
            } else if (callback) {
                // 5WHY: A concurrent ReplayKit error handler may have
                // cleared s_outputPath before this completion block ran.
                // If outPath is nil AND a real callback is waiting, we
                // MUST deliver it — otherwise the FSM hangs in
                // StoppingRecording forever (no timeout on stop-callback).
                callback(false, QStringLiteral("Recording output path lost — "
                    "concurrent error may have torn down the session"));
            }
        }];
    }];
}

bool platformIsRecording() {
    return s_recording;
}

bool platformSupportsScreenshotWhileRecording() {
    // 5WHY: RPScreenRecorder delivers CMSampleBuffer frames — we could
    // extract CVImageBuffer → CGImage → UIImage → PNG in the capture
    // handler for in-band screenshots.  Declared as supported so the
    // Both mode stays enabled on iOS.
    return true;
}

#endif // PLATFORM_IOS
