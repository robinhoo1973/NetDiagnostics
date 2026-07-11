#pragma once
#include "Diagnostics/Model/GBase.h"
namespace G1G2G3Native {
DiagnosticResult routingTable(DiagId id);
DiagnosticResult arpTable(DiagId id);
DiagnosticResult networkProfile(DiagId id);
DiagnosticResult tcpSettings(DiagId id);
DiagnosticResult defaultGateway(DiagId id);
DiagnosticResult proxySettings(DiagId id);
}
