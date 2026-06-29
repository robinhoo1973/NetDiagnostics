// =============================================================================
// IosWiFiHelper.mm — iOS WiFi authorization + SSID retrieval (Objective-C++)
// =============================================================================
// iOS 26 SDK removed CNCopyCurrentNetworkInfo (deprecated since iOS 14).
// Migrated to NEHotspotNetwork fetchCurrentWithCompletionHandler:.
//
// Functions:
//   iosRequestWiFiAuthorization()  — call once at app startup (main thread)
//   iosCopyWiFiSSID()              — get current WiFi SSID, or empty QString
//
// Requires: com.apple.developer.networking.wifi-info entitlement
//           + location authorization on iOS 14+
// =============================================================================

#ifdef PLATFORM_IOS

#include <QString>
#import <NetworkExtension/NetworkExtension.h>
#import <CoreLocation/CoreLocation.h>

// ── Authorization ────────────────────────────────────────────────────────────

void iosRequestWiFiAuthorization()
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            iosRequestWiFiAuthorization();
        });
        return;
    }

    if (@available(iOS 13.0, *)) {
        static CLLocationManager* mgr = nil;
        if (!mgr) {
            mgr = [[CLLocationManager alloc] init];
            [mgr requestWhenInUseAuthorization];
        }
    }
}

// ── SSID retrieval ───────────────────────────────────────────────────────────
// iOS 26 SDK: CNCopyCurrentNetworkInfo removed — use NEHotspotNetwork instead.
// fetchCurrentWithCompletionHandler: is async; we bridge to sync with a semaphore.
// Without the entitlement, the callback returns nil → result is empty QString.

QString iosCopyWiFiSSID()
{
    __block NSString* ssid = nil;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);

    if (@available(iOS 14.0, *)) {
        [NEHotspotNetwork fetchCurrentWithCompletionHandler:^(NEHotspotNetwork* _Nullable network) {
            if (network && network.SSID.length > 0) {
                ssid = [network.SSID copy];
            }
            dispatch_semaphore_signal(sem);
        }];
        // 3-second timeout — if entitlement is missing, callback fires quickly with nil
        dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC));
    }

    if (ssid && ssid.length > 0) {
        QString result = QString::fromNSString(ssid);
        return result;
    }
    return QString();
}

#endif // PLATFORM_IOS
