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
    __block UIDeviceOrientation currentOrientation = UIDeviceOrientationUnknown;

    runOnMainThread(^{
        // 5WHY: beginGeneratingDeviceOrientationNotifications is REQUIRED
        // before reading [UIDevice currentDevice].orientation — otherwise
        // the value is always UIDeviceOrientationUnknown.  Must be on the
        // main thread per Apple docs (UIDevice is not thread-safe).
        [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];

        currentOrientation = [UIDevice currentDevice].orientation;

        // 5WHY: If the device is flat or sensors haven't calibrated, the
        // orientation may be Unknown.  Fall back to the interface orientation
        // from the window scene (iOS 13+) or status bar (iOS 12 fallback).
        if (currentOrientation == UIDeviceOrientationUnknown) {
            UIInterfaceOrientation uiOrientation = UIInterfaceOrientationPortrait;
#if defined(__IPHONE_13_0)
            if (@available(iOS 13.0, *)) {
                // 5WHY: statusBarOrientation was deprecated in iOS 13.
                // Use windowScene.interfaceOrientation instead.
                UIWindowScene* scene = (UIWindowScene*)[UIApplication sharedApplication]
                    .connectedScenes.anyObject;
                if (scene) {
                    uiOrientation = scene.interfaceOrientation;
                }
            } else
#endif
            {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
                uiOrientation = [UIApplication sharedApplication].statusBarOrientation;
#pragma clang diagnostic pop
            }
            switch (uiOrientation) {
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

        if (s_savedOrientation == (UIInterfaceOrientation)-1) {
            s_savedOrientation = (UIInterfaceOrientation)currentOrientation;
        }
        // 5WHY: Force the device orientation via KVO.
        [[UIDevice currentDevice] setValue:@(currentOrientation) forKey:@"orientation"];
    });

    return true;
}

bool platformUnlockOrientation() {
    runOnMainThread(^{
        // 5WHY: Must read s_savedOrientation under the main-thread
        // serialisation to avoid a data race with platformLockOrientation.
        if (s_savedOrientation == (UIInterfaceOrientation)-1) return;

        [[UIDevice currentDevice] setValue:@(UIDeviceOrientationUnknown) forKey:@"orientation"];
        s_savedOrientation = (UIInterfaceOrientation)-1;
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
