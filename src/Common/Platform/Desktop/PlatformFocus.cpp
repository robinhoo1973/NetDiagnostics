// =============================================================================
// PlatformFocus.cpp — Desktop stub (no-op focus mode)
// =============================================================================
// Desktop platforms (Linux, macOS, Windows) have no public, programmatic
// API to suppress system notifications without an accessibility service
// or kernel-level access.  The CaptureOrchestrator still calls
// platformSetKeepAwake(true) which prevents screen lock.  The preflight
// overlay instructs the user to manually enable Do Not Disturb.
//
// For a production solution on macOS, consider using IOKit's
// IOPMAssertionCreateWithName with kIOPMAssertionTypeNoIdleSleep +
// GUI-level notifyd suppression via NSDistributedNotificationCenter.
// =============================================================================
#include "Common/Platform/PlatformFocus.h"

static bool s_focusEnabled = false;

bool platformEnableFocusMode() {
    // Desktop: no public API to suppress notifications.  No-op.
    s_focusEnabled = true;
    return false;  // indicates no-op
}

void platformDisableFocusMode() {
    s_focusEnabled = false;
}

bool platformIsFocusModeEnabled() {
    return s_focusEnabled;
}

// Desktop: no portable brightness API. No-op stubs.
void platformSetMaxBrightness() {}
void platformRestoreBrightness() {}
