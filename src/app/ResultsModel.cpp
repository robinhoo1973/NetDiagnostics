// =============================================================================
// ResultsModel.cpp — Diagnostic result aggregation and formatting for QML
// =============================================================================
#include "app/ResultsModel.h"
#include "Common/Model/DiagNames.h"
#include "Common/Platform/DeviceCapability.h"
#include "Configuration/Model/DiagnosticConfig.h"
#include "Diagnostics/Model/G5/G5WebsiteUrl.h"
#include <QClipboard>
#include <QGuiApplication>

ResultsModel::ResultsModel(QObject* parent) : QObject(parent) {}

void ResultsModel::setSchemeFilter(const QString& scheme, bool hasUrl) {
    m_schemeFilter = scheme.isEmpty() ? QStringLiteral("https") : scheme.toLower();
    m_hasUrlScheme = hasUrl;
}

void ResultsModel::setTotalPerGroup(const QMap<DiagGroup, int>& totalPerGroup) {
    m_totalPerGroup = totalPerGroup;
}

void ResultsModel::setEnabledDiags(const QSet<int>& enabledIds) {
    m_enabledDiags = enabledIds;
}

void ResultsModel::setTotalDiags(int total) {
    m_totalDiags = total;
}

void ResultsModel::setCurrentGroup(int groupIdx) {
    if (m_currentRunningGroup == groupIdx) return;
    m_currentRunningGroup = groupIdx;
    emit currentRunningGroupChanged();
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
    if (m_currentRunningGroup != -1) {
        m_currentRunningGroup = -1;
        emit currentRunningGroupChanged();
    }
    m_enabledDiags.clear();
    m_cachedStatsVersion = -1;
    m_cachedGroupStats.clear();
    emit progressChanged();
}

// ── Static helpers (delegated to DiagNames.h / G5Common.h) ─────────────

// ── ResultProperty → QVariantMap (recursive; preserves severity + children) ─
// 5WHY: the serializer previously emitted only label/value — DetailPage's
// severity dots and nested child rows (e.g. G1DhcpStatus leases) were dead
// code because severity/children never crossed the C++→QML boundary.
static QVariantMap propToMap(const ResultProperty& p) {
    QVariantMap pm;
    pm["label"] = p.label;
    pm["value"] = p.value;
    pm["severity"] = static_cast<int>(p.severity);  // Info=0 Warning=1 Error=2
    QVariantList kids;
    for (const auto& c : p.children)
        kids.append(propToMap(c));
    if (!kids.isEmpty()) pm["children"] = kids;
    return pm;
}

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
        for (const auto& p : r.properties)
            props.append(propToMap(p));
        m["properties"] = props;
    }
    m["isDone"] = true;
    m["isPending"] = false;
    m["isRunning"] = false;
    // L5 Living Diagnostics: pass structured data + template classification
    if (!r.data.isEmpty()) {
        // Auto-inject templateType from DiagId — eliminates duck-typing in QML
        QVariantMap enriched = r.data;
        enriched["templateType"] = static_cast<int>(::diagTemplateType(r.id));
        m["data"] = enriched;
    }
    return m;
}

// ── Clipboard copy for the L5 detail-page "copy" action ─────────────────
// 5WHY: pure-QML clipboard access (Qt.copyTextToClipboard) requires Qt 6.5+,
// but this project's minimum is Qt 6.3 — the copy action must be a C++
// Q_INVOKABLE so it works on every supported platform (iOS/Android/desktop).
void ResultsModel::copyDetailToClipboard(int diagIdInt) const {
    if (!DiagnosticConfig::isValidDiagId(diagIdInt)) return;
    auto id = static_cast<DiagId>(diagIdInt);
    const auto resultIt = m_results.constFind(id);
    if (resultIt == m_results.constEnd()) return;

    const auto& r = resultIt.value();
    QString text = r.displayName.isEmpty() ? ::diagDisplayName(r.id) : r.displayName;
    if (!r.summary.isEmpty()) text += QStringLiteral("\n") + r.summary;
    if (!r.details.isEmpty()) text += QStringLiteral("\n\n") + r.details;
    if (!r.rawOutput.isEmpty() && r.rawOutput != r.details)
        text += QStringLiteral("\n\n") + r.rawOutput;
    if (!r.errorOutput.isEmpty())
        text += QStringLiteral("\n\n[error] ") + r.errorOutput;

    if (QGuiApplication::clipboard())
        QGuiApplication::clipboard()->setText(text);
}

