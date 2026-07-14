// =============================================================================
// MacOSWifiHelper.mm — macOS WiFi SSID/BSSID via CoreWLAN (Objective-C++)
// =============================================================================
// 5WHY: CoreWLAN code was inline in G1WifiDiagnostics.cpp via objc_msgSend.
// Xcode 26.5 SDK did not expose Class/SEL types even with -x objective-c++,
// causing cryptic "expected ';' after expression" errors.  Extracting to a
// proper .mm file guarantees Objective-C++ compilation with full type support.
#if defined(__APPLE__) && !defined(PLATFORM_IOS)

#import <CoreWLAN/CoreWLAN.h>
#import <Foundation/Foundation.h>
#include <QString>

extern "C" {

QString macosWifiSsid() {
    @autoreleasepool {
        CWWiFiClient* client = [CWWiFiClient sharedWiFiClient];
        CWInterface* iface = client.interface;
        if (!iface || !iface.ssid) return QString();
        return QString::fromNSString(iface.ssid);
    }
}

QString macosWifiBssid() {
    @autoreleasepool {
        CWWiFiClient* client = [CWWiFiClient sharedWiFiClient];
        CWInterface* iface = client.interface;
        if (!iface || !iface.bssid) return QString();
        return QString::fromNSString(iface.bssid);
    }
}

} // extern "C"

#endif // __APPLE__ && !PLATFORM_IOS
