// =============================================================================
// WifiHelper.h — macOS WiFi SSID/BSSID via CoreWLAN (declarations)
// =============================================================================
#pragma once
#include <QString>

// 5WHY: Functions declared as extern "C" to prevent C++ name mangling.
// The .mm definitions are also extern "C", but the .cpp caller (G1WifiDiagnostics)
// includes this header as C++. Without extern "C", the caller expects
// mangled names but the .mm exports unmangled symbols → linker error.

#ifdef __cplusplus
extern "C" {
#endif

// Returns current WiFi SSID, or empty QString if unavailable.
// Requires CoreWLAN framework (macOS only, no-op on other platforms).
QString macosWifiSsid();

// Returns current WiFi BSSID (MAC), or empty QString if unavailable.
QString macosWifiBssid();

#ifdef __cplusplus
}
#endif
