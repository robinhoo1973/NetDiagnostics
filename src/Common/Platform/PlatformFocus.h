// =============================================================================
// PlatformFocus.h — Cross-platform focus / Do-Not-Disturb abstraction
// =============================================================================
// Provides programmatic focus mode that suppresses notifications and
// distractions during automated evidence capture.  This is NOT a user-facing
// feature — it is called automatically by CaptureOrchestrator during the
// preflight phase and reverted after the capture completes.
//
// Platforms:
//   Linux/macOS desktop — no-op (no public API to suppress notifications)
//   Windows — no-op (no public API; Focus Assist requires WinRT)
//   iOS — set max brightness (1.0) for clear recordings + mute audio (best-effort)
//   Android — NotificationManager.setInterruptionFilter (API 23+)
//
// Design ref: review/06_Capture_Architecture_Design.md §2.6
// =============================================================================
#pragma once

/// Enable focus mode (suppress notifications, prevent interruptions).
/// This is best-effort — not all platforms support programmatic DND.
/// Returns true if the platform supports focus mode, false if no-op.
bool platformEnableFocusMode();

/// Disable focus mode and restore system notification state.
void platformDisableFocusMode();

/// Returns true if focus mode is currently active.
bool platformIsFocusModeEnabled();

/// Set screen brightness to maximum for clear recordings.
/// Saves previous brightness and restores via platformRestoreBrightness().
void platformSetMaxBrightness();

/// Restore screen brightness to the level saved by platformSetMaxBrightness().
void platformRestoreBrightness();

/// [iOS/Android only] Open the system Focus / Do-Not-Disturb settings page
/// so the user can manually enable it before capture starts.
/// Desktop: no-op.
void platformOpenFocusSettings();

/// Lock screen orientation to current value to prevent rotation during capture.
/// Must be paired with platformUnlockOrientation() at capture end.
void platformLockOrientation();

/// Restore auto-rotation after capture completes.
void platformUnlockOrientation();
