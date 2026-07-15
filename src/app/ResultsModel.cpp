// =============================================================================
// ResultsModel.cpp — Diagnostic result aggregation and formatting for QML
// =============================================================================
#include "app/ResultsModel.h"
#include "Common/Model/DiagNames.h"
#include "Configuration/Model/DiagnosticConfig.h"
#include "Diagnostics/Model/G5/G5WebsiteUrl.h"

ResultsModel::ResultsModel(QObject* parent) : QObject(parent) {}

void ResultsModel::setSchemeFilter(const QString& scheme, bool hasUrl) {
    m_schemeFilter = scheme.isEmpty() ? QStringLiteral("https") : scheme.toLower();
    m_hasUrlScheme = hasUrl;
}

void ResultsModel::setTotalPerGroup(const QMap<DiagGroup, int>& totalPerGroup) {
    m_totalPerGroup = totalPerGroup;
}

void ResultsModel::setTotalDiags(int total) {
    m_totalDiags = total;
}

void ResultsModel::setCurrentGroup(int groupIdx) {
    m_currentRunningGroup = groupIdx;
}

void ResultsModel::addResult(DiagId id, const DiagnosticResult& result) {
    m_results[id] = result;
    DiagGroup g = DiagnosticConfig::diagGroup(id);
    m_completedPerGroup[g]++;
    m_totalCompleted++;
    m_resultsVersion++;
    m_cachedStatsVersion = -1;
    emit progressChanged();
}

void ResultsModel::clear() {
    m_results.clear();
    m_completedPerGroup.clear();
    m_totalPerGroup.clear();
    m_totalCompleted = 0;
    m_totalDiags = 0;
    m_resultsVersion = 0;
    m_currentRunningGroup = -1;
    m_cachedStatsVersion = -1;
    m_cachedGroupStats.clear();
    emit progressChanged();
}

// ── Static helpers (delegated to DiagNames.h / G5Common.h) ─────────────

QVariantMap ResultsModel::resultToVariantMap(const DiagnosticResult& r, bool includeProperties) {
    QVariantMap m;
    m["id"] = static_cast<int>(r.id);
    m["diagId"] = static_cast<int>(r.id);
    m["displayName"] = r.displayName.isEmpty() ? ::diagDisplayName(r.id) : r.displayName;
    m["status"] = static_cast<int>(r.status);
    m["statusIcon"] = r.statusIcon();
    m["summary"] = r.summary;
    m["details"] = r.details;
    m["durationMs"] = r.durationMs;
    if (includeProperties) {
        QVariantList props;
        for (const auto& p : r.properties) {
            QVariantMap pm;
            pm["label"] = p.label;
            pm["value"] = p.value;
            props.append(pm);
        }
        m["properties"] = props;
    }
    m["isDone"] = true;
    m["isPending"] = false;
    m["isRunning"] = false;
    return m;
}

// ── QML-invokable result accessors ──────────────────────────────────────
QVariantList ResultsModel::resultsForGroup(int groupInt) const {
    QVariantList list;
    if (!DiagnosticConfig::isValidGroup(groupInt)) return list;
    auto g = static_cast<DiagGroup>(groupInt);
    for (auto id : DiagnosticConfig::diagIdsForGroup(g)) {
        if (m_results.contains(id)) {
            const auto& r = m_results[id];
            if (r.status == DiagStatus::Skipped) continue;
            list.append(resultToVariantMap(r, false));
        }
    }
    return list;
}

QVariantList ResultsModel::allDiagsForGroup(int groupInt) const {
    QVariantList list;
    if (!DiagnosticConfig::isValidGroup(groupInt)) return list;
    auto g = static_cast<DiagGroup>(groupInt);

    for (auto id : DiagnosticConfig::diagIdsForGroup(g)) {
        // G5: hide pending tests that don't match the current URL scheme
        if (g == DiagGroup::G5 && m_hasUrlScheme && !m_results.contains(id)) {
            if (!g5DiagMatchesScheme(id, m_schemeFilter)) continue;
        }
        if (m_results.contains(id)) {
            if (m_results[id].status == DiagStatus::Skipped) continue;
            list.append(resultToVariantMap(m_results[id], true));
        } else {
            QVariantMap m;
            m["id"] = static_cast<int>(id);
            m["diagId"] = static_cast<int>(id);
            m["displayName"] = ::diagDisplayName(id);
            m["status"] = -1;
            m["statusIcon"] = QStringLiteral("badge-skip");
            m["summary"] = QString();
            m["details"] = QString();
            m["durationMs"] = 0;
            m["isDone"] = false;
            m["isPending"] = true;
            // isRunning: this pending test's group matches the currently executing group
            m["isRunning"] = (static_cast<int>(g) == m_currentRunningGroup);
            list.append(m);
        }
    }
    return list;
}

