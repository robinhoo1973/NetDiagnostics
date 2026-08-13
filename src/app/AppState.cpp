// =============================================================================
// AppState.cpp — Suite observer implementation
// =============================================================================
#include "App/AppState.h"

#include "Common/Model/DiagnosticMeta.h"
#include "Common/Model/DiagNames.h"
#include "Common/Model/OutputContract.h"
#include "Common/Platform/DeviceCapability.h"
#include "Common/Platform/PlatformFlags.h"
#include "Configuration/Controller/ConfigurationController.h"
#include "Diagnostics/Model/DiagnosticSuite.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QSettings>
#include <QVector>

namespace {
// 组索引 → DiagGroup（visibleGroups 用）
DiagGroup groupForIndex(int idx) {
    switch (idx) {
        case 0: return DiagGroup::G1;
        case 1: return DiagGroup::G2;
        case 2: return DiagGroup::G3;
        case 3: return DiagGroup::G4;
        default: return DiagGroup::G5;
    }
}
} // namespace

AppState::AppState(QObject* parent) : QObject(parent) {
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS) || defined(Q_OS_MACOS)
    m_isPremiumPlatform = true;   // Premium 仅在移动/Apple 平台提供（spec：Windows/Linux 隐藏）
#endif
    loadPreferences();
    m_config = new ConfigurationController(this, this);
    m_config->loadSettings();
}

QStringList AppState::supportedSchemes() const {
    return {QStringLiteral("https"), QStringLiteral("http"), QStringLiteral("ftp"),
            QStringLiteral("ssh"), QStringLiteral("smtp"), QStringLiteral("mysql"),
            QStringLiteral("postgresql"), QStringLiteral("redis"), QStringLiteral("mqtt")};
}

void AppState::setTarget(const QString& host, const QString& scheme) {
    // 输入解析：host 中可能带 path；scheme 来自下拉框。
    QString h = host.trimmed();
    QString path;
    const int slash = h.indexOf(QLatin1Char('/'));
    if (slash > 0) { path = h.mid(slash); h = h.left(slash); }
    if (m_targetHost != h || m_targetPath != path || m_targetScheme != scheme) {
        m_targetHost = h;
        m_targetPath = path;
        m_targetScheme = scheme.isEmpty() ? QStringLiteral("https") : scheme;
        m_targetError = h.isEmpty() ? QStringLiteral("Target host is required") : QString();
        emit targetChanged();
    }
}

void AppState::runDiagnostics() {
    if (m_runStatus == Running) return;
    if (m_targetHost.isEmpty()) {
        m_targetError = QStringLiteral("Target host is required");
        emit targetChanged();
        return;
    }
    m_results.clear();
    m_groupDone.clear();
    m_errorMessage.clear();
    m_currentDiagLabel.clear();   // 上一轮残留的“当前测试”标签清零
    // P1：仅运行激活的组（Config 页 groupActive 控制）
    m_pendingGroups.clear();
    for (int i = 0; i < 5; ++i)
        if (isGroupActive(i))
            m_pendingGroups.append(i);
    if (m_pendingGroups.isEmpty()) {
        m_runStatus = Error;
        m_errorMessage = QStringLiteral("All groups are disabled");
        emit runStatusChanged();
        emit progressChanged();
        return;
    }
    m_runStatus = Running;
    emit runStatusChanged();
    emit progressChanged();
    runNextGroup();
}

