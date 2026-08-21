// =============================================================================
// DiagId.h — Diagnostic test identifiers, groups, statuses (contract layer)
//
// Refactored per review/refactor/diag/diag-execution-architecture-guide.md.
// Single source of truth for the 46-test identity space (45 schedulable +
// 1 deprecated slot, per NEW-24).
// =============================================================================
#pragma once

#include <QString>
#include <QVector>
#include <array>
#include <QMap>

// ── Test Group ──────────────────────────────────────────────────────────────
enum class DiagGroup { G1, G2, G3, G4, G5 };

inline QString diagGroupLabel(DiagGroup g) {
    switch (g) {
        case DiagGroup::G1: return QStringLiteral("System & Adapters");
        case DiagGroup::G2: return QStringLiteral("Connectivity & Security");
        case DiagGroup::G3: return QStringLiteral("Internet & DNS");
        case DiagGroup::G4: return QStringLiteral("Remote Host");
        case DiagGroup::G5: return QStringLiteral("Protocol");
    }
    return {};
}

// ── Test Status ─────────────────────────────────────────────────────────────
enum class DiagStatus { Pass, Warning, Fail, Skipped, Error, Info, Cancelled };

// 5WHY (复核 2026-08-18 用户诉求 "5/5 但徽标仅 4"): 结果状态以 int 持久化，
// 重启读回直接 static_cast——旧版本枚举重排会产出越界值：completed 照计、
// 7 状态 switch 无处落账，X/Y 与徽标求和分叉。单一摄入边界校验助手。
inline bool isValidDiagStatus(int v) {
    return v >= static_cast<int>(DiagStatus::Pass)
        && v <= static_cast<int>(DiagStatus::Cancelled);
}

// 5WHY (复核 2026-08-18 Reuse C5): 状态→{图标,报告文本,CSS类,调色板下标,字形}
// 此前是 4 个平行 switch（diagStatusIcon + ReportEngine 的 statusIndex/
// reportStatusText/reportStatusClass）——加 Cancelled 四处同步、漏一处静默
// 回退 Info。收敛为单一描述符表；QML 侧 statusColors/statusIconNames 数组
// 与 ThemeEngine.statusRows 是其镜像（跨语言无法同源，语言内同源）。
// 5WHY (复核 2026-08-20 Apple 保留词): 枚举成员 Check 触发 pre-commit
// 第 10 项 WARN（Apple SDK 保留词模式）——改 Tick，图标名 "badge-check"
// 与主题镜像均不受影响（QML 侧用字符串图标名，不经此枚举）。
enum class DiagStatusGlyph { Tick, Warning, Cross, Skip, Info };

struct DiagStatusDescriptor {
    DiagStatus      status;
    const char*     iconName;      // QML AppIcon 名称（与 ThemeEngine.statusIconNames 同值）
    const char*     reportText;    // 导出报告状态词
    const char*     reportCssClass;// buildRichDocument 徽标 CSS 类
    int             paletteIndex;  // ReportEngine hexColors/rgbColors 数组下标
    DiagStatusGlyph glyph;         // renderStatusIcon 画法
};

inline const DiagStatusDescriptor& statusDescriptor(DiagStatus s) {
    static const DiagStatusDescriptor table[] = {
        { DiagStatus::Pass,      "badge-check",   "Pass",      "pass",   0, DiagStatusGlyph::Tick },
        { DiagStatus::Warning,   "badge-warning", "Warning",   "warn",   1, DiagStatusGlyph::Warning },
        { DiagStatus::Fail,      "badge-close",   "Fail",      "fail",   2, DiagStatusGlyph::Cross },
        { DiagStatus::Error,     "badge-error",   "Error",     "error",  3, DiagStatusGlyph::Cross },
        { DiagStatus::Skipped,   "badge-skip",    "Skipped",   "skip",   4, DiagStatusGlyph::Skip },
        // Cancelled: close 图标 = X 中止语义，与 Skipped 区分（NEW-17 复核 2026-08-18）
        { DiagStatus::Cancelled, "close",         "Cancelled", "cancel", 5, DiagStatusGlyph::Cross },
        { DiagStatus::Info,      "badge-info",    "Info",      "info",   6, DiagStatusGlyph::Info },
    };
    for (const auto& d : table)
        if (d.status == s) return d;
    return table[6];   // 未知状态回退 Info
}

inline QString diagStatusIcon(DiagStatus s) {
    return QString::fromLatin1(statusDescriptor(s).iconName);
}