QVariantList ResultsModel::allDiagIdsForGroup(int groupInt) const {
    QVariantList list;
    if (!DiagnosticConfig::isValidGroup(groupInt)) return list;
    auto g = static_cast<DiagGroup>(groupInt);
    for (auto id : DiagnosticConfig::diagIdsForGroup(g)) {
        // G5: filter by scheme so Config shows only relevant protocol tests
        if (g == DiagGroup::G5 && m_hasUrlScheme) {
            if (!g5DiagMatchesScheme(id, m_schemeFilter)) continue;
        }
        list.append(static_cast<int>(id));
    }
    return list;
}

QVariantList ResultsModel::visibleGroups() const {
    QVariantList list;
    for (int i = 0; i < 5; ++i) {
        QVariantMap s = groupStats(i);
        if (s["enabled"].toInt() > 0 || s["total"].toInt() > 0)
            list.append(i);
    }
    return list;
}

QVariantMap ResultsModel::groupStats(int groupInt) const {
    QVariantMap stats;
    if (groupInt < 0) {
        int total = 0, pass = 0, warn = 0, fail = 0, skip = 0, info = 0, error = 0, completed = 0;
        for (int g = 0; g < 5; ++g) {
            QVariantMap gs = groupStats(g);
            total     += gs["total"].toInt();
            pass      += gs["pass"].toInt();
            warn      += gs["warn"].toInt();
            fail      += gs["fail"].toInt();
            skip      += gs["skip"].toInt();
            info      += gs["info"].toInt();
            error     += gs["error"].toInt();
            completed += gs["completed"].toInt();
        }
        stats["pass"] = pass; stats["warn"] = warn;
        stats["fail"] = fail; stats["skip"] = skip;
        stats["info"] = info; stats["error"] = error;
        stats["completed"] = completed; stats["total"] = total;
        stats["enabled"] = total;
        return stats;
    }
    auto g = static_cast<DiagGroup>(groupInt);
    int total = m_totalPerGroup.value(g, 0);
    int pass = 0, warn = 0, fail = 0, skip = 0, info = 0, error = 0, completed = 0;
    for (auto id : DiagnosticConfig::diagIdsForGroup(g)) {
        if (!m_results.contains(id)) continue;
        completed++;
        switch (m_results[id].status) {
            case DiagStatus::Pass:    pass++; break;
            case DiagStatus::Warning: warn++; break;
            case DiagStatus::Fail:    fail++; break;
            case DiagStatus::Skipped: skip++; break;
            case DiagStatus::Info:    info++; break;
            case DiagStatus::Error:   error++; break;
            default: break;
        }
    }
    stats["pass"] = pass; stats["warn"] = warn;
    stats["fail"] = fail; stats["skip"] = skip; stats["info"] = info; stats["error"] = error;
    stats["completed"] = completed; stats["total"] = total;
    stats["enabled"] = total;
    return stats;
}

QVariantList ResultsModel::allGroupStats() const {
    if (m_cachedStatsVersion == m_resultsVersion && !m_cachedGroupStats.isEmpty())
        return m_cachedGroupStats;
    m_cachedStatsVersion = m_resultsVersion;
    m_cachedGroupStats.clear();
    for (int g = 0; g < 5; ++g)
        m_cachedGroupStats.append(groupStats(g));
    return m_cachedGroupStats;
}

QVariantMap ResultsModel::getDetailResult(int diagIdInt) const {
    QVariantMap m;
    if (!DiagnosticConfig::isValidDiagId(diagIdInt)) return m;
    auto id = static_cast<DiagId>(diagIdInt);
    if (!m_results.contains(id)) return m;

    const auto& r = m_results[id];
    m["displayName"] = r.displayName;
    m["status"] = static_cast<int>(r.status);
    m["summary"] = r.summary;
    m["details"] = r.details;
    m["durationMs"] = r.durationMs;

    QVariantList props;
    for (const auto& p : r.properties) {
        QVariantMap pm;
        pm["label"] = p.label;
        pm["value"] = p.value;
        props.append(pm);
    }
    m["properties"] = props;
    return m;
}
