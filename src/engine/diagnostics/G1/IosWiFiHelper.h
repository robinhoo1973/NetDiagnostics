// =============================================================================
// IosWiFiHelper.h — iOS WiFi auth + SSID + Cellular retrieval (declarations)
// =============================================================================
#ifndef IOS_WIFI_HELPER_H
#define IOS_WIFI_HELPER_H

#include <QString>
#include <QVariantMap>

// Call once at app startup (main thread safe).
// iOS 13+: requests CLLocationManager WhenInUse authorization.
// No-op on non-iOS platforms.
void iosRequestWiFiAuthorization();

// Returns the current WiFi SSID, or empty QString if unavailable
// (e.g. location permission denied, airplane mode, or non-iOS).
QString iosCopyWiFiSSID();

// Returns WiFi network information: ssid, bssid (MAC address), and diagnostics
// Keys: "ssid", "bssid", "error" (if applicable)
// Empty map if unavailable or non-iOS.
QVariantMap iosWiFiInfo();

// Returns cellular network info: carrierName, radioAccess, countryCode, networkCode, etc.
// Empty map if no cellular service or non-iOS.
QVariantMap iosCellularInfo();

#endif // IOS_WIFI_HELPER_H
