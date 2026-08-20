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
#else
#if defined(PLATFORM_ANDROID)
static constexpr unsigned kCurrentPlatformFlag = PlatformFlag::PF_Android;
#else
static constexpr unsigned kCurrentPlatformFlag = PlatformFlag::PF_Desktop;
#endif
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

bool AdapterRegistry::verifyAllDiagIds() {
    bool ok = true;
    int overridden = 0;
    for (DiagId id : allDiagIds()) {
        if (!isSchedulable(id)) continue;                 // deprecated slot
        if (!anyRunnable(id)) {
            // 5WHY (复核 2026-08-20 白名单派生): 曾手维护 per-id 白名单
            // switch（Netskope 恢复时漏加即 iOS qFatal、Android 注销后同样
            // 漏同步即 Android qFatal）——平台缺席的合法性应由 meta 平台
            // 声明派生：本平台不在声明内即合法缺席，声明含本平台却无
            // 适配器才是注册缺口。手写 switch 删除，缺口自动封死。
            if (!(diagnosticMeta(id).platforms & kCurrentPlatformFlag)) continue;
            Logger::instance().error(
                QStringLiteral("verifyAllDiagIds: DiagId %1 has no adapter for current platform")
                    .arg(static_cast<int>(id)));
            ok = false;
            continue;
        }
        // §6.1（5WHY DIAG-1 单一事实来源）：registry 是 platforms 的权威——
        // 启动时把注册联合值覆写进 meta 视图（不再仅告警），消除两套表漂移。
        const unsigned declared = registeredPlatforms(id);
        if (declared != diagnosticMeta(id).platforms) {
            setMetaPlatformOverride(id, declared);
            ++overridden;
        }
    }
    // 启动日志仅汇总一行（避免每 id 一行噪音；平台编译裁剪导致的覆写属预期）
    if (overridden > 0)
        Logger::instance().event(
            QStringLiteral("verifyAllDiagIds: registry overrode meta.platforms for %1 diag(s)")
                .arg(overridden));
    return ok;
}
