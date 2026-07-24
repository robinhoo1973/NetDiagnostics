// =============================================================================
// PlatformRecording.h — Cross-platform screen recording abstraction
// =============================================================================
// Design ref: docs/AutomatedEvidenceCapture_Design.md §4.7
//
// Desktop (Linux):  ffmpeg x11grab via QProcess
// Desktop (macOS):   AVFoundation via ffmpeg avfoundation
// Desktop (Windows): ffmpeg gdigrab via QProcess
// iOS:               RPScreenRecorder (ReplayKit)
// Android:           MediaProjection + MediaRecorder
//
// Requires ffmpeg in PATH for desktop platforms.
// =============================================================================
#pragma once

#include <QString>
#include <functional>

using RecordingCallback = std::function<void(bool ok, const QString& filePathOrError)>;

// Start recording the primary screen to the given filePath (without extension;
// platform adds .mp4 / .mov as appropriate).
// callback: invoked when recording has started (or failed to start).
void platformStartRecording(const QString& filePath, RecordingCallback callback);

// Stop the active recording. callback returns the final file path on success,
// or an error message on failure.
void platformStopRecording(RecordingCallback callback);

// True if a recording is currently in progress.
bool platformIsRecording();

// True if the platform supports taking screenshots while recording
// (iOS ReplayKit does; most other platforms require separate processes).
bool platformSupportsScreenshotWhileRecording();
