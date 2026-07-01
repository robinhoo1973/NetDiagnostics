// =============================================================================
// IosDnsTask.h — Declarations for iOS DNS resolution via CFHost
//
// Include this header (not IosDnsTask.mm) in other translation units.
// IosDnsTask.mm is compiled separately as its own translation unit.
// =============================================================================
#pragma once
#ifdef PLATFORM_IOS

#include <QString>
#include "models/DiagnosticResult.h"
#include "models/DiagId.h"

DiagnosticResult iosDnsResolve(DiagId id, const QString& target, int timeoutMs);

#endif // PLATFORM_IOS
