// =============================================================================
// PlatformFocus.mm — iOS focus / Do-Not-Disturb implementation
// =============================================================================
// iOS does not expose a public API to programmatically enable DND/Focus mode.
// As a best-effort alternative:
//   1. Lower screen brightness to near-zero to reduce visual distraction
//   2. Mute the device via Ringer switch simulation (AVAudioSession)
//   3. Prevent screen lock (already handled by PlatformKeepAwake)
//
// True Focus mode activation requires the Settings app or a system extension
// (not available to third-party apps without MDM).
// =============================================================================
#include "Common/Platform/PlatformFocus.h"
#import <UIKit/UIKit.h>
#import <AVFoundation/AVFoundation.h>

static bool s_focusEnabled = false;
static CGFloat s_originalBrightness = -1.0;

bool platformEnableFocusMode() {
    if (s_focusEnabled) return true;

    dispatch_sync(dispatch_get_main_queue(), ^{
        // Save original brightness
        s_originalBrightness = [UIScreen mainScreen].brightness;

        // Lower brightness to near-zero to suppress visual distractions
        [UIScreen mainScreen].brightness = 0.05;

        // Mute audio (best-effort — this silences app audio, not ringer)
        [[AVAudioSession sharedInstance] setActive:NO error:nil];
    });

    s_focusEnabled = true;
    return true;
}

void platformDisableFocusMode() {
    if (!s_focusEnabled) return;

    dispatch_sync(dispatch_get_main_queue(), ^{
        // Restore original brightness
        if (s_originalBrightness >= 0.0) {
            [UIScreen mainScreen].brightness = s_originalBrightness;
            s_originalBrightness = -1.0;
        }
    });

    s_focusEnabled = false;
}

bool platformIsFocusModeEnabled() {
    return s_focusEnabled;
}

// ── Brightness control for recording clarity ──────────────────────────────
static CGFloat s_savedBrightness = -1.0;

void platformSetMaxBrightness() {
    dispatch_sync(dispatch_get_main_queue(), ^{
        if (s_savedBrightness < 0) {
            s_savedBrightness = [UIScreen mainScreen].brightness;
        }
        [UIScreen mainScreen].brightness = 1.0;
    });
}

void platformRestoreBrightness() {
    if (s_savedBrightness < 0) return;
    dispatch_sync(dispatch_get_main_queue(), ^{
        [UIScreen mainScreen].brightness = s_savedBrightness;
        s_savedBrightness = -1.0;
    });
}
