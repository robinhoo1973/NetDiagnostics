// =============================================================================
// PlatformAdapter.h — Platform adapter + SchemeFilter + AdapterRegistry (§6)
//
// Per diag-execution-architecture-guide.md §6:
//  - SchemeFilter: include/exclude/通配 (NEW-2, replaces single-value scheme)
//  - PlatformAdapter: no devicePredicate (NEW-4 — device probes belong to
//    DeviceCapability)
//  - AdapterRegistry: Meyer's singleton, explicit registerAllAdapters() from
//    main() (DIAG-2 + A1 — no SIOF, no linker dead-strip)
// =============================================================================
#pragma once

#include "Common/Platform/PlatformFlags.h"
#include "Common/Model/DiagnosticResult.h"
#include <QString>
#include <QStringList>
#include <functional>
#include <initializer_list>

// ── RunContext (DIAG-3 + NEW-5) ────────────────────────────────────────────
// Cancellation + fine-grained progress channel handed to probe implementations.
// NEW-5: probes only call progress() — the callback must queue to the main
// thread before touching any QObject/UI state.
struct RunContext {
    std::atomic<bool>& cancelled;
    std::function<void(int pct, const QString& stage)> progress;
};

// ── SchemeFilter (NEW-2) ───────────────────────────────────────────────────
// include empty = wildcard (any scheme); exclude=true = match schemes NOT in
// include (e.g. G5ServiceBanner = non-http/https).
struct SchemeFilter {
    QStringList include;         // lower-case scheme names
    bool        exclude = false;

    bool matches(const QString& schemeLower) const {
        if (include.isEmpty()) return true;
        return exclude ? !include.contains(schemeLower) : include.contains(schemeLower);
    }
};

// ── PlatformAdapter ────────────────────────────────────────────────────────
struct PlatformAdapter {
    unsigned platforms;          // PlatformFlag::Flag
    const char* name;            // "Desktop"/"iOS"/"Android"
    SchemeFilter scheme = {};    // DIAG-4 + NEW-2
    std::function<DiagnosticResult(DiagId, const QString&, RunContext&)> run; // DIAG-3
};

// ── AdapterRegistry ────────────────────────────────────────────────────────
class AdapterRegistry {
public:
    static void registerAdapters(DiagId id, std::initializer_list<PlatformAdapter> adapters);
    static void registerAdapters(DiagId id, const QVector<PlatformAdapter>& adapters);
    static const PlatformAdapter* select(DiagId id, const QString& schemeLower = {});
    static bool anyRunnable(DiagId id);
    static bool verifyAllDiagIds();   // DIAG-2 startup invariant (main() calls)
    static unsigned registeredPlatforms(DiagId id); // NEW-1: derive meta.platforms

private:
    struct Impl;
    static Impl& impl();
};
