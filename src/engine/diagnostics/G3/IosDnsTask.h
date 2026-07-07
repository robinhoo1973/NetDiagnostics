// IosDnsTask.h — iOS CFHost DNS resolution
#pragma once
#include "models/DiagnosticResult.h"
#include "models/DiagId.h"
DiagnosticResult iosDnsResolve(DiagId id, const QString& target, int timeoutMs = 3000);
