// G5CommonIos.h — iOS NSURLSession HTTP diagnostics
#pragma once
#include "models/DiagnosticResult.h"
#include "models/DiagId.h"
DiagnosticResult iosHttpDiagnostic(DiagId id, const QString& target);
