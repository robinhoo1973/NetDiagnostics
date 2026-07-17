#pragma once
#include "Diagnostics/Model/GBase.h"
namespace G1G2G3Native {
// Forward-declare SpeedTest for use in vpnStatus (defined in G3InternetSpeedTest.cpp)
class SpeedTest { public: static QString detectCountry(int = 3000); };

DiagnosticResult netskopeStatus(DiagId id);
DiagnosticResult dnsServers(DiagId id);
DiagnosticResult dnsCache(DiagId id);
DiagnosticResult dnsPollution(DiagId id);
DiagnosticResult vpnStatus(DiagId id);
DiagnosticResult speedTest(DiagId id);
}
