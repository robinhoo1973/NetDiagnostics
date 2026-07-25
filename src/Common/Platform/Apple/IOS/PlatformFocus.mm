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

// ── Orientation lock using UIDevice KVO ───────────────────────────────────
// 5WHY: UIDevice.orientation is declared readonly but is writable via KVO.
// Setting the orientation to the current value effectively locks it because
// iOS will not change the orientation while a fixed KVO value is set and
// beginGeneratingDeviceOrientationNotifications is active.  When we set
// orientation to Unknown and end notifications, auto-rotation resumes.
//
// Apple has reviewed apps using this KVO technique since ~iOS 6 and has
// not rejected them solely for this pattern.
static UIInterfaceOrientation s_savedOrientation = (UIInterfaceOrientation)-1;

bool platformLockOrientation() {
    [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];

    UIDeviceOrientation currentOrientation = [UIDevice currentDevice].orientation;

    // 5WHY: If the device is flat or sensors haven't calibrated, the
    // orientation may be Unknown.  Fall back to the status bar orientation
    // which reflects the current UI layout.
    if (currentOrientation == UIDeviceOrientationUnknown) {
        switch ([UIApplication sharedApplication].statusBarOrientation) {
            case UIInterfaceOrientationPortrait:
                currentOrientation = UIDeviceOrientationPortrait; break;
            case UIInterfaceOrientationLandscapeLeft:
                currentOrientation = UIDeviceOrientationLandscapeLeft; break;
            case UIInterfaceOrientationLandscapeRight:
                currentOrientation = UIDeviceOrientationLandscapeRight; break;
            case UIInterfaceOrientationPortraitUpsideDown:
                currentOrientation = UIDeviceOrientationPortraitUpsideDown; break;
            default:
                currentOrientation = UIDeviceOrientationPortrait; break;
        }
    }

    runOnMainThread(^{
        if (s_savedOrientation == (UIInterfaceOrientation)-1) {
            s_savedOrientation = (UIInterfaceOrientation)currentOrientation;
        }
        [[UIDevice currentDevice] setValue:@(currentOrientation) forKey:@"orientation"];
    });

    return true;
}

bool platformUnlockOrientation() {
    if (s_savedOrientation == (UIInterfaceOrientation)-1) return false;

    runOnMainThread(^{
        [[UIDevice currentDevice] setValue:@(UIDeviceOrientationUnknown) forKey:@"orientation"];
        s_savedOrientation = (UIInterfaceOrientation)-1;
    });

    dispatch_async(dispatch_get_main_queue(), ^{
        [[UIDevice currentDevice] endGeneratingDeviceOrientationNotifications];
    });

    return true;
}

void platformOpenFocusSettings() {
    runOnMainThread(^{
        NSURL* url = [NSURL URLWithString:@"App-Prefs:root=Focus"];
        if ([[UIApplication sharedApplication] canOpenURL:url]) {
            [[UIApplication sharedApplication] openURL:url
                options:@{} completionHandler:nil];
        } else {
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
