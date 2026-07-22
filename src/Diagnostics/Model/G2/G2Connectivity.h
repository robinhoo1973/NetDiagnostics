#pragma once
#include "Diagnostics/Model/GBase.h"
namespace SystemDiagnostics {
DiagnosticResult routingTable(DiagId id);
DiagnosticResult arpTable(DiagId id);
DiagnosticResult networkProfile(DiagId id);
DiagnosticResult tcpSettings(DiagId id);
DiagnosticResult defaultGateway(DiagId id);
DiagnosticResult proxySettings(DiagId id);
}
