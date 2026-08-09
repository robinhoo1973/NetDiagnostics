// =============================================================================
// DiagnosticConfig.cpp — extracted from AppState.cpp (~100 lines)
// =============================================================================
#include "Configuration/Model/DiagnosticConfig.h"
#include "Common/Platform/DeviceCapability.h"

DiagnosticConfig::DiagnosticConfig(QObject* parent) : QObject(parent) {
    enableDefaultGroups();
}

void DiagnosticConfig::enableDefaultGroups() {
    // 5WHY: G4/G5 tests were excluded here and re-enabled at runtime by
    // ConfigurationController::setAutomaticGroupEnabled() whenever the target
    // changed — that mutated the user-persisted enabledDiags set, silently
    // overriding explicit Config-page choices and polluting QSettings.
    // Defaulting ALL five groups to enabled keeps "type a target → G4/G5 run"
    // behaviour, while runtime availability is gated purely by AppState's
    // m_activeGroups (target-driven). loadSettings() later replaces this
    // default with the user's persisted preferences.
    for (auto id : allDiagIds())
        if (DeviceCapability::diagRunnable(id))   // 5WHY: only runnable tests
            m_enabledDiags.insert(id);            // are ever configurable
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

bool DiagnosticConfig::setDiagEnabled(int diagIdInt, bool enabled) {
    if (!isValidDiagId(diagIdInt)) return false;
    auto id = static_cast<DiagId>(diagIdInt);
    if (m_enabledDiags.contains(id) == enabled) return false;
    if (enabled) m_enabledDiags.insert(id);
    else m_enabledDiags.remove(id);
    return true;
}

// ── Group enable/disable ──────────────────────────────────────────────
// 5WHY: group operations iterate ONLY runnable tests.  Hidden (platform/
// device-impossible) tests must not affect select-all / deselect-all state,
// otherwise a group whose visible tests are all deselected would still report
// isGroupAnyEnabled()==true because the hidden ones stay enabled.
bool DiagnosticConfig::setGroupEnabled(int groupInt, bool enabled) {
    if (!isValidGroup(groupInt)) return false;
    auto g = static_cast<DiagGroup>(groupInt);
    if (enabled ? isGroupAllEnabled(groupInt) : !isGroupAnyEnabled(groupInt))
        return false;
    for (auto id : diagIdsForGroup(g)) {
        if (!DeviceCapability::diagRunnable(id)) continue;
        if (enabled) m_enabledDiags.insert(id);
        else m_enabledDiags.remove(id);
    }
    return true;
}

bool DiagnosticConfig::isGroupAllEnabled(int groupInt) const {
    if (!isValidGroup(groupInt)) return false;
    auto g = static_cast<DiagGroup>(groupInt);
    for (auto id : diagIdsForGroup(g)) {
        if (!DeviceCapability::diagRunnable(id)) continue;
        if (!m_enabledDiags.contains(id)) return false;
    }
    return true;
}

bool DiagnosticConfig::isGroupAnyEnabled(int groupInt) const {
    if (!isValidGroup(groupInt)) return false;
    auto g = static_cast<DiagGroup>(groupInt);
    for (auto id : diagIdsForGroup(g)) {
        if (!DeviceCapability::diagRunnable(id)) continue;
        if (m_enabledDiags.contains(id)) return true;
    }
    return false;
}