void AppState::runNextGroup() {
    if (m_pendingGroups.isEmpty()) {
        m_runStatus = Completed;
        m_currentGroup = -1;
        emit currentRunningGroupChanged();
        emit runStatusChanged();
        emit progressChanged();
        return;
    }
    const int gi = m_pendingGroups.takeFirst();
    const DiagGroup g = groupForIndex(gi);
    m_currentGroup = gi;
    emit currentRunningGroupChanged();

    m_suite = new DiagnosticSuite(g, this);
    QVector<DiagId> ids;
    for (DiagId id : allDiagIds())
        if (diagGroup(id) == g && isSchedulable(id)
            && (m_config == nullptr || m_config->isDiagEnabled(static_cast<int>(id))))
            ids.append(id);
    m_suite->setDiagIds(ids);
    connect(m_suite, &DiagnosticSuite::resultReady, this, [this](const DiagnosticResult& r) {
        m_results.insert(r.id, r);
        m_currentDiagLabel = r.displayName;
        updateItemModel(r.id, r);
        emit progressChanged();
    });
    connect(m_suite, &DiagnosticSuite::suiteFinished, this, &AppState::onSuiteFinished);
    m_groupDone.insert(gi, false);
    const QString target = m_targetHost + m_targetPath;
    m_suite->run(target, m_targetScheme.toLower());
}

void AppState::onSuiteFinished() {
    if (!m_suite) return;
    // 5WHY（取消竞态）：cancel() 已把状态置为 Cancelled，但 in-flight suite 的
    // suiteFinished 仍会到达。若继续推进 runNextGroup()（pending 已清空），
    // 状态会被覆写回 Completed，且 m_groupDone 写入 -1 脏键。
    if (m_runStatus != Running) {
        m_suite->deleteLater();
        m_suite = nullptr;
        return;
    }
    const int gi = m_currentGroup;
    m_groupDone.insert(gi, true);
    m_suite->deleteLater();
    m_suite = nullptr;
    emit progressChanged();
    runNextGroup();
}

void AppState::cancel() {
    if (m_runStatus != Running) return;
    if (m_suite) m_suite->cancel();
    m_pendingGroups.clear();
    m_runStatus = Cancelled;
    m_currentGroup = -1;
    emit currentRunningGroupChanged();
    emit runStatusChanged();
    emit progressChanged();
}

QVariantMap AppState::itemFor(DiagId id) const {
    QVariantMap m;
    m[QStringLiteral("diagId")] = static_cast<int>(id);
    m[QStringLiteral("label")] = diagDisplayName(id);
    m[QStringLiteral("iconName")] = diagnosticMeta(id).iconName;
    m[QStringLiteral("group")] = static_cast<int>(diagGroup(id));
    const auto it = m_results.constFind(id);
    if (it == m_results.constEnd()) {
        m[QStringLiteral("status")] = -1;
        m[QStringLiteral("isPending")] = true;
        m[QStringLiteral("isDone")] = false;
        m[QStringLiteral("isDisabled")] = false;
        m[QStringLiteral("durationMs")] = 0;
        m[QStringLiteral("summary")] = QString();
    } else {
        m[QStringLiteral("status")] = static_cast<int>(it->status);
        m[QStringLiteral("isPending")] = false;
        m[QStringLiteral("isDone")] = true;
        m[QStringLiteral("isDisabled")] = false;
        m[QStringLiteral("durationMs")] = it->durationMs;
        m[QStringLiteral("summary")] = it->summary;
    }
    return m;
}

QVariantList AppState::allDiagsForGroup(int groupInt) const {
    QVariantList out;
    const DiagGroup g = groupForIndex(groupInt);
    for (DiagId id : allDiagIds())
        if (diagGroup(id) == g && isSchedulable(id))
            out.append(itemFor(id));
    return out;
}

