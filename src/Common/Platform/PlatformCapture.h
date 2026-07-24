// =============================================================================
// PlatformCapture.h — Cross-platform screenshot abstraction
// =============================================================================
// Captures a screenshot of the primary screen and saves it as PNG.
//
// Desktop (Linux/macOS/Windows): QScreen::grabWindow(0) — entire screen.
// iOS/Android: platform-specific implementations in respective .mm/.cpp files.
//
// Design ref: review/06_Capture_Architecture_Design.md §2.14
// =============================================================================
#pragma once

#include <QString>

// Capture the primary screen and save as PNG to the given filePath.
// Returns true on success, false if the screen is unavailable or save fails.
// Creates parent directories if they don't exist.
bool platformCaptureScreenshot(const QString& filePath);
