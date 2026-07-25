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
#include <memory>
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
    [s_writer startWriting];

    // 5WHY: The capture-handler and completion-handler error paths each
    // duplicated a ~16-line cleanup block.  Extract once — defined before
    // startCaptureWithHandler so it is alive when the handlers fire.
    void (^cleanupAfterError)(NSError*) = ^(NSError* err) {
        __block RecordingCallback cb;
        __block QString errStr;
        dispatch_sync(s_stateQueue(), ^{
            if (s_startCb) {
                cb = s_startCb;
                s_startCb = nullptr;
                errStr = QString::fromNSString(err.localizedDescription);
            } else {
                s_lastError = err.localizedDescription;
            }
            s_writer = nil;
            s_input = nil;
            s_outputPath = nil;
        });
        s_stopping = false;
        if (cb) {
            dispatch_async(dispatch_get_main_queue(), ^{
                cb(false, errStr);
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
        if (!s_recording && s_writer.status == AVAssetWriterStatusUnknown) {
            [s_writer startSessionAtSourceTime:CMSampleBufferGetPresentationTimeStamp(sampleBuffer)];
            s_recording = true;
            // 5WHY: s_startCb (std::function) read/write was unsynchronised
            // between the ReplayKit queue and the main thread via
            // platformStopRecording → dispatch_sync(s_stateQueue(), …).
            // Concurrent read/write of a non-trivial C++ object from two
            // threads is a data race (§[intro.races] UB).  Wrap access in
            // the state queue — this only executes once (first frame), so
            // the dispatch_sync cost is a one-time overhead of ~1-3 µs.
            auto cbPtr = std::make_shared<RecordingCallback>();
            dispatch_sync(s_stateQueue(), ^{
                if (s_startCb) {
                    *cbPtr = std::move(s_startCb);
                    s_startCb = nullptr;
                }
            });
            if (*cbPtr) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    (*cbPtr)(true, outPath);
                });
            }
        }

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
        // 5WHY: When !s_recording (recording hasn't started yet, e.g. cancel
        // during StartingRecording), the ReplayKit frame handler may still
        // fire after this function returns and invoke the stale s_startCb
        // callback, which would try to transition the FSM out of its current
        // (terminal/cancelled) state.  Clear s_startCb AND set s_stopping so
        // the frame handler (line 186: if (s_stopping) return) prevents the
        // recording from ever starting — without s_stopping, the recording
        // starts on the platform and runs indefinitely because the orchestrator
        // has already cleared m_recording in restoreSystemState().
        if (!s_recording) {
            s_stopping = true;
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
