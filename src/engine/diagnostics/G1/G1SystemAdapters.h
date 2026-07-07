#pragma once
#include "engine/diagnostics/GBase.h"
namespace G1G2G3Native {
DiagnosticResult networkAdapters(DiagId id);
DiagnosticResult nicAdvanced(DiagId id);
DiagnosticResult wifiDiagnostics(DiagId id);
DiagnosticResult wiredDiagnostics(DiagId id);
DiagnosticResult dhcpStatus(DiagId id);
DiagnosticResult ipConfiguration(DiagId id);
DiagnosticResult activeConnections(DiagId id);
DiagnosticResult cellularInfo(DiagId id);
}
