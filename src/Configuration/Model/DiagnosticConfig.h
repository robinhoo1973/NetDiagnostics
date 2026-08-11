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
    // 5WHY: allDiagIds().size() was 45 (G3NetskopeStatus excluded), but
    // the enum has 46 values (0-45).  G5Mqtt at position 45 failed all
    // downstream gates (detail page, clipboard, config enable/disable).
    // Use the G5Mqtt enum value directly as the upper bound.
    static bool isValidDiagId(int id) { return id >= 0 && id <= static_cast<int>(DiagId::G5Mqtt); }
    static bool isValidGroup(int g) { return g >= 0 && g < 5; }

private:
    QSet<DiagId> m_enabledDiags;
};
