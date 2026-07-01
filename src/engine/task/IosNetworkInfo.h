// =============================================================================
// IosNetworkInfo.h — Declarations for iOS network info diagnostics
//
// Include this header (not IosNetworkInfo.mm) in other translation units.
// IosNetworkInfo.mm is compiled separately as its own translation unit.
// =============================================================================
#pragma once
#ifdef PLATFORM_IOS

#include <QString>
#include "models/DiagnosticResult.h"
#include "models/DiagId.h"

DiagnosticResult iosDefaultGatewayDiag(DiagId id);
DiagnosticResult iosDhcpDiag(DiagId id);

#endif // PLATFORM_IOS
