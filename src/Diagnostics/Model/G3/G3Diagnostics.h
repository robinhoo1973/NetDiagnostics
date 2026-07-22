#pragma once
#include "Diagnostics/Model/GBase.h"
namespace G1G2G3Native {

DiagnosticResult netskopeStatus(DiagId id);
DiagnosticResult dnsServers(DiagId id);
DiagnosticResult dnsCache(DiagId id);
DiagnosticResult dnsIntegrity(DiagId id);
DiagnosticResult geoIPLoc(DiagId id);
DiagnosticResult internetConnectivity(DiagId id);
QString detectCountry(int timeoutMs = 5000);

} // namespace G1G2G3Native
