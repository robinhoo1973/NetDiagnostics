// =============================================================================
// PlatformRecordingStubs.cpp — Stubs for iOS/Android (no QProcess available)
// =============================================================================
// Desktop implementations are in PlatformRecording.cpp (compiled only on
// non-mobile platforms). These stubs ensure the linker can resolve the
// symbols without pulling in QProcess, which is not available on iOS/Android.
// =============================================================================
#include "Common/Platform/PlatformRecording.h"

void platformStartRecording(const QString&, RecordingCallback callback) {
    if (callback) callback(false, QStringLiteral("Screen recording not supported on this platform"));
}

void platformStopRecording(RecordingCallback callback) {
    if (callback) callback(false, QStringLiteral("Screen recording not supported on this platform"));
}

bool platformIsRecording() {
    return false;
}

bool platformSupportsScreenshotWhileRecording() {
    return false;
}
