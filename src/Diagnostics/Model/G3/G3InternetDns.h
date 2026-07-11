#pragma once
#include "Diagnostics/Model/GBase.h"
namespace G1G2G3Native {
DiagnosticResult netskopeStatus(DiagId id);
DiagnosticResult dnsServers(DiagId id);
DiagnosticResult dnsCache(DiagId id);
DiagnosticResult dnsPollution(DiagId id);
DiagnosticResult speedTest(DiagId id);
}
