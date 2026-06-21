// =============================================================================
// G4RemoteHost.h — DNS resolution, ping, traceroute, pathping, MTU discovery
// =============================================================================
#pragma once

#include <QString>
#include "models/DiagnosticResult.h"
#include "models/DiagId.h"

namespace G4RemoteHost {

// Extract hostname from target (URL, hostname, or IP with optional port)
QString extractHostname(const QString& target);

DiagnosticResult dnsResolution(const QString& target);
DiagnosticResult ping(const QString& target);
DiagnosticResult traceroute(const QString& target);
DiagnosticResult pathPing(const QString& target);
DiagnosticResult mtuDiscovery(const QString& target);

} // namespace G4RemoteHost
