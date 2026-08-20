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
#include <QHash>
#include <QMutex>
#include <functional>
#include <initializer_list>
#include <memory>

// ── RunSnapshot (5WHY 2026-08-20 每轮工具缓存) ────────────────────────────
// 每轮套件运行共享的工具输出缓存（通用，非平台专属）：probeDhcp/probeIpConfig
// 字段重叠（DHCP4.OPTION 与 IP4.DNS 出自同一次 `nmcli device show` 输出）——
// 池线程并行执行时曾每轮 spawn 两次 nmcli（各带 4s 超时）。键 = exe +
// args（SystemDiagnostics::cachedRunTool 生成），互斥惰性填充：同一命令
// 一轮只跑一次，其余探针复用文本。探针不得假设内容完整（空 = 命令缺失
// 或失败，自行回退到其它数据源）。
// 5WHY (复核 2026-08-21 串行化): 缓存表锁曾覆盖整个进程执行段——不同命令
// （nmcli/iw/mmcli -L）被同一把锁串行化，一轮最坏多付 ~10s。表锁只护
// 表；每键独立键锁（toolMutexes）串行化同键执行，异键并行。
struct RunSnapshot {
    QMutex mutex;                       // 仅护 toolOutputs/toolMutexes 两表
    QHash<QString, QString> toolOutputs;
    QHash<QString, std::shared_ptr<QMutex>> toolMutexes;   // 每键执行锁
};

// ── RunContext (DIAG-3 + NEW-5) ────────────────────────────────────────────
// Cancellation + fine-grained progress channel handed to probe implementations.
// NEW-5: probes only call progress() — the callback must queue to the main
// thread before touching any QObject/UI state.
struct RunContext {
    std::atomic<bool>& cancelled;
    std::function<void(int pct, const QString& stage)> progress;
    std::shared_ptr<RunSnapshot> snapshot;   // 每轮共享快照（nullptr=无，探针自行回退）
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
