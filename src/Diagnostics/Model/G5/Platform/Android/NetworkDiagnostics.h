// =============================================================================
// G5Android.h — Android native diagnostic function declarations
//
// These functions are implemented via JNI in G5Common.cpp (PLATFORM_ANDROID).
// Separated from G5Common.h to keep shared C++ engine and platform code apart.
// =============================================================================
#pragma once

#include "Common/Model/DiagnosticResult.h"
#include "Common/Model/DiagId.h"

DiagnosticResult androidDnsDiag(DiagId id, const QString& target);
DiagnosticResult androidWifiDiag(DiagId id);
DiagnosticResult androidCellularDiag(DiagId id);
DiagnosticResult androidDhcpDiag(DiagId id);
DiagnosticResult androidGatewayDiag(DiagId id);
DiagnosticResult androidHttpDiag(DiagId id, const QString& target);
