// IosNetworkInfo.h — iOS 网络信息平台层（路由/网关/DHCP/接口/蜂窝/WiFi）
// 5WHY (2026-08-22 模块边界纠错): 曾名 GatewayDhcpRouting——历轮追加使
// 内容远超网关/DHCP（接口枚举、CoreTelephony 蜂窝、WiFi 均入内）。
// 改名 IosNetworkInfo（伞式自描述），分区见 .mm 顶部横幅。
#pragma once
#if defined(PLATFORM_IOS)
#include <QString>
#include "Common/Model/DiagnosticResult.h"
#include "Common/Model/DiagId.h"
DiagnosticResult iosDefaultGatewayDiag(DiagId id);
DiagnosticResult iosDhcpDiag(DiagId id);
DiagnosticResult iosRoutingTableDiag(DiagId id);

// Interface helpers used by the cellular / WiFi panels.
// iosInterfaceIPv4: IPv4 assigned to an interface (e.g. "pdp_ip0", "en0"), or empty.
// iosGatewayForInterface: next-hop gateway routed via the interface, or empty.
// iosCellularIfaces: cellular-like interfaces (pdp/rmnet/wwan/ap/ipsec) with
// IPv4/IPv6 — network-layer connection facts (immune to CoreTelephony privacy
// placeholders since iOS 16.4+).
// iosCellularIPv4: cellular data IPv4 via superset fallback (pdp_ip0 first —
// historical behavior preserved — then interface enumeration, then the default
// route's cellular-like interface); writes the actual interface name to
// ifaceOut when found.
QString iosInterfaceIPv4(const QString& iface);
QString iosGatewayForInterface(const QString& iface);
QVariantList iosCellularIfaces();
QString iosCellularIPv4(QString* ifaceOut = nullptr);
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
#endif // PLATFORM_IOS
