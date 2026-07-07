// IosNetworkInfo.h — iOS network info (gateway, DHCP)
#pragma once
#include <QString>
#include "models/DiagnosticResult.h"
#include "models/DiagId.h"
DiagnosticResult iosDefaultGatewayDiag(DiagId id);
DiagnosticResult iosDhcpDiag(DiagId id);
DiagnosticResult iosRoutingTableDiag(DiagId id);

// Interface helpers used by the cellular / WiFi panels.
// iosInterfaceIPv4: IPv4 assigned to an interface (e.g. "pdp_ip0", "en0"), or empty.
// iosGatewayForInterface: next-hop gateway routed via the interface, or empty.
QString iosInterfaceIPv4(const QString& iface);
QString iosGatewayForInterface(const QString& iface);
