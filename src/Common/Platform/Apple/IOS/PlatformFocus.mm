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

static bool s_focusEnabled = false;

bool platformEnableFocusMode() {
    if (s_focusEnabled) return true;

    // 5WHY: dispatch_sync on the main queue from the main thread deadlocks.
    // Check isMainThread first — if already on main, execute directly.
    // CaptureOrchestrator always calls this from the Qt main thread.
    void (^block)(void) = ^{
        // Mute audio (best-effort — this silences app audio, not ringer)
        [[AVAudioSession sharedInstance] setActive:NO error:nil];
    };

    if ([NSThread isMainThread]) {
        block();
    } else {
        dispatch_sync(dispatch_get_main_queue(), block);
    }

    s_focusEnabled = true;
    return true;
}

void platformDisableFocusMode() {
    if (!s_focusEnabled) return;

    // 5WHY: dispatch_sync on the main queue from the main thread deadlocks.
    void (^block)(void) = ^{
        // Reactivate audio session (reverses the setActive:NO in enable)
        [[AVAudioSession sharedInstance] setActive:YES error:nil];
    };

    if ([NSThread isMainThread]) {
        block();
    } else {
        dispatch_sync(dispatch_get_main_queue(), block);
    }

    s_focusEnabled = false;
}

bool platformIsFocusModeEnabled() {
    return s_focusEnabled;
}

// ── Brightness control for recording clarity ──────────────────────────────
// 5WHY: dispatch_sync on the main queue from the main thread deadlocks.
// Check isMainThread first — if already on the main thread, execute directly.
static CGFloat s_savedBrightness = -1.0;

void platformSetMaxBrightness() {
    if ([NSThread isMainThread]) {
        if (s_savedBrightness < 0) {
            s_savedBrightness = [UIScreen mainScreen].brightness;
        }
        [UIScreen mainScreen].brightness = 1.0;
    } else {
        dispatch_sync(dispatch_get_main_queue(), ^{
            if (s_savedBrightness < 0) {
                s_savedBrightness = [UIScreen mainScreen].brightness;
            }
            [UIScreen mainScreen].brightness = 1.0;
        });
    }
}

void platformRestoreBrightness() {
    if (s_savedBrightness < 0) return;
    if ([NSThread isMainThread]) {
        [UIScreen mainScreen].brightness = s_savedBrightness;
        s_savedBrightness = -1.0;
    } else {
        dispatch_sync(dispatch_get_main_queue(), ^{
            [UIScreen mainScreen].brightness = s_savedBrightness;
            s_savedBrightness = -1.0;
        });
    }
}
