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
#include <QString>
#include <QtDebug>
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
        // 5WHY: An active VoIP call or higher-priority audio session can
        // cause setActive:NO to fail.  Capture the NSError so failure is
        // visible in the console log — otherwise the capture proceeds
        // assuming audio is muted when it is not.
        NSError* err = nil;
        BOOL ok = [[AVAudioSession sharedInstance] setActive:NO error:&err];
        if (!ok) {
            qWarning() << "PlatformFocus: cannot deactivate audio session — "
                           "app audio may not be muted during capture:"
                        << QString::fromNSString(err.localizedDescription);
        }
    });

    s_focusEnabled = true;
    return true;
}

bool platformDisableFocusMode() {
    if (!s_focusEnabled) return false;

    runOnMainThread(^{
        NSError* err = nil;
        [[AVAudioSession sharedInstance] setActive:YES error:&err];
        if (err) {
            qWarning() << "PlatformFocus: error reactivating audio session:"
                        << QString::fromNSString(err.localizedDescription);
        }
    });

    s_focusEnabled = false;
    return true;
}

bool platformIsFocusModeEnabled() {
    return s_focusEnabled;
}

// ── Brightness control for recording clarity ──────────────────────────────
// 5WHY: UIScreen.brightness is clamped to [0.0, 1.0], so the sentinel -1.0
// cannot overlap with any valid brightness value.  Unlike Android where
// -1.0f collides with BRIGHTNESS_OVERRIDE_NONE, iOS can derive "has been
// saved" directly from the float — no separate boolean needed.
static CGFloat s_savedBrightness = -1.0;

bool platformSetMaxBrightness() {
    runOnMainThread(^{
        if (s_savedBrightness < 0.0) {
            s_savedBrightness = [UIScreen mainScreen].brightness;
        }
        [UIScreen mainScreen].brightness = 1.0;
    });
    return true;
}

bool platformRestoreBrightness() {
    if (s_savedBrightness < 0.0) return false;
    // 5WHY: The guard check above ensures we only dispatch when there is
    // work to do.  The block itself must be serialised on the main thread
    // to avoid TOCTOU with a concurrent platformSetMaxBrightness.
    runOnMainThread(^{
        if (s_savedBrightness < 0.0) return;  // re-check under main-thread serialisation
        [UIScreen mainScreen].brightness = s_savedBrightness;
        s_savedBrightness = -1.0;  // reset sentinel
    });
    return true;
}

// ── Orientation lock ────────────────────────────────────────────────
// 5WHY: UIKit orientation lock requires overriding
// supportedInterfaceOrientations on the root UIViewController — which Qt
// owns, not us.  Swizzling or dynamic subclassing carries unacceptable
// risk of breaking Qt's own orientation handling.
//
// The correct cross-platform approach uses QML:
//   1. In onStateChanged(CountdownToStart): save ApplicationWindow.contentOrientation
//      to a property, then set it to the current device orientation.
//   2. In restoreSystemState(): restore the saved contentOrientation.
//
// This is implemented in CaptureOrchestrator which calls these stubs.
// The CaptureOrchestrator already checks the return value and emits a
// qWarning when orientation lock is unavailable — the QML overlay tells
// the user not to rotate the device, but on iOS this is advisory only
// until the QML contentOrientation wiring is added.
bool platformLockOrientation() { return false; }
bool platformUnlockOrientation() { return false; }

void platformOpenFocusSettings() {
    // 5WHY: iOS does not allow programmatic Focus mode activation.
    // The best we can do is open Settings → Focus so the user can
    // manually enable it.
    //
    // App-Prefs:root=Focus is a private URL scheme that works on iOS 15-17
    // but may fail on iOS 18+ (Apple tightens private scheme enforcement).
    // canOpenURL: serves as a runtime guard — if it returns NO (iOS 18+
    // or App Review rejection), fall back to the app's own Settings page.
    //
    // 5WHY: UIApplication APIs must be called from the main thread.
    // Use runOnMainThread (same as every other function in this file)
    // so the function is safe from any calling thread.
    runOnMainThread(^{
        NSURL* url = [NSURL URLWithString:@"App-Prefs:root=Focus"];
        if ([[UIApplication sharedApplication] canOpenURL:url]) {
            [[UIApplication sharedApplication] openURL:url
                options:@{} completionHandler:nil];
        } else {
            // 5WHY: On iOS 18+, Apple blocks App-Prefs: private URL schemes.
            // canOpenURL: returns NO statically without LSApplicationQueriesSchemes
            // in Info.plist (which risks App Store rejection).  The fallback
            // opens the app's own Settings page — not the Focus page — so the
            // user cannot enable/disable DND from there.  Log a warning so
            // the developer knows the DND hint was non-functional on this device.
            qWarning() << "PlatformFocus: App-Prefs:root=Focus not available — "
                           "falling back to app Settings (Focus/DND not accessible)";
            NSURL* fallbackUrl = [NSURL URLWithString:UIApplicationOpenSettingsURLString];
            if (fallbackUrl) {
                [[UIApplication sharedApplication] openURL:fallbackUrl
                    options:@{} completionHandler:nil];
            }
        }
    });
}
