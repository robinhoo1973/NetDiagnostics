// =============================================================================
// DiagnosticConfig.h — Configuration state for diagnostics: enable/disable
// individual tests and groups, port-scan settings, and group queries.
//
// Extracted from AppState (~100 lines).  Owns the source of truth for which
// tests are active and what port-scan parameters to use.  All methods are
// const-correct and self-contained.
// =============================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSet>
#include <QMap>
#include <QVariantMap>
#include "models/DiagId.h"
#include "models/DiagnosticResult.h"

class DiagnosticConfig : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool portScanCommon READ portScanCommon WRITE setPortScanCommon NOTIFY portScanConfigChanged)
    Q_PROPERTY(int portScanFrom READ portScanFrom WRITE setPortScanFrom NOTIFY portScanConfigChanged)
    Q_PROPERTY(int portScanTo READ portScanTo WRITE setPortScanTo NOTIFY portScanConfigChanged)

public:
    explicit DiagnosticConfig(QObject* parent = nullptr);

    // ── Port scan ────────────────────────────────────────────────────
    bool portScanCommon() const { return m_portScanCommon; }
    void setPortScanCommon(bool v);
    int portScanFrom() const { return m_portScanFrom; }
    void setPortScanFrom(int v);
    int portScanTo() const { return m_portScanTo; }
    void setPortScanTo(int v);

    // ── Diag enable/disable ──────────────────────────────────────────
    bool isDiagEnabled(int diagIdInt) const;
    void setDiagEnabled(int diagIdInt, bool enabled);

    // ── Group enable/disable ─────────────────────────────────────────
    void setGroupEnabled(int groupInt, bool enabled);
    bool isGroupAllEnabled(int groupInt) const;
    bool isGroupAnyEnabled(int groupInt) const;

    // ── Group queries ────────────────────────────────────────────────
    static QStringList groupLabels();
    static QList<DiagId> allDiagIds();
    static QList<DiagId> diagIdsForGroup(DiagGroup group);
    static DiagGroup diagGroup(DiagId id);

    // ── Group stats (read from results) ──────────────────────────────
    QVariantMap groupStats(int groupInt,
                           const QMap<DiagId, DiagnosticResult>& results) const;
    QVariantList allGroupStats(const QMap<DiagId, DiagnosticResult>& results) const;

    // ── Auto-enable G1-G3 ───────────────────────────────────────────
    void enableDefaultGroups();

    // ── Accessors for task factory ───────────────────────────────────
    const QSet<DiagId>& enabledDiags() const { return m_enabledDiags; }

    // ── Validation helpers (used by AppState) ───────────────────────
    static bool isValidDiagId(int id) { return id >= 0 && id < static_cast<int>(allDiagIds().size()); }
    static bool isValidGroup(int g) { return g >= 0 && g < 5; }

signals:
    void portScanConfigChanged();

private:
    bool m_portScanCommon = true;
    int m_portScanFrom = 0;
    int m_portScanTo = 0;
    QSet<DiagId> m_enabledDiags;
};