QVariantMap AppState::groupStats(int groupInt) const {
    QVariantMap s;
    int total = 0, completed = 0, pass = 0, warn = 0, fail = 0,
        skip = 0, info = 0, error = 0, cancelled = 0;
    const bool aggregate = (groupInt < 0);
    const DiagGroup g = groupForIndex(qMax(0, groupInt));
    for (DiagId id : allDiagIds()) {
        if (!isSchedulable(id)) continue;
        if (!aggregate && diagGroup(id) != g) continue;
        ++total;
        const auto it = m_results.constFind(id);
        if (it == m_results.constEnd()) continue;
        ++completed;
        switch (it->status) {
            case DiagStatus::Pass:      ++pass; break;
            case DiagStatus::Warning:   ++warn; break;
            case DiagStatus::Fail:      ++fail; break;
            case DiagStatus::Skipped:   ++skip; break;
            case DiagStatus::Info:      ++info; break;
            case DiagStatus::Error:     ++error; break;
            case DiagStatus::Cancelled: ++cancelled; break;
        }
    }
    s[QStringLiteral("total")] = total;
    s[QStringLiteral("completed")] = completed;
    s[QStringLiteral("pass")] = pass;
    s[QStringLiteral("warn")] = warn;
    s[QStringLiteral("fail")] = fail;
    s[QStringLiteral("skip")] = skip;
    s[QStringLiteral("info")] = info;
    s[QStringLiteral("error")] = error;
    s[QStringLiteral("cancelled")] = cancelled;
    qint64 totalMs = 0;
    for (const DiagnosticResult& r : m_results)
        totalMs += r.durationMs;
    s[QStringLiteral("durationMs")] = static_cast<qlonglong>(totalMs);
    return s;
}

QVariantList AppState::visibleGroups() const {
    QVariantList out;
    for (int i = 0; i < 5; ++i)
        if (isGroupActive(i))
            out.append(i);
    return out;
}

QVariantMap AppState::resultFor(int diagIdInt) const {
    const auto it = m_results.constFind(static_cast<DiagId>(diagIdInt));
    if (it == m_results.constEnd()) return {};
    QVariantMap m;
    m[QStringLiteral("diagId")] = static_cast<int>(it->id);
    m[QStringLiteral("displayName")] = it->displayName;
    m[QStringLiteral("status")] = static_cast<int>(it->status);
    m[QStringLiteral("summary")] = it->summary;
    m[QStringLiteral("details")] = it->details;
    m[QStringLiteral("rawOutput")] = it->rawOutput;
    m[QStringLiteral("durationMs")] = it->durationMs;
    m[QStringLiteral("errorOutput")] = it->errorOutput;
    QVariantList props;
    for (const auto& p : it->properties) {
        QVariantMap pm;
        pm[QStringLiteral("label")] = p.label;
        pm[QStringLiteral("value")] = p.value;
        pm[QStringLiteral("severity")] = static_cast<int>(p.severity);
        QVariantList children;
        for (const auto& c : p.children) {
            QVariantMap cm;
            cm[QStringLiteral("label")] = c.label;
            cm[QStringLiteral("value")] = c.value;
            children.append(cm);
        }
        pm[QStringLiteral("children")] = children;
        props.append(pm);
    }
    m[QStringLiteral("properties")] = props;
    // 5WHY（图表不显示根因）：viz/ResultChart 依赖 data.templateType 选图，
    // 但重建的契约层不再向结果注入该字段——在此由 meta 注入（不改探针结果）。
    QVariantMap data = it->data;
    data[QStringLiteral("templateType")] = static_cast<int>(diagnosticMeta(it->id).tmplType);
    m[QStringLiteral("data")] = data;
    return m;
}

QVariantMap AppState::contractFor(int diagIdInt) const {
    const DiagId id = static_cast<DiagId>(diagIdInt);
    const OutputContract c = ::contractFor(id);
    QVariantMap m;
    m[QStringLiteral("metricField")] = c.metric.field ? QString::fromLatin1(c.metric.field) : QString();
    m[QStringLiteral("metricUnit")] = c.metric.unit ? QString::fromLatin1(c.metric.unit) : QString();
    m[QStringLiteral("metricPrecision")] = c.metric.precision;
    m[QStringLiteral("chartType")] = static_cast<int>(c.chart.type);
    m[QStringLiteral("chartField")] = c.chart.field ? QString::fromLatin1(c.chart.field) : QString();
    m[QStringLiteral("showError")] = c.showError;
    m[QStringLiteral("showProps")] = c.showProps;
    m[QStringLiteral("showTerminal")] = c.showTerminal;
    return m;
}

