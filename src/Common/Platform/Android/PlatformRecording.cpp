// =============================================================================
// PlatformRecording.cpp — Android screen recording (MediaProjection)
// =============================================================================
// Uses MediaProjection API (API 21+) to capture the screen and encode it
// to an MP4 file via MediaRecorder.
//
// Permission flow:
//   1. Create a screen capture Intent via MediaProjectionManager
//   2. Start the Intent for result (user sees a system dialog)
//   3. On permission granted, create MediaProjection + VirtualDisplay
//   4. MediaRecorder encodes frames from the VirtualDisplay surface
//
// Reference:
//   https://developer.android.com/media/mediaprojection
//   https://developer.android.com/reference/android/media/MediaRecorder
//
// Qt integration:
//   Uses QtAndroidPrivate::startActivity() (Qt 6.3+) for the async
//   permission result.  Falls back to a warning stub if unavailable.
// =============================================================================
#if defined(PLATFORM_ANDROID)

#include "Common/Platform/PlatformRecording.h"
#include "Common/Platform/Android/PlatformAndroidJni.h"
#include <QJniEnvironment>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <atomic>

// QtAndroidPrivate is available as a private header when building for Android
// with Qt 6.3+.  Guard with __ANDROID__ and a runtime availability check.
#if __has_include(<QtCore/private/qandroidextras_p.h>)
#include <QtCore/private/qandroidextras_p.h>
#define HAS_QT_ANDROID_EXTRAS
#endif

// ═══════════════════════════════════════════════════════════════════════════
// Static state
// ═══════════════════════════════════════════════════════════════════════════
static bool            s_recording   = false;
static std::atomic<bool> s_stopping {false};
static QString         s_outputPath;
static RecordingCallback s_startCallback = nullptr;
// 5WHY: MediaProjection and VirtualDisplay are JNI global refs that must
// outlive individual JNI calls.  JNI local refs are invalid after the
// native method returns — using them across async callbacks (permission
// dialog result, recording error) would crash.  Store as QJniObject globals.
static QJniObject      s_mediaProjection;
static QJniObject      s_virtualDisplay;
static QJniObject      s_mediaRecorder;
static int             s_requestCode = 1001;

// ═══════════════════════════════════════════════════════════════════════════
// Helper: get MediaProjectionManager system service
// ═══════════════════════════════════════════════════════════════════════════
static QJniObject getProjectionManager() {
    QJniObject activity = getQtActivity();
    if (!activity.isValid()) return {};
    return activity.callObjectMethod(
        "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;",
        QJniObject::getStaticObjectField(
            "android/content/Context",
            "MEDIA_PROJECTION_SERVICE",
            "Ljava/lang/String;").object());
}

