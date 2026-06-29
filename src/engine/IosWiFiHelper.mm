// =============================================================================
// IosWiFiHelper.mm — iOS WiFi authorization + SSID retrieval (Objective-C++)
// =============================================================================
// Separated from G1G2G3Native.cpp because CNCopyCurrentNetworkInfo is
// deprecated in iOS 14+ and NEHotspotNetwork requires Objective-C.
//
// Functions:
//   iosRequestWiFiAuthorization()  — call once at app startup (main thread)
//   iosCopyWiFiSSID()              — get current WiFi SSID, or empty QString
// =============================================================================

#ifdef PLATFORM_IOS

#include <QString>
// SystemConfiguration: prefer module import in ObjC++ mode; fallback to header
@import SystemConfiguration;
#import <NetworkExtension/NetworkExtension.h>
#import <CoreLocation/CoreLocation.h>

// ── Authorization ────────────────────────────────────────────────────────────
// iOS 14+: CNCopyCurrentNetworkInfo requires location permission OR the
// app be a VPN/NEHotspotHelper participant. We request WhenInUse authorization.
// Must be called from the main thread.

void iosRequestWiFiAuthorization()
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            iosRequestWiFiAuthorization();
        });
        return;
    }

    // iOS 13+: CLLocationManager required for WiFi SSID access
    if (@available(iOS 13.0, *)) {
        static CLLocationManager* mgr = nil;
        if (!mgr) {
            mgr = [[CLLocationManager alloc] init];
            [mgr requestWhenInUseAuthorization];
        }
    }
}

// ── SSID retrieval ───────────────────────────────────────────────────────────
// Uses CNCopyCurrentNetworkInfo (iOS 9+, deprecated in 14+ but still functional).
// When CNCopyCurrentNetworkInfo is removed, migrate to NEHotspotNetwork
// fetchCurrentWithCompletionHandler: with a Qt signal bridge.
//
// Requires: com.apple.developer.networking.wifi-info entitlement
//           + location authorization on iOS 14+ (or approved VPN/MDM use case)

QString iosCopyWiFiSSID()
{
    QString result;

    // CNCopyCurrentNetworkInfo (works iOS 9+, deprecated in 14+ but functional)
    CFArrayRef ifList = CNCopySupportedInterfaces();
    if (ifList && CFArrayGetCount(ifList) > 0) {
        CFDictionaryRef info = CNCopyCurrentNetworkInfo(CFArrayGetValueAtIndex(ifList, 0));
        if (info) {
            CFStringRef ssid = (CFStringRef)CFDictionaryGetValue(info, kCNNetworkInfoKeySSID);
            if (ssid) {
                // Apple privacy sentinel: when location permission is denied
                // on iOS 14+, the API returns "Wi-Fi" instead of the real SSID.
                CFStringRef sentinel = CFSTR("Wi-Fi");
                if (CFStringCompare(ssid, sentinel, 0) != kCFCompareEqualTo) {
                    char buf[128] = {};
                    if (CFStringGetCString(ssid, buf, sizeof(buf), kCFStringEncodingUTF8))
                        result = QString::fromUtf8(buf);
                }
            }
            CFRelease(info);
        }
        CFRelease(ifList);
    }

    return result;
}

#endif // PLATFORM_IOS
