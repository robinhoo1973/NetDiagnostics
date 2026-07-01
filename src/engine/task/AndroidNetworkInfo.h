#pragma once
// =============================================================================
// AndroidNetworkInfo.h — Declarations for Android JNI diagnostic entry points
//
// Implementation is in AndroidNetworkInfo.cpp, compiled separately by CMake.
// =============================================================================
#ifdef PLATFORM_ANDROID

#include <QString>
#include "models/DiagnosticResult.h"
#include "models/DiagId.h"

// Network type string (WiFi / LTE / 5G / etc.)
QString androidNetworkTypeInfo();

// G1 diagnostics
DiagnosticResult androidWifiDiag(DiagId id);
DiagnosticResult androidCellularDiag(DiagId id);
DiagnosticResult androidDhcpDiag(DiagId id);

// G2 diagnostics
DiagnosticResult androidGatewayDiag(DiagId id);

// G3/G4 DNS
QString androidDnsResolve(const QString& host, int timeoutMs);
DiagnosticResult androidDnsDiag(DiagId id, const QString& target);

// G5 HTTP
DiagnosticResult androidHttpDiag(DiagId id, const QString& target);

#endif // PLATFORM_ANDROID
