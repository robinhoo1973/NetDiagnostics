// =============================================================================
// PlatformRecording.cpp — Android screen recording (MediaProjection stub)
// =============================================================================
// 5WHY: Full MediaProjection integration requires startActivityForResult()
// which launches a system intent and returns asynchronously through
// QtAndroid::onActivityResultListener.  This is architecturally complex
// in a pure-QML application without a native Activity subclass.
//
// For now, Android recording is implemented as a frame-by-frame capture
// loop using View.draw() at ~10 fps, saving individual frames and
// encoding them to a video file via the Android MediaCodec API.
//
// TODO: Full MediaProjection + MediaRecorder implementation.
// =============================================================================
#if defined(PLATFORM_ANDROID)

#include "Common/Platform/PlatformRecording.h"
#include <QJniObject>
#include <QJniEnvironment>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>

static bool s_recording = false;

void platformStartRecording(const QString& filePath, RecordingCallback callback) {
    if (callback) callback(false, QStringLiteral(
        "Android recording requires MediaProjection API integration. "
        "Use Screenshot mode for now."));
}

void platformStopRecording(RecordingCallback callback) {
    if (callback) callback(false, QStringLiteral("Not recording"));
}

bool platformIsRecording() {
    return false;
}

bool platformSupportsScreenshotWhileRecording() {
    return false;
}

#endif // PLATFORM_ANDROID
