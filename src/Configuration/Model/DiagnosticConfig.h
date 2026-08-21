// =============================================================================
// DiagnosticConfig.h — Configuration state for diagnostics: enable/disable
// individual tests and groups, and group queries.
//
// Extracted from AppState (~100 lines).  Owns the source of truth for which
// tests are active.  All methods are const-correct and self-contained.
// =============================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSet>
#include "Common/Model/DiagId.h"

class DiagnosticConfig : public QObject {
    Q_OBJECT

public:
    explicit DiagnosticConfig(QObject* parent = nullptr);

    // ── Diag enable/disable ──────────────────────────────────────────
    bool isDiagEnabled(int diagIdInt) const;
    bool setDiagEnabled(int diagIdInt, bool enabled);

    // ── Group enable/disable ─────────────────────────────────────────
    bool setGroupEnabled(int groupInt, bool enabled);
    bool isGroupAllEnabled(int groupInt) const;
    bool isGroupAnyEnabled(int groupInt) const;

    // ── Group queries ────────────────────────────────────────────────
    static QStringList groupLabels();
    static QVector<DiagId> const& allDiagIds();
    static QVector<DiagId> const& diagIdsForGroup(DiagGroup group);
    static DiagGroup diagGroup(DiagId id);

    // ── Auto-enable G1-G5 ──────────────────────────────────────────
    void enableDefaultGroups();

    // ── Accessor for task factory ────────────────────────────────────
    const QSet<DiagId>& enabledDiags() const { return m_enabledDiags; }

    // ── Validation helpers (used by AppState) ────────────────────────
    // 上界取 G5Mqtt 枚举值本身（曾按 allDiagIds().size() 计算，
    // 枚举增删时计数与枚举脱节使末尾 id 漏过下游门控）。
    static bool isValidDiagId(int id) { return id >= 0 && id <= static_cast<int>(DiagId::G5Mqtt); }
    static bool isValidGroup(int g) { return g >= 0 && g < 5; }

private:
    QSet<DiagId> m_enabledDiags;
};
