// =============================================================================
// PlatformKeepAwake.h — Prevent screen lock / auto-dim during capture
// =============================================================================
// Design ref: docs/AutomatedEvidenceCapture_Design.md §4.6
//
// Desktop (Linux): D-Bus org.freedesktop.ScreenSaver.Inhibit / UnInhibit
// iOS:             [[UIApplication sharedApplication] setIdleTimerDisabled:]
// Android:         WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
// =============================================================================
#pragma once

// Enable or disable screen keep-awake. Call in pairs: enable before capture,
// disable after capture completes or is cancelled.
void platformSetKeepAwake(bool enable);

// Query current keep-awake state (best-effort; may return false on platforms
// where state tracking is not available).
bool platformIsKeepAwake();