// ═══════════════════════════════════════════════════════════════════════════
// Set up MediaRecorder + VirtualDisplay after permission granted
// ═══════════════════════════════════════════════════════════════════════════
static void setupRecorder(int resultCode, QJniObject data) {
    if (resultCode != -1 /* RESULT_OK */) {
        if (s_startCallback) {
            auto cb = s_startCallback;
            s_startCallback = nullptr;
            cb(false, QStringLiteral("Screen capture permission denied by user"));
        }
        return;
    }

    QJniObject activity = getQtActivity();
    if (!activity.isValid()) {
        if (s_startCallback) {
            auto cb = s_startCallback;
            s_startCallback = nullptr;
            cb(false, QStringLiteral("Cannot get Activity after permission grant"));
        }
        return;
    }

    // Create MediaProjection from the granted intent
    QJniObject projectionMgr = getProjectionManager();
    if (!projectionMgr.isValid()) {
        if (s_startCallback) {
            auto cb = s_startCallback;
            s_startCallback = nullptr;
            cb(false, QStringLiteral("Cannot get MediaProjectionManager"));
        }
        return;
    }

    s_mediaProjection = projectionMgr.callObjectMethod(
        "getMediaProjection",
        "(ILandroid/content/Intent;)Landroid/media/projection/MediaProjection;",
        resultCode, data.object());
    if (!s_mediaProjection.isValid()) {
        if (s_startCallback) {
            auto cb = s_startCallback;
            s_startCallback = nullptr;
            cb(false, QStringLiteral("Failed to create MediaProjection"));
        }
        return;
    }

    // ── Set up MediaRecorder ────────────────────────────────────────
    s_mediaRecorder = QJniObject("android/media/MediaRecorder", "()V");
    if (!s_mediaRecorder.isValid()) {
        if (s_startCallback) {
            auto cb = s_startCallback;
            s_startCallback = nullptr;
            cb(false, QStringLiteral("Cannot create MediaRecorder"));
        }
        return;
    }

    jint audioSource = QJniObject::getStaticField<jint>(
        "android/media/MediaRecorder$AudioSource", "MIC");
    jint videoSource = QJniObject::getStaticField<jint>(
        "android/media/MediaRecorder$VideoSource", "SURFACE");
    jint outputFormat = QJniObject::getStaticField<jint>(
        "android/media/MediaRecorder$OutputFormat", "MPEG_4");
    jint videoEncoder = QJniObject::getStaticField<jint>(
        "android/media/MediaRecorder$VideoEncoder", "H264");
    jint audioEncoder = QJniObject::getStaticField<jint>(
        "android/media/MediaRecorder$AudioEncoder", "AAC");

    s_mediaRecorder.callMethod<void>("setAudioSource", "(I)V", audioSource);
    s_mediaRecorder.callMethod<void>("setVideoSource", "(I)V", videoSource);
    s_mediaRecorder.callMethod<void>("setOutputFormat", "(I)V", outputFormat);
    s_mediaRecorder.callMethod<void>("setOutputFile", "(Ljava/lang/String;)V",
        QJniObject::fromString(s_outputPath).object());
    s_mediaRecorder.callMethod<void>("setVideoEncoder", "(I)V", videoEncoder);
    s_mediaRecorder.callMethod<void>("setAudioEncoder", "(I)V", audioEncoder);
    s_mediaRecorder.callMethod<void>("setVideoSize", "(II)V", 1280, 720);
    s_mediaRecorder.callMethod<void>("setVideoFrameRate", "(I)V", 30);

    // 5WHY: MediaRecorder.prepare() and .start() can throw IOException
    // (invalid output file, unsupported codec) or IllegalStateException
    // (wrong call order).  QJniObject translates pending JNI exceptions
    // into C++ exceptions — without try/catch, an uncaught exception
    // propagates through the QtAndroidPrivate callback and crashes.
    // The stop/release calls in platformStopRecording() already have
    // this protection; add it here for symmetry.
    try {
        s_mediaRecorder.callMethod<void>("prepare");
        s_mediaRecorder.callMethod<void>("start");
    } catch (...) {
        qWarning() << "PlatformRecording: MediaRecorder prepare/start threw — tearing down";
        // 5WHY: release() can also throw if the MediaRecorder is in a
        // corrupted state after a failed prepare/start.  Wrap in a nested
        // try/catch so a JNI exception from release() doesn't escape the
        // outer catch handler and crash through the QtAndroidPrivate callback.
        try {
            s_mediaRecorder.callMethod<void>("release");
        } catch (...) {
            qWarning() << "PlatformRecording: MediaRecorder.release() also threw during cleanup";
        }
        s_mediaRecorder = {};
        // 5WHY: s_mediaProjection was set at line 105 BEFORE prepare/start.
        // If we fail here, the projection must be stopped or the screen-cast
        // icon stays in the status bar indefinitely.
        if (s_mediaProjection.isValid()) {
            s_mediaProjection.callMethod<void>("stop");
            s_mediaProjection = {};
        }
        if (s_startCallback) {
            auto cb = s_startCallback;
            s_startCallback = nullptr;
            cb(false, QStringLiteral("MediaRecorder prepare/start failed"));
        }
        return;
    }

    // Get display density for VirtualDisplay
    QJniObject metrics("android/util/DisplayMetrics", "()V");
    QJniObject display = activity.callObjectMethod(
        "getWindowManager", "()Landroid/view/WindowManager;")
        .callObjectMethod("getDefaultDisplay", "()Landroid/view/Display;");
    display.callMethod<void>("getMetrics",
        "(Landroid/util/DisplayMetrics;)V", metrics.object());
    jint displayDensity = metrics.getField<jint>("densityDpi");

    QJniObject surface = s_mediaRecorder.callObjectMethod(
        "getSurface", "()Landroid/view/Surface;");

    s_virtualDisplay = s_mediaProjection.callObjectMethod(
        "createVirtualDisplay",
        "(Ljava/lang/String;IIIILandroid/view/Surface;Landroid/hardware/display/VirtualDisplay$Callback;Landroid/os/Handler;)Landroid/hardware/display/VirtualDisplay;",
        QJniObject::fromString("NetDiagnosticsCapture").object(),
        1280, 720, displayDensity,
        QJniObject::getStaticField<jint>(
            "android/hardware/display/VirtualDisplay",
            "VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR"),
        surface.object(),
        nullptr, nullptr);

    if (!s_virtualDisplay.isValid()) {
        // 5WHY: platformStopRecording wraps stop() in try/catch (line 294).
        // Match that protection here for symmetry — a partially-started
        // MediaRecorder can throw IllegalStateException on stop().
        try {
            s_mediaRecorder.callMethod<void>("stop");
        } catch (...) {
            qWarning() << "PlatformRecording: MediaRecorder.stop() threw during VirtualDisplay failure cleanup";
        }
        try {
            s_mediaRecorder.callMethod<void>("release");
        } catch (...) {
            qWarning() << "PlatformRecording: MediaRecorder.release() threw during VirtualDisplay failure cleanup";
        }
        s_mediaRecorder = {};
        // 5WHY: s_mediaProjection was set at line 105.  Stop it so the
        // screen-cast icon doesn't persist in the status bar.
        if (s_mediaProjection.isValid()) {
            s_mediaProjection.callMethod<void>("stop");
            s_mediaProjection = {};
        }
        if (s_startCallback) {
            auto cb = s_startCallback;
            s_startCallback = nullptr;
            cb(false, QStringLiteral("Failed to create VirtualDisplay"));
        }
        return;
    }

    s_recording = true;
    if (s_startCallback) {
        auto cb = s_startCallback;
        s_startCallback = nullptr;
        cb(true, s_outputPath);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// platformStartRecording
// ═══════════════════════════════════════════════════════════════════════════
void platformStartRecording(const QString& filePath, RecordingCallback callback) {
    if (s_recording) {
        if (callback) callback(false, QStringLiteral("Recording already in progress"));
        return;
    }

    s_outputPath = filePath;
    if (!s_outputPath.endsWith(QStringLiteral(".mp4"), Qt::CaseInsensitive)) {
        s_outputPath += QStringLiteral(".mp4");
    }
    QDir().mkpath(QFileInfo(s_outputPath).absolutePath());

    s_startCallback = callback;

    // Request screen capture permission
    QJniObject projectionMgr = getProjectionManager();
    if (!projectionMgr.isValid()) {
        if (callback) callback(false, QStringLiteral("Cannot get MediaProjectionManager"));
        s_startCallback = nullptr;
        return;
    }

    QJniObject intent = projectionMgr.callObjectMethod(
        "createScreenCaptureIntent",
        "()Landroid/content/Intent;");
    if (!intent.isValid()) {
        if (callback) callback(false, QStringLiteral("Cannot create screen capture intent"));
        s_startCallback = nullptr;
        return;
    }

#ifdef HAS_QT_ANDROID_EXTRAS
    // 5WHY: Qt 6.3+ provides QtAndroidPrivate::startActivity which registers
    // an internal activity result listener and invokes the callback when the
    // user responds to the system permission dialog.  This is the cleanest
    // approach — no manual JNI listener registration needed.
    QtAndroidPrivate::startActivity(intent, s_requestCode, [](int requestCode, int resultCode, QJniObject data) {
        Q_UNUSED(requestCode);
        setupRecorder(resultCode, data);
    });
#else
    // 5WHY: Fallback for older Qt versions or when qtandroidextras_p.h is
    // not available.  Register a manual OnActivityResultListener via JNI.
    // Store the listener ID so it can be removed later.
    //
    // For now, warn and return — the recording won't start without the
    // permission callback.  The scenario's timeout will advance the capture.
    qWarning() << "PlatformRecording: QtAndroidPrivate not available — "
                  "screen recording requires user permission and cannot proceed. "
                  "Use Screenshot-only mode.";
    if (s_startCallback) {
        auto cb = s_startCallback;
        s_startCallback = nullptr;
        cb(false, QStringLiteral("QtAndroidPrivate unavailable — cannot request screen capture permission"));
    }
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// platformStopRecording
// ═══════════════════════════════════════════════════════════════════════════
void platformStopRecording(RecordingCallback callback) {
    if (!s_recording && !s_mediaRecorder.isValid()) {
        if (callback) callback(false, QStringLiteral("No recording in progress"));
        return;
    }

    // 5WHY: Atomic guard prevents double-stop from cancel() +
    // restoreSystemState() racing.  The first call proceeds with
    // stop+release; the second returns immediately.
    if (s_stopping.exchange(true)) {
        if (callback) callback(false, QStringLiteral("Stop already in progress"));
        return;
    }

    if (s_mediaRecorder.isValid()) {
        try {
            s_mediaRecorder.callMethod<void>("stop");
        } catch (...) {
            qWarning() << "PlatformRecording: MediaRecorder.stop() threw — continuing cleanup";
        }
        // 5WHY: release() can also throw if MediaRecorder is in a corrupted
        // state after stop().  Wrap in try/catch for symmetry with setupRecorder()
        // (which protects both stop+release in the prepare/start failure path).
        try {
            s_mediaRecorder.callMethod<void>("release");
        } catch (...) {
            qWarning() << "PlatformRecording: MediaRecorder.release() threw during stop cleanup";
        }
        s_mediaRecorder = {};
    }

    if (s_virtualDisplay.isValid()) {
        try {
            s_virtualDisplay.callMethod<void>("release");
        } catch (...) {
            qWarning() << "PlatformRecording: VirtualDisplay.release() threw during stop cleanup";
        }
        s_virtualDisplay = {};
    }

    if (s_mediaProjection.isValid()) {
        try {
            s_mediaProjection.callMethod<void>("stop");
        } catch (...) {
            qWarning() << "PlatformRecording: MediaProjection.stop() threw during stop cleanup";
        }
        s_mediaProjection = {};
    }

    s_recording = false;
    s_stopping = false;

    QFileInfo fi(s_outputPath);
    if (fi.exists() && fi.size() > 0) {
        if (callback) callback(true, s_outputPath);
    } else {
        if (callback) callback(false, QStringLiteral("Recording file is empty or missing"));
    }
}

bool platformIsRecording() {
    return s_recording;
}

bool platformSupportsScreenshotWhileRecording() {
    // 5WHY: MediaProjection captures the screen into a Surface via
    // VirtualDisplay.  Taking a separate QScreen::grabWindow during
    // recording contends on the display pipeline and produces
    // corrupted/blank screenshots.  Both mode is iOS-only (ReplayKit
    // delivers CMSampleBuffer frames that can be extracted in-band).
    return false;
}

#endif // PLATFORM_ANDROID
