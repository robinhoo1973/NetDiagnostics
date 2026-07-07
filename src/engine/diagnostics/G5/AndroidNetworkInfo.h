// AndroidNetworkInfo.h — Android native diagnostics
#pragma once
#include "models/DiagnosticResult.h"
#include "models/DiagId.h"
DiagnosticResult androidDnsDiag(DiagId id, const QString& target);
DiagnosticResult androidWifiDiag(DiagId id);
DiagnosticResult androidCellularDiag(DiagId id);
DiagnosticResult androidDhcpDiag(DiagId id);
DiagnosticResult androidGatewayDiag(DiagId id);
DiagnosticResult androidHttpDiag(DiagId id, const QString& target);
