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
//   iOS — brightness → 0 + silent physical-switch simulation (best-effort)
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
