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

// ── Orientation lock ────────────────────────────────────────────────
// 5WHY: Qt/QML doesn't own UIViewController — can't override
// supportedInterfaceOrientations.
//
// IMPLEMENTATION GAP (2026-07-25): These stubs mean orientation is NEVER
// locked on iOS during capture.  The intended QML-based solution
// (ApplicationWindow.contentOrientation) requires work in main.qml /
// AppContent.qml to save/restore the orientation property.  Until that
// QML wiring is added, orientation lock is a no-op on iOS.
//
// These stubs exist so the cross-platform caller compiles without #ifdef.
void platformLockOrientation() {}
void platformUnlockOrientation() {}

void platformOpenFocusSettings() {
    // 5WHY: iOS does not allow programmatic Focus mode activation.
    // The best we can do is open Settings → Focus so the user can
    // manually enable it.
    //
    // App-Prefs:root=Focus is a private URL scheme that works on iOS 15-17
    // but may fail on iOS 18+ (Apple tightens private scheme enforcement).
    // canOpenURL: serves as a runtime guard — if it returns NO (iOS 18+
    // or App Review rejection), fall back to the app's own Settings page.
    NSURL* url = [NSURL URLWithString:@"App-Prefs:root=Focus"];
    if ([[UIApplication sharedApplication] canOpenURL:url]) {
        [[UIApplication sharedApplication] openURL:url
            options:@{} completionHandler:nil];
    } else {
        // Fallback: open the app's Settings bundle page.  This won't
        // navigate to Focus but at least gives the user access to system
        // settings where they can manually navigate.
        NSURL* fallbackUrl = [NSURL URLWithString:UIApplicationOpenSettingsURLString];
        if (fallbackUrl) {
            [[UIApplication sharedApplication] openURL:fallbackUrl
                options:@{} completionHandler:nil];
        }
    }
}
