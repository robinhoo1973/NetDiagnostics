// =============================================================================
// IosWiFiHelper.h — iOS WiFi auth + SSID retrieval (declarations)
// =============================================================================
#ifndef IOS_WIFI_HELPER_H
#define IOS_WIFI_HELPER_H

#include <QString>

// Call once at app startup (main thread safe).
// iOS 13+: requests CLLocationManager WhenInUse authorization.
// No-op on non-iOS platforms.
void iosRequestWiFiAuthorization();

// Returns the current WiFi SSID, or empty QString if unavailable
// (e.g. location permission denied, airplane mode, or non-iOS).
// Uses CNCopyCurrentNetworkInfo with Apple privacy-sentinel filtering.
QString iosCopyWiFiSSID();

#endif // IOS_WIFI_HELPER_H
