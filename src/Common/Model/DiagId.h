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

inline QString diagStatusIcon(DiagStatus s) {
    switch (s) {
        case DiagStatus::Pass:     return QStringLiteral("badge-check");
        case DiagStatus::Warning:  return QStringLiteral("badge-warning");
        case DiagStatus::Fail:     return QStringLiteral("badge-close");
        case DiagStatus::Skipped:  return QStringLiteral("badge-skip");
        case DiagStatus::Error:    return QStringLiteral("badge-error");
        case DiagStatus::Info:     return QStringLiteral("badge-info");
        case DiagStatus::Cancelled: return QStringLiteral("badge-skip"); // NEW-17: deadline 中止项
    }
    return {};
}

// ── Test ID (46 values; 45 schedulable + 1 deprecated slot) ────────────────
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

    // G2 — Connectivity & Security (6)
    G2NetworkProfile,
    G2TcpSettings,
    G2DefaultGateway,
    G2RoutingTable,
    G2ArpTable,
    G2ProxySettings,

    // G3 — Internet & DNS (5 + 1 deprecated slot)
    _G3Reserved17_Deprecated,
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
    if (v >= static_cast<int>(DiagId::_G3Reserved17_Deprecated) && v <= static_cast<int>(DiagId::G3InternetConnectivity)) return DiagGroup::G3;
    if (v >= static_cast<int>(DiagId::G4DnsResolution)    && v <= static_cast<int>(DiagId::G4IPv6Connectivity)) return DiagGroup::G4;
    return DiagGroup::G5;
}

// Every DiagId value in declaration order (46 entries).
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

inline bool isSchedulable(DiagId id) { return id != DiagId::_G3Reserved17_Deprecated; }

inline const QVector<DiagId>& diagIdsForGroup(DiagGroup g) {
    static const std::array<QVector<DiagId>, 5> cache = [] {
        std::array<QVector<DiagId>, 5> a;
        for (DiagId id : allDiagIds()) {
            // L6：接口内过滤不可调度槽（_G3Reserved17_Deprecated），
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
