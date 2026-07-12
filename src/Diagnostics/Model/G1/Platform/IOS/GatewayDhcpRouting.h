// IosNetworkInfo.h — iOS network info (gateway, DHCP)
#pragma once
#include <QString>
#include "Common/Model/DiagnosticResult.h"
#include "Common/Model/DiagId.h"
DiagnosticResult iosDefaultGatewayDiag(DiagId id);
DiagnosticResult iosDhcpDiag(DiagId id);
DiagnosticResult iosRoutingTableDiag(DiagId id);
// 5WHY: Wired after MVC refactoring — iosCellularInfo() existed but had
// no DiagnosticResult wrapper.  Now properly available to TaskFactory.
DiagnosticResult iosCellularDiag(DiagId id);

// Interface helpers used by the cellular / WiFi panels.
// iosInterfaceIPv4: IPv4 assigned to an interface (e.g. "pdp_ip0", "en0"), or empty.
// iosGatewayForInterface: next-hop gateway routed via the interface, or empty.
QString iosInterfaceIPv4(const QString& iface);
QString iosGatewayForInterface(const QString& iface);
// =============================================================================
// IosWiFiHelper.h — iOS WiFi auth + SSID + Cellular retrieval (declarations)
// =============================================================================
#if !defined(IOS_WIFI_HELPER_H)
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
