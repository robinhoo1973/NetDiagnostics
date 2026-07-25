// =============================================================================
// PlatformFocus.mm — iOS focus / Do-Not-Disturb implementation
// =============================================================================
// iOS does not expose a public API to programmatically enable DND/Focus mode.
// As a best-effort alternative:
//   1. Mute the app's audio session (AVAudioSession) — silences in-app audio
//   2. Prevent screen lock (already handled by PlatformKeepAwake)
// Note: Screen brightness is managed separately by platformSetMaxBrightness /
// platformRestoreBrightness, which set max brightness for clear recordings
// and restore the original level after capture.
//
// True Focus mode activation requires the Settings app or a system extension
// (not available to third-party apps without MDM).
// =============================================================================
#include "Common/Platform/PlatformFocus.h"
#import <UIKit/UIKit.h>
#import <AVFoundation/AVFoundation.h>

// ── Main-thread dispatch helper ───────────────────────────────────────────
// 5WHY: dispatch_sync on the main queue from the main thread deadlocks.
// Use this helper to safely execute UI work on the main thread regardless
// of the calling thread.  Eliminates duplication across all platformFocus
// and brightness functions.
static void runOnMainThread(void (^block)(void)) {
    if ([NSThread isMainThread]) {
        block();
    } else {
        dispatch_sync(dispatch_get_main_queue(), block);
    }
}

static bool s_focusEnabled = false;

bool platformEnableFocusMode() {
    if (s_focusEnabled) return true;

    runOnMainThread(^{
        // Mute audio (best-effort — this silences app audio, not ringer)
        [[AVAudioSession sharedInstance] setActive:NO error:nil];
    });

    s_focusEnabled = true;
    return true;
}

void platformDisableFocusMode() {
    if (!s_focusEnabled) return;

    runOnMainThread(^{
        // Reactivate audio session (reverses the setActive:NO in enable)
        [[AVAudioSession sharedInstance] setActive:YES error:nil];
    });

    s_focusEnabled = false;
}

bool platformIsFocusModeEnabled() {
    return s_focusEnabled;
}

// ── Brightness control for recording clarity ──────────────────────────────
static CGFloat s_savedBrightness = -1.0;
// 5WHY: Track save state with a separate boolean, consistent with the
// Android fix.  Avoids relying on a magic sentinel value that could
// overlap with a valid brightness value.
static bool s_brightnessSaved = false;

void platformSetMaxBrightness() {
    runOnMainThread(^{
        if (!s_brightnessSaved) {
            s_savedBrightness = [UIScreen mainScreen].brightness;
            s_brightnessSaved = true;
        }
        [UIScreen mainScreen].brightness = 1.0;
    });
}

void platformRestoreBrightness() {
    // 5WHY: guard must be inside the runOnMainThread block — if it's on the
    // calling thread, a concurrent call can slip past before the block
    // executes, causing a double-restore or stale-value TOCTOU.
    runOnMainThread(^{
        if (!s_brightnessSaved) return;
        [UIScreen mainScreen].brightness = s_savedBrightness;
        s_brightnessSaved = false;
    });
}
