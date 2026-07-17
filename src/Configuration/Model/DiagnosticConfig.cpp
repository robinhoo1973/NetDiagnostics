// =============================================================================
// DiagnosticConfig.cpp — extracted from AppState.cpp (~100 lines)
// =============================================================================
#include "Configuration/Model/DiagnosticConfig.h"

DiagnosticConfig::DiagnosticConfig(QObject* parent) : QObject(parent) {
    enableDefaultGroups();
}

void DiagnosticConfig::enableDefaultGroups() {
    // Default: G1–G3 always on (no target needed); G4/G5 auto-toggle via setTarget()
    for (auto id : allDiagIds()) {
        auto g = diagGroup(id);
        if (g == DiagGroup::G1 || g == DiagGroup::G2 || g == DiagGroup::G3)
            m_enabledDiags.insert(id);
    }
}

// ── Group queries — delegate to canonical DiagId.h (single source of truth)
QStringList DiagnosticConfig::groupLabels() {
    return { diagGroupLabel(DiagGroup::G1), diagGroupLabel(DiagGroup::G2),
             diagGroupLabel(DiagGroup::G3), diagGroupLabel(DiagGroup::G4),
             diagGroupLabel(DiagGroup::G5) };
}

const QVector<DiagId>& DiagnosticConfig::allDiagIds() {
    return ::allDiagIds(); // DiagId.h free function (static cache, O(1))
}

const QVector<DiagId>& DiagnosticConfig::diagIdsForGroup(DiagGroup group) {
    // 5WHY: was copying QVector→QList on every call (heap alloc per invocation).
    // Now delegates directly to DiagId.h free function (static cache, O(1) ref).
    return ::diagIdsForGroup(group);
}

DiagGroup DiagnosticConfig::diagGroup(DiagId id) {
    return ::diagGroup(id);                  // DiagId.h free function (exhaustive switch)
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
    if (!isValidGroup(groupInt)) return false;
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
    int pass = 0, warn = 0, fail = 0, skip = 0, info = 0, error = 0;
    auto g = static_cast<DiagGroup>(groupInt);
    for (auto id : diagIdsForGroup(g)) {
        if (!m_enabledDiags.contains(id)) continue;
        auto it = results.find(id);
        if (it == results.end()) continue;
        switch (it->status) {
        case DiagStatus::Pass: ++pass; break;
        case DiagStatus::Warning: ++warn; break;
        case DiagStatus::Fail: ++fail; break;
        case DiagStatus::Skipped: ++skip; break;
        case DiagStatus::Info: ++info; break;
        case DiagStatus::Error: ++error; break;
        default: break;
        }
    }
    s[QStringLiteral("pass")] = pass;
    s[QStringLiteral("warn")] = warn;
    s[QStringLiteral("fail")] = fail;
    s[QStringLiteral("skip")] = skip;
    s[QStringLiteral("info")] = info;
    s[QStringLiteral("error")] = error;
    s[QStringLiteral("total")] = pass + warn + fail + skip + info + error;
    return s;
}

QVariantList DiagnosticConfig::allGroupStats(
        const QMap<DiagId, DiagnosticResult>& results) const {
    QVariantList list;
    for (int g = 0; g < 5; ++g)
        list.append(groupStats(g, results));
    return list;
}
