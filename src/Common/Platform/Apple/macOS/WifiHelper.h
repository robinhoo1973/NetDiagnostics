// =============================================================================
// WifiHelper.h — macOS WiFi SSID/BSSID via CoreWLAN (declarations)
// =============================================================================
#pragma once
#include <QString>

// Returns current WiFi SSID, or empty QString if unavailable.
// Requires CoreWLAN framework (macOS only, no-op on other platforms).
QString macosWifiSsid();

// Returns current WiFi BSSID (MAC), or empty QString if unavailable.
QString macosWifiBssid();
