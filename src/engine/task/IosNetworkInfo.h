// IosNetworkInfo.h — iOS network info (gateway, DHCP)
#pragma once
#include "models/DiagnosticResult.h"
#include "models/DiagId.h"
DiagnosticResult iosDefaultGatewayDiag(DiagId id);
DiagnosticResult iosDhcpDiag(DiagId id);
