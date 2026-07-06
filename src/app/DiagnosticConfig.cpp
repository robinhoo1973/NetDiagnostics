// =============================================================================
// DiagnosticConfig.cpp — extracted from AppState.cpp (~100 lines)
// =============================================================================
#include "app/DiagnosticConfig.h"

DiagnosticConfig::DiagnosticConfig(QObject* parent) : QObject(parent) {
    enableDefaultGroups();
}

void DiagnosticConfig::enableDefaultGroups() {
    for (auto id : allDiagIds()) {
        auto g = diagGroup(id);
        if (g == DiagGroup::G1 || g == DiagGroup::G2 || g == DiagGroup::G3)
            m_enabledDiags.insert(id);
    }
}

// ── Port scan config ──────────────────────────────────────────────────
void DiagnosticConfig::setPortScanCommon(bool v) {
    if (m_portScanCommon != v) { m_portScanCommon = v; emit portScanConfigChanged(); }
}
void DiagnosticConfig::setPortScanFrom(int v) {
    if (m_portScanFrom != v) { m_portScanFrom = v; emit portScanConfigChanged(); }
}
void DiagnosticConfig::setPortScanTo(int v) {
    if (m_portScanTo != v) { m_portScanTo = v; emit portScanConfigChanged(); }
}

// ── Group labels ──────────────────────────────────────────────────────
QStringList DiagnosticConfig::groupLabels() {
    return { QStringLiteral("System & Adapters"),
             QStringLiteral("Connectivity & Security"),
             QStringLiteral("Internet & DNS"),
             QStringLiteral("Remote Host"),
             QStringLiteral("Website / URL") };
}

// ── Static helpers (now inline in DiagnosticConfig.h) ─────────────────

QList<DiagId> DiagnosticConfig::allDiagIds() {
    QList<DiagId> ids;
    for (int i = 0; i < 38; ++i) ids.append(static_cast<DiagId>(i));
    return ids;
}

QList<DiagId> DiagnosticConfig::diagIdsForGroup(DiagGroup group) {
    QList<DiagId> ids;
    for (auto id : allDiagIds()) {
        if (diagGroup(id) == group) ids.append(id);
    }
    return ids;
}

DiagGroup DiagnosticConfig::diagGroup(DiagId id) {
    auto i = static_cast<int>(id);
    if (i <= 4) return DiagGroup::G1;       // 0-4: Network adapters + MAC + IP
    if (i <= 13) return DiagGroup::G2;       // 5-13: Gateway, routing, ARP, DHCP, DNS servers, connections
    if (i <= 22) return DiagGroup::G3;       // 14-22: Ping, DNS resolution, traceroute, speed test
    if (i <= 29) return DiagGroup::G4;       // 23-29: Remote host diagnostics
    return DiagGroup::G5;                    // 30-37: Website/URL diagnostics
}

// ── Diag enable/disable ───────────────────────────────────────────────
bool DiagnosticConfig::isDiagEnabled(int diagIdInt) const {
    if (!isValidDiagId(diagIdInt)) return false;
    return m_enabledDiags.contains(static_cast<DiagId>(diagIdInt));
}

void DiagnosticConfig::setDiagEnabled(int diagIdInt, bool enabled) {
    if (!isValidDiagId(diagIdInt)) return;
    auto id = static_cast<DiagId>(diagIdInt);
    if (enabled) m_enabledDiags.insert(id);
    else m_enabledDiags.remove(id);
}

// ── Group enable/disable ──────────────────────────────────────────────
void DiagnosticConfig::setGroupEnabled(int groupInt, bool enabled) {
    if (!isValidGroup(groupInt)) return;
    auto g = static_cast<DiagGroup>(groupInt);
    for (auto id : diagIdsForGroup(g)) {
        if (enabled) m_enabledDiags.insert(id);
        else m_enabledDiags.remove(id);
    }
}

bool DiagnosticConfig::isGroupAllEnabled(int groupInt) const {
    if (!isValidGroup(groupInt)) return false;
    auto g = static_cast<DiagGroup>(groupInt);
    for (auto id : diagIdsForGroup(g)) {
        if (!m_enabledDiags.contains(id)) return false;
    }
    return true;
}

bool DiagnosticConfig::isGroupAnyEnabled(int groupInt) const {
    auto g = static_cast<DiagGroup>(groupInt);
    for (auto id : diagIdsForGroup(g)) {
        if (m_enabledDiags.contains(id)) return true;
    }
    return false;
}

// ── Group stats ───────────────────────────────────────────────────────
QVariantMap DiagnosticConfig::groupStats(int groupInt,
        const QMap<DiagId, DiagnosticResult>& results) const {
    QVariantMap s;
    int pass = 0, warn = 0, fail = 0, skip = 0, info = 0;
    auto g = static_cast<DiagGroup>(groupInt);
    for (auto id : diagIdsForGroup(g)) {
        if (!m_enabledDiags.contains(id)) continue;
        auto it = results.find(id);
        if (it == results.end()) continue;
        switch (it->status) {
        case DiagStatus::Pass: ++pass; break;
        case DiagStatus::Warn: ++warn; break;
        case DiagStatus::Fail: ++fail; break;
        case DiagStatus::Skipped: ++skip; break;
        default: ++info; break;
        }
    }
    s[QStringLiteral("pass")] = pass;
    s[QStringLiteral("warn")] = warn;
    s[QStringLiteral("fail")] = fail;
    s[QStringLiteral("skip")] = skip;
    s[QStringLiteral("info")] = info;
    s[QStringLiteral("total")] = pass + warn + fail + skip + info;
    return s;
}

QVariantList DiagnosticConfig::allGroupStats(
        const QMap<DiagId, DiagnosticResult>& results) const {
    QVariantList list;
    for (int g = 0; g < 5; ++g)
        list.append(groupStats(g, results));
    return list;
}
