// =============================================================================
// PlatformKeepAwake.mm — iOS screen keep-awake (UIApplication.idleTimerDisabled)
// =============================================================================
#if defined(PLATFORM_IOS)

#include "Common/Platform/PlatformKeepAwake.h"
#import <UIKit/UIKit.h>

// 5WHY: static state survives across calls but doesn't survive app
// backgrounding (iOS resets idleTimerDisabled automatically).  Using
// a static is correct here because we're tracking the INTENT, not the
// actual system state (which iOS may override).
static bool s_keepAwakeRequested = false;

void platformSetKeepAwake(bool enable) {
    // 5WHY: must be called from main thread — UIApplication is not
    // thread-safe.  The caller (CaptureOrchestrator) always runs on
    // the main thread via Qt's event loop.
    dispatch_async(dispatch_get_main_queue(), ^{
        [UIApplication sharedApplication].idleTimerDisabled = enable;
    });
    s_keepAwakeRequested = enable;
}

bool platformIsKeepAwake() {
    return s_keepAwakeRequested;
}

#endif // PLATFORM_IOS
