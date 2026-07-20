#pragma once
#include "Diagnostics/Model/GBase.h"
#include "Diagnostics/Model/G3/G3InternetConnectivity.h"  // SpeedTest class
namespace G1G2G3Native {

DiagnosticResult netskopeStatus(DiagId id);
DiagnosticResult dnsServers(DiagId id);
DiagnosticResult dnsCache(DiagId id);
DiagnosticResult dnsPollution(DiagId id);
DiagnosticResult geoIPLoc(DiagId id);
// internetConnectivity → G3InternetConnectivity.h

} // namespace G1G2G3Native