void AppState::updateItemModel(DiagId id, const DiagnosticResult& r) {
    Q_UNUSED(id);
    Q_UNUSED(r);
    // items model 为惰性视图（allDiagsForGroup 每次由 m_results 推导），
    // 此钩子保留给未来增量通知（progressChanged 已驱动 UI 刷新）。
}

// ═══════════════════════════════════════════════════════════════════════
// P1：Config / 语言 / 主题 桥接
// ═══════════════════════════════════════════════════════════════════════

QStringList AppState::langItems() const {
    // 与 translations.json 语言顺序一致（0=ZH_CN … 14=AR）
    return {
        QStringLiteral("简体中文"), QStringLiteral("繁體中文"), QStringLiteral("日本語"),
        QStringLiteral("한국어"), QStringLiteral("हिन्दी"), QStringLiteral("Tiếng Việt"),
        QStringLiteral("Türkçe"), QStringLiteral("English"), QStringLiteral("Français"),
        QStringLiteral("Deutsch"), QStringLiteral("Русский"), QStringLiteral("Italiano"),
        QStringLiteral("Español"), QStringLiteral("Português"), QStringLiteral("العربية")
    };
}

QVariantList AppState::allDiagIdsForGroup(int groupInt) const {
    QVariantList out;
    const DiagGroup g = groupForIndex(groupInt);
    for (DiagId id : allDiagIds())
        if (diagGroup(id) == g && isSchedulable(id)
            && DeviceCapability::diagSupportedOnDevice(id))
            out.append(static_cast<int>(id));
    return out;
}

int AppState::diagCountForGroup(int groupInt) const {
    return allDiagIdsForGroup(groupInt).size();
}

QVariantList AppState::resultsForGroup(int groupInt) const {
    QVariantList out;
    const DiagGroup g = groupForIndex(groupInt);
    for (DiagId id : allDiagIds()) {
        if (diagGroup(id) != g || !isSchedulable(id)) continue;
        const auto it = m_results.constFind(id);
        if (it != m_results.constEnd())
            out.append(itemFor(id));
    }
    return out;
}

void AppState::setLanguage(int index) {
    if (index < 0 || index >= langItems().size()) return;
    if (m_languageIndex == index) return;
    m_languageIndex = index;
    savePreferences();
    bumpState();
    emit languageChanged();
}

void AppState::setThemeMode(int mode) {
    if (m_themeMode == mode) return;
    m_themeMode = mode;
    savePreferences();
    emit themeModeChanged();
}

void AppState::setGroupActive(int groupInt, bool active) {
    if (groupInt < 0 || groupInt > 4) return;
    if (m_activeGroups.contains(groupInt) == active) return;
    if (active) m_activeGroups.insert(groupInt);
    else m_activeGroups.remove(groupInt);
    savePreferences();
    bumpState();
}

bool AppState::isGroupActive(int groupInt) const {
    return groupInt >= 0 && groupInt <= 4 && m_activeGroups.contains(groupInt);
}

bool AppState::isDiagEnabled(int diagIdInt) const {
    return m_config && m_config->isDiagEnabled(diagIdInt);
}
bool AppState::setDiagEnabled(int diagIdInt, bool enabled) {
    const bool ok = m_config && m_config->setDiagEnabled(diagIdInt, enabled);
    if (ok) bumpState();
    return ok;
}
bool AppState::setGroupEnabled(int groupInt, bool enabled) {
    const bool ok = m_config && m_config->setGroupEnabled(groupInt, enabled);
    if (ok) bumpState();
    return ok;
}
bool AppState::isGroupAllEnabled(int groupInt) const {
    return m_config && m_config->isGroupAllEnabled(groupInt);
}
bool AppState::isGroupAnyEnabled(int groupInt) const {
    return m_config && m_config->isGroupAnyEnabled(groupInt);
}

