// IosDnsTask.h — iOS CFHost DNS resolution
#pragma once
#include "Common/Model/DiagnosticResult.h"
#include "Common/Model/DiagId.h"
DiagnosticResult iosDnsResolve(DiagId id, const QString& target, int timeoutMs = 3000);
