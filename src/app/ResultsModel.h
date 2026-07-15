// =============================================================================
// ResultsModel.h — Diagnostic result aggregation and formatting for QML
//
// Extracted from AppState to reduce the God Object.  Provides a read-only
// view of diagnostic results with QML-friendly formatting.  AppState owns
// the raw data; ResultsModel accesses it via const reference setters
// called from AppState::onDiagFinished / AppState::reset.
// =============================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QMap>
#include "Common/Model/DiagId.h"
#include "Common/Model/DiagnosticResult.h"

enum class DiagGroup;

class ResultsModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(int totalCompleted READ totalCompleted NOTIFY progressChanged)
    Q_PROPERTY(int totalDiags READ totalDiags NOTIFY progressChanged)
    Q_PROPERTY(QVariantList allGroupStats READ allGroupStats NOTIFY progressChanged)
    Q_PROPERTY(int resultsVersion READ resultsVersion NOTIFY progressChanged)

public:
    explicit ResultsModel(QObject* parent = nullptr);

    // Called by AppState during scheduling
    void setTotalPerGroup(const QMap<DiagGroup, int>& totalPerGroup);
    void setTotalDiags(int total);
    // Called by AppState when the target changes, so G5 tests can be filtered
    void setSchemeFilter(const QString& scheme, bool hasUrl);
    // Called by AppState when a result completes
    void addResult(DiagId id, const DiagnosticResult& result);
    void clear();  // reset to empty state

    int totalCompleted() const { return m_totalCompleted; }
    int totalDiags() const { return m_totalDiags; }
    int resultsVersion() const { return m_resultsVersion; }

    Q_INVOKABLE QVariantList resultsForGroup(int groupInt) const;
    Q_INVOKABLE QVariantList allDiagsForGroup(int groupInt) const;
    Q_INVOKABLE QVariantList allDiagIdsForGroup(int groupInt) const;
    Q_INVOKABLE QVariantList visibleGroups() const;
    Q_INVOKABLE QVariantMap groupStats(int groupInt) const;
    QVariantList allGroupStats() const;
    Q_INVOKABLE QVariantMap getDetailResult(int diagIdInt) const;

signals:
    void progressChanged();

private:
    static QVariantMap resultToVariantMap(const DiagnosticResult& r, bool includeProperties);
    static QString staticDiagDisplayName(DiagId id);

    QMap<DiagId, DiagnosticResult> m_results;
    QMap<DiagGroup, int> m_completedPerGroup;
    QMap<DiagGroup, int> m_totalPerGroup;
    int m_totalCompleted = 0;
    int m_totalDiags = 0;
    int m_resultsVersion = 0;
    QString m_schemeFilter;
    bool m_hasUrlScheme = false;

    // Cached group stats
    mutable QVariantList m_cachedGroupStats;
    mutable int m_cachedStatsVersion = -1;
};