void AppState::loadPreferences() {
    QSettings s;
    s.beginGroup(QStringLiteral("AppSettings"));
    // activeGroups：默认全部激活（缺省键 → 全量）
    const QStringList active = s.value(QStringLiteral("activeGroups")).toStringList();
    if (active.isEmpty()) {
        for (int i = 0; i < 5; ++i) m_activeGroups.insert(i);
    } else {
        for (const QString& str : active) {
            bool ok = false;
            const int g = str.toInt(&ok);
            if (ok && g >= 0 && g < 5) m_activeGroups.insert(g);
        }
    }
    m_languageIndex = s.value(QStringLiteral("languageIndex"), 7).toInt();
    m_themeMode = s.value(QStringLiteral("themeMode"), 2).toInt();
    s.endGroup();
}

void AppState::savePreferences() {
    QSettings s;
    s.beginGroup(QStringLiteral("AppSettings"));
    QStringList active;
    for (int i = 0; i < 5; ++i)
        if (m_activeGroups.contains(i))
            active.append(QString::number(i));
    s.setValue(QStringLiteral("activeGroups"), active);
    s.setValue(QStringLiteral("languageIndex"), m_languageIndex);
    s.setValue(QStringLiteral("themeMode"), m_themeMode);
    s.endGroup();
}

void AppState::bumpState() {
    ++m_stateVersion;
    emit stateVersionChanged();
}

// ═══════════════════════════════════════════════════════════════════════
// 剪贴板 / 报告文本
// ═══════════════════════════════════════════════════════════════════════

static QString statusToken(DiagStatus s) {
    switch (s) {
        case DiagStatus::Pass:      return QStringLiteral("PASS");
        case DiagStatus::Warning:   return QStringLiteral("WARN");
        case DiagStatus::Fail:      return QStringLiteral("FAIL");
        case DiagStatus::Skipped:   return QStringLiteral("SKIP");
        case DiagStatus::Info:      return QStringLiteral("INFO");
        case DiagStatus::Error:     return QStringLiteral("ERROR");
        case DiagStatus::Cancelled: return QStringLiteral("CANCELLED");
    }
    return QString();
}

QString AppState::buildReportText() const {
    QStringList lines;
    lines.append(QStringLiteral("NetDiagnostics report"));
    lines.append(QStringLiteral("Target: %1%2").arg(m_targetHost, m_targetPath));
    lines.append(QString());
    for (int gi = 0; gi < 5; ++gi) {
        const DiagGroup g = groupForIndex(gi);
        QStringList gl;
        for (DiagId id : allDiagIds()) {
            if (diagGroup(id) != g || !isSchedulable(id)) continue;
            const auto it = m_results.constFind(id);
            if (it == m_results.constEnd()) continue;
            gl.append(QStringLiteral("[%1] %2 — %3 (%4ms)")
                          .arg(statusToken(it->status), it->displayName,
                               it->summary, QString::number(it->durationMs)));
        }
        if (gl.isEmpty()) continue;
        lines.append(diagGroupLabel(g));
        for (const QString& l : gl) lines.append(QStringLiteral("  ") + l);
        lines.append(QString());
    }
    return lines.join(QLatin1Char('\n'));
}

void AppState::copyReportToClipboard() {
    QGuiApplication::clipboard()->setText(buildReportText());
}

void AppState::copyDetailToClipboard(int diagIdInt) {
    const auto it = m_results.constFind(static_cast<DiagId>(diagIdInt));
    if (it == m_results.constEnd()) return;
    QStringList lines;
    lines.append(QStringLiteral("[%1] %2").arg(statusToken(it->status), it->displayName));
    if (!it->summary.isEmpty()) lines.append(it->summary);
    if (!it->details.isEmpty()) lines.append(it->details);
    for (const auto& p : it->properties) {
        lines.append(QStringLiteral("%1: %2").arg(p.label, p.value));
        for (const auto& c : p.children)
            lines.append(QStringLiteral("  %1: %2").arg(c.label, c.value));
    }
    QGuiApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
}
