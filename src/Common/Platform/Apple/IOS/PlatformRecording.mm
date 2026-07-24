// =============================================================================
// PlatformRecording.mm — iOS screen recording (RPScreenRecorder + AVAssetWriter)
// =============================================================================
#if defined(PLATFORM_IOS)

#include "Common/Platform/PlatformRecording.h"
#include <QString>
#include <QFileInfo>
#include <QDir>
#import <ReplayKit/ReplayKit.h>
#import <AVFoundation/AVFoundation.h>

// Static recording state — single-recording model matches the API contract.
static RPScreenRecorder*   s_recorder    = nil;
static AVAssetWriter*      s_writer      = nil;
static AVAssetWriterInput* s_input       = nil;
static NSString*           s_outputPath  = nil;
static RecordingCallback   s_startCb;
static bool                s_recording   = false;
static bool                s_stopping    = false;
static int                 s_frameCount  = 0;

// 5WHY: ReplayKit delivers CMSampleBuffer callbacks serially on its
// own internal queue.  Appending directly from the callback is safe —
// no need for a second serialization queue.  The previous s_writerQueue
// was allocated but never dispatched to (dead code).

// ═══════════════════════════════════════════════════════════════════════
// iOS: RPScreenRecorder.startCapture → AVAssetWriter → .mp4
// ═══════════════════════════════════════════════════════════════════════

void platformStartRecording(const QString& filePath, RecordingCallback callback) {
    if (s_recording) {
        if (callback) callback(false, QStringLiteral("Recording already in progress"));
        return;
    }

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

    s_outputPath = [outPath.toNSString() retain];
    s_startCb = callback;

    // Remove previous file
    [[NSFileManager defaultManager] removeItemAtPath:s_outputPath error:nil];

    // ── Configure AVAssetWriter ──
    NSError* err = nil;
    NSURL* url = [NSURL fileURLWithPath:s_outputPath];
    s_writer = [[AVAssetWriter alloc] initWithURL:url fileType:AVFileTypeMPEG4 error:&err];
    if (!s_writer || err) {
        if (s_startCb) s_startCb(false, QString::fromNSString(err.localizedDescription));
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
        if (s_startCb) s_startCb(false, QStringLiteral("Cannot add video input to writer"));
        return;
    }
    [s_writer addInput:s_input];

    // ── Start capture + writer ──
    s_frameCount = 0;
    [s_writer startWriting];
    // 5WHY: startSessionAtSourceTime deferred until first sample arrives.
    // Calling it before any sample has been appended places the writer
    // in an inconsistent state.

    [s_recorder startCaptureWithHandler:^(CMSampleBufferRef sampleBuffer,
                                           RPSampleBufferType bufferType,
                                           NSError* error) {
        if (error) {
            s_stopping = true;
            [s_writer cancelWriting];
            s_recording = false;
            if (s_startCb) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    s_startCb(false, QString::fromNSString(error.localizedDescription));
                });
            }
            return;
        }

        if (bufferType != RPSampleBufferTypeVideo) return;

        if (s_stopping) return;

        // 5WHY: startSessionAtSourceTime MUST be the first frame's PTS,
        // not kCMTimeZero.  Using zero can cause the first few frames
        // to have negative decode timestamps, corrupting the output.
        if (!s_recording && s_writer.status == AVAssetWriterStatusUnknown) {
            [s_writer startSessionAtSourceTime:CMSampleBufferGetPresentationTimeStamp(sampleBuffer)];
            s_recording = true;
            s_frameCount = 0;
            if (s_startCb) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    s_startCb(true, outPath);
                });
            }
        }

        // Append frame to writer (serialized via writer queue)
        if (s_recording && s_input.readyForMoreMediaData) {
            [s_input appendSampleBuffer:sampleBuffer];
            s_frameCount++;
        }
    } completionHandler:^(NSError* error) {
        if (error) {
            s_recording = false;
            if (s_startCb) {
                dispatch_async(dispatch_get_main_queue(), ^{
                    s_startCb(false, QString::fromNSString(error.localizedDescription));
                });
            }
        }
    }];
}

void platformStopRecording(RecordingCallback callback) {
    if (!s_recording || s_stopping) {
        if (callback) callback(false, QStringLiteral("No recording in progress"));
        return;
    }
    s_stopping = true;

    // 5WHY: stopCapture and finishWriting are asynchronous.  The callback
    // must fire AFTER both complete, or the file may be incomplete/zero-byte.
    [s_recorder stopCaptureWithHandler:^(NSError* error) {
        if (error) {
            s_recording = false;
            s_stopping = false;
            if (callback) callback(false, QString::fromNSString(error.localizedDescription));
            return;
        }

        [s_input markAsFinished];
        [s_writer finishWritingWithCompletionHandler:^{
            s_recording = false;
            s_stopping = false;

            // 5WHY: s_writer.status and s_writer.error must be captured
            // BEFORE s_writer is set to nil.  ObjC nil-messaging returns
            // 0 / nil, so checking .status after nil always shows
            // AVAssetWriterStatusUnknown instead of the real failure.
            BOOL ok = (s_writer.status == AVAssetWriterStatusCompleted);
            AVAssetWriterStatus finalStatus = s_writer.status;
            NSString* finalError = s_writer.error.localizedDescription;
            s_writer = nil;
            s_input = nil;
            s_frameCount = 0;

            if (s_outputPath) {
                if (ok && callback) {
                    callback(true, QString::fromNSString(s_outputPath));
                } else if (callback) {
                    callback(false, finalStatus == AVAssetWriterStatusFailed
                        ? QString::fromNSString(finalError)
                        : QStringLiteral("Writer did not complete"));
                }
                [s_outputPath release];
                s_outputPath = nil;
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