// ── Test ID (44 values; 全槽位可调度) ─────────────────────────────────────
// 5WHY (复核 2026-08-21 删除 TCP Settings/Netskope): 曾 46 值（45 可调度 +
// 1 弃用槽）。用户要求彻底删除该二检测项——枚举成员移除后为 44 值；
// translations.json 的 diagName/diagDesc 索引同步重排（QML 按 ""+id 查询）。
enum class DiagId {
    // G1 — System & Adapters (8)
    G1NetworkAdapters,
    G1NicAdvanced,
    G1WifiDiagnostics,
    G1WiredDiagnostics,
    G1DhcpStatus,
    G1IpConfiguration,
    G1ActiveConnections,
    G1CellularInfo,

    // G2 — Connectivity & Security (5)
    // 5WHY (复核 2026-08-21 用户诉求 "彻底删除 TCP Settings"): TCP 参数
    // 检测（/proc/sys 一组 sysctl 键值）信息价值低且跨平台数据源不一
    // （Windows 注册表/Linux sysfs 字段集不同）——用户明确要求全平台
    // 彻底删除，枚举成员移除（translations.json 索引同步重排）。
    G2NetworkProfile,
    G2DefaultGateway,
    G2RoutingTable,
    G2ArpTable,
    G2ProxySettings,

    // G3 — Internet & DNS
    // 5WHY (复核 2026-08-21 用户诉求 "彻底删除 Netskope"): Netskope 安全
    // 代理检测（ps 进程扫描）已按用户要求全平台彻底删除——枚举成员移除
    // （translations.json 索引同步重排）。
    G3DnsServers,
    G3DnsCache,
    G3DnsIntegrity,
    G3GeoIPLoc,
    G3InternetConnectivity,

    // G4 — Remote Host (6)
    G4DnsResolution,
    G4Ping,
    G4Traceroute,
    G4PathPing,
    G4MtuDiscovery,
    G4IPv6Connectivity,

    // G5 — Protocol (20)
    G5UrlParsing,
    G5TcpConnect,
    G5ServiceBanner,
    G5CurlVerbose,
    G5HttpHeaders,
    G5SecurityHeaders,
    G5SslCertificate,
    G5HttpRedirect,
    G5HttpCompression,
    G5HttpTiming,
    G5FtpDiagnostics,
    G5SshDiagnostics,
    G5EmailDiagnostics,
    G5Telnet,
    G5Mysql,
    G5Postgres,
    G5Redis,
    G5Mongodb,
    G5Ldap,
    G5Mqtt,
};

inline DiagGroup diagGroup(DiagId id) {
    const int v = static_cast<int>(id);
    if (v >= static_cast<int>(DiagId::G1NetworkAdapters) && v <= static_cast<int>(DiagId::G1CellularInfo)) return DiagGroup::G1;
    if (v >= static_cast<int>(DiagId::G2NetworkProfile)   && v <= static_cast<int>(DiagId::G2ProxySettings)) return DiagGroup::G2;
    if (v >= static_cast<int>(DiagId::G3DnsServers) && v <= static_cast<int>(DiagId::G3InternetConnectivity)) return DiagGroup::G3;
    if (v >= static_cast<int>(DiagId::G4DnsResolution)    && v <= static_cast<int>(DiagId::G4IPv6Connectivity)) return DiagGroup::G4;
    return DiagGroup::G5;
}

// Every DiagId value in declaration order (44 entries).
// 静态缓存 const& 返回（原 DiagnosticConfig 契约：O(1)，调用方持有引用安全）。
inline const QVector<DiagId>& allDiagIds() {
    static const QVector<DiagId> ids = [] {
        QVector<DiagId> v;
        for (int i = static_cast<int>(DiagId::G1NetworkAdapters);
             i <= static_cast<int>(DiagId::G5Mqtt); ++i)
            v.append(static_cast<DiagId>(i));
        return v;
    }();
    return ids;
}

// 5WHY (复核 2026-08-19): 弃用槽曾恢复为 Netskope；2026-08-21 用户要求
// 彻底删除该检测项——现在全部槽位均为正式可调度项。
// 函数形状保留（调用方契约；未来新增保留槽时在此排除）。
inline bool isSchedulable(DiagId id) { Q_UNUSED(id); return true; }

inline const QVector<DiagId>& diagIdsForGroup(DiagGroup g) {
    static const std::array<QVector<DiagId>, 5> cache = [] {
        std::array<QVector<DiagId>, 5> a;
        for (DiagId id : allDiagIds()) {
            // L6：接口内过滤不可调度槽（如未来新增保留槽），
            // 消除调用方依赖外部过滤的隐患
            if (!isSchedulable(id)) continue;
            const int gi = static_cast<int>(diagGroup(id));
            if (gi >= 0 && gi < 5)
                a[gi].append(id);
        }
        return a;
    }();
    const int gi = static_cast<int>(g);
    return (gi >= 0 && gi < 5) ? cache[gi] : cache[0];
}
