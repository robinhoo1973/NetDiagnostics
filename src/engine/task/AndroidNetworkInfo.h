// =============================================================================
// AndroidNetworkInfo.h — Declarations for native Android JNI diagnostics
//
// Include this header (not AndroidNetworkInfo.cpp) in other translation units.
// AndroidNetworkInfo.cpp is compiled separately as its own translation unit.
// =============================================================================
#pragma once
#ifdef PLATFORM_ANDROID

#include <QString>
#include "models/DiagnosticResult.h"
#include "models/DiagId.h"

QString androidNetworkTypeInfo();
DiagnosticResult androidWifiDiag(DiagId id);
DiagnosticResult androidCellularDiag(DiagId id);
DiagnosticResult androidDhcpDiag(DiagId id);
DiagnosticResult androidGatewayDiag(DiagId id);
QString androidDnsResolve(const QString& host, int timeoutMs);
DiagnosticResult androidDnsDiag(DiagId id, const QString& target);
DiagnosticResult androidHttpDiag(DiagId id, const QString& target);

#endif // PLATFORM_ANDROID
