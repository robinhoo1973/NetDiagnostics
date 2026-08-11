#pragma once
#include "Diagnostics/Model/GBase.h"
namespace SystemDiagnostics {

DiagnosticResult dnsServers(DiagId id);
DiagnosticResult dnsCache(DiagId id);
DiagnosticResult dnsIntegrity(DiagId id);
DiagnosticResult geoIPLoc(DiagId id);
DiagnosticResult internetConnectivity(DiagId id);
QString detectCountry(int timeoutMs = 5000);

} // namespace SystemDiagnostics