// ── QML-invokable result accessors ──────────────────────────────────────
QVariantList ResultsModel::resultsForGroup(int groupInt) const {
    QVariantList list;
    if (!DiagnosticConfig::isValidGroup(groupInt)) return list;
    auto g = static_cast<DiagGroup>(groupInt);
    for (auto id : DiagnosticConfig::diagIdsForGroup(g)) {
        // 5WHY: keep the same hidden-test invariant everywhere — a stale
        // result for a now platform/device-impossible test (e.g. hardware
        // changed since the last run) must not resurface on the Dashboard.
        if (!DeviceCapability::diagRunnable(id)) continue;
        const auto resultIt = m_results.constFind(id);
        if (resultIt == m_results.constEnd()) continue;
        const auto& result = resultIt.value();
        if (result.status == DiagStatus::Skipped) continue;
        list.append(resultToVariantMap(result, false));
    }
    return list;
}

QVariantList ResultsModel::allDiagsForGroup(int groupInt) const {
    QVariantList list;
    if (!DiagnosticConfig::isValidGroup(groupInt)) return list;
    auto g = static_cast<DiagGroup>(groupInt);

    for (auto id : DiagnosticConfig::diagIdsForGroup(g)) {
        // 5WHY: platform/device-impossible tests are hidden entirely (never
        // scheduled, never counted as Skipped) — the Config page must not
        // offer a switch for a test that cannot run on this OS/hardware.
        if (!DeviceCapability::diagRunnable(id)) continue;
        // 5WHY: contains() followed by const operator[] performs two tree
        // searches and copies DiagnosticResult for every row during progress
        // refresh. Keep one iterator for G5 filtering and result formatting.
        const auto resultIt = m_results.constFind(id);
        // G5: hide pending tests that don't match the current URL scheme
        if (g == DiagGroup::G5 && m_hasUrlScheme && resultIt == m_results.constEnd()) {
            if (!g5DiagMatchesScheme(id, m_schemeFilter)) continue;
        }
        if (resultIt != m_results.constEnd()) {
            const auto& result = resultIt.value();
            if (result.status == DiagStatus::Skipped) continue;
            list.append(resultToVariantMap(result, true));
        } else if (!m_enabledDiags.isEmpty() && !m_enabledDiags.contains(static_cast<int>(id))) {
            // Disabled in config — show as pending with skip icon, not spinning
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
            m["isRunning"] = false;  // disabled — never shows spinner
            m["isDisabled"] = true;  // for per-item spinner gating (reactive group flag)
            list.append(m);
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
            // isRunning: this enabled pending test's group matches the currently executing group
            m["isRunning"] = (static_cast<int>(g) == m_currentRunningGroup);
            m["isDisabled"] = false;
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
        // 5WHY: hide OS/device-impossible tests from the Config list (they
        // are not scheduled and must not be configurable).
        if (!DeviceCapability::diagRunnable(id)) continue;
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
        const auto resultIt = m_results.constFind(id);
        if (resultIt == m_results.constEnd()) continue;
        completed++;
        switch (resultIt.value().status) {
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
    const auto resultIt = m_results.constFind(id);
    if (resultIt == m_results.constEnd()) return m;

    const auto& r = resultIt.value();
    // 5WHY: diagId was missing from the return map. page.currentDetail
    // in DiagnosticScreen.qml is set to this map, and the language-change
    // handler calls showDetailOverlay(page.currentDetail) which reads
    // detail.diagId → T.diagName(detail.diagId).  Without diagId in the
    // map, the read returns undefined → T.diagName(0) → wrong test name.
    m["diagId"] = diagIdInt;
    m["displayName"] = r.displayName;
    m["status"] = static_cast<int>(r.status);
    m["summary"] = r.summary;
    m["details"] = r.details;
    m["rawOutput"] = r.rawOutput;
    m["errorOutput"] = r.errorOutput;
    m["durationMs"] = r.durationMs;

    QVariantList props;
    for (const auto& p : r.properties)
        props.append(propToMap(p));
    m["properties"] = props;
    // L5 Living Diagnostics: pass structured data + template classification
    if (!r.data.isEmpty()) {
        QVariantMap enriched = r.data;
        enriched["templateType"] = static_cast<int>(::diagTemplateType(r.id));
        m["data"] = enriched;
    }
    return m;
}
