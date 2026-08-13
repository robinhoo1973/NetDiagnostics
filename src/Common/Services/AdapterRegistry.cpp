// =============================================================================
// AdapterRegistry.cpp — Registration table + selection + startup invariant
// =============================================================================
#include "Common/Services/PlatformAdapter.h"
#include "Common/Services/Logger.h"
#include "Common/Model/DiagnosticMeta.h"

#include <QMap>
#include <QVector>

// ── Current platform (compile-time, DIAG-12) ───────────────────────────────
#if defined(PLATFORM_IOS)
static constexpr unsigned kCurrentPlatformFlag = PlatformFlag::PF_IOS;
#elif defined(PLATFORM_ANDROID)
static constexpr unsigned kCurrentPlatformFlag = PlatformFlag::PF_Android;
#else
static constexpr unsigned kCurrentPlatformFlag = PlatformFlag::PF_Desktop;
#endif

// ── Meyer's singleton storage (DIAG-2: initialization-order safe) ─────────
struct AdapterRegistry::Impl {
    QMap<DiagId, QVector<PlatformAdapter>> byId;
};
AdapterRegistry::Impl& AdapterRegistry::impl() {
    static Impl s_impl;   // function-local static — no SIOF
    return s_impl;
}

void AdapterRegistry::registerAdapters(DiagId id, std::initializer_list<PlatformAdapter> adapters) {
    auto& v = impl().byId[id];
    for (const auto& a : adapters) v.append(a);
}

void AdapterRegistry::registerAdapters(DiagId id, const QVector<PlatformAdapter>& adapters) {
    auto& v = impl().byId[id];
    v += adapters;
}

const PlatformAdapter* AdapterRegistry::select(DiagId id, const QString& schemeLower) {
    const auto it = impl().byId.constFind(id);
    if (it == impl().byId.constEnd()) return nullptr;
    for (const auto& a : it.value()) {
        if (!(a.platforms & kCurrentPlatformFlag)) continue;
        if (schemeLower.isEmpty() || a.scheme.matches(schemeLower)) return &a;
    }
    return nullptr;
}

bool AdapterRegistry::anyRunnable(DiagId id) { return select(id) != nullptr; }

unsigned AdapterRegistry::registeredPlatforms(DiagId id) {
    unsigned flags = 0;
    const auto it = impl().byId.constFind(id);
    if (it == impl().byId.constEnd()) return 0;
    for (const auto& a : it.value()) flags |= a.platforms;
    return flags;
}

// NEW-1: iOS-sandbox-impossible set (DiagCapability manifest).  Everything
// else is registered on all three platforms.
static bool unsupportedOnCurrentPlatform(DiagId id) {
#if defined(PLATFORM_IOS)
    switch (id) {
        case DiagId::G1NicAdvanced:
        case DiagId::G1WiredDiagnostics:
        case DiagId::G1ActiveConnections:
        case DiagId::G2TcpSettings:
        case DiagId::G2ArpTable:
        case DiagId::G2ProxySettings:
        case DiagId::G3DnsCache:
            return true;
        default:
            return false;
    }
#else
    Q_UNUSED(id);
    return false;
#endif
}

bool AdapterRegistry::verifyAllDiagIds() {
    bool ok = true;
    for (DiagId id : allDiagIds()) {
        if (!isSchedulable(id)) continue;                 // deprecated slot
        if (!anyRunnable(id)) {
            if (unsupportedOnCurrentPlatform(id)) continue;  // legitimate whitelist
            Logger::instance().error(
                QStringLiteral("verifyAllDiagIds: DiagId %1 has no adapter for current platform")
                    .arg(static_cast<int>(id)));
            ok = false;
            continue;
        }
        // R2-3 (DIAG-1 single-source enforcement): meta.platforms must equal the
        // union of registered adapter platforms — catches drift between the two.
        const unsigned declared = registeredPlatforms(id);
        if (declared != diagnosticMeta(id).platforms) {
            Logger::instance().warn(
                QStringLiteral("verifyAllDiagIds: DiagId %1 meta.platforms (%2) != registry (%3)")
                    .arg(static_cast<int>(id)).arg(diagnosticMeta(id).platforms).arg(declared));
            ok = false;
        }
    }
    return ok;
}
