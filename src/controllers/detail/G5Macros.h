// =============================================================================
// G5Macros.h — Shared G5 dispatch macro for IosHttpClient / DesktopHttpClient
// =============================================================================
#pragma once
#include "engine/diagnostic/G5WebsiteUrl.h"

#ifdef NO_CURL
#define G5_METHOD(cls, method, diagId) \
    DiagnosticResult cls::method(const QString&) { \
        return DiagnosticResult::skipped(DiagId::diagId, QStringLiteral("G5 not available (no curl)")); \
    }
#else
#define G5_METHOD(cls, method, diagId) \
    DiagnosticResult cls::method(const QString& t) { return G5WebsiteUrl::method(t); }
#endif
