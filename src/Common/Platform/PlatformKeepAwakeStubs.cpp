// =============================================================================
// PlatformKeepAwakeStubs.cpp — Stubs for iOS/Android (no QProcess available)
// =============================================================================
// Desktop implementations are in PlatformKeepAwake.cpp (compiled only on
// non-mobile platforms). On iOS the UIApplication idleTimerDisabled API
// would be used (.mm file); on Android FLAG_KEEP_SCREEN_ON. These stubs
// are no-ops — the mobile implementations live in platform-specific files.
// =============================================================================
#include "Common/Platform/PlatformKeepAwake.h"

void platformSetKeepAwake(bool) {
    // iOS: use [UIApplication sharedApplication].idleTimerDisabled
    // Android: use WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
    // For now, no-op — mobile platforms have their own implementations.
}

bool platformIsKeepAwake() {
    return false;
}
