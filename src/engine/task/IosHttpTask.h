// =============================================================================
// IosHttpTask.h — Declarations for iOS HTTP diagnostics via NSURLSession
//
// Include this header (not IosHttpTask.mm) in other translation units.
// IosHttpTask.mm is compiled separately as its own translation unit.
// =============================================================================
#pragma once
#ifdef PLATFORM_IOS

#include <QString>
#include "models/DiagnosticResult.h"
#include "models/DiagId.h"

DiagnosticResult iosHttpDiagnostic(DiagId id, const QString& target);

#endif // PLATFORM_IOS
