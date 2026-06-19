// =============================================================================
// AppState.h — Central state object bridging C++ engine ↔ QML UI
// =============================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QSet>
#include <QFuture>
#include <atomic>
#include <memory>
#include "models/TestId.h"
#include "models/DiagnosticResult.h"

class DiagnosticEngine;
class PlatformCommand;

enum class RunStatus { Idle, Running, Completed, Cancelled, Error };

class AppState : public QObject {
    Q_OBJECT

    // ── Properties exposed to QML ──────────────────────────────────────────
    Q_PROPERTY(QString target READ target WRITE setTarget NOTIFY targetChanged)
    Q_PROPERTY(int runStatus READ runStatusInt NOTIFY runStatusChanged)
    Q_PROPERTY(int totalCompleted READ totalCompleted NOTIFY progressChanged)
    Q_PROPERTY(int totalTests READ totalTests NOTIFY progressChanged)
    Q_PROPERTY(QString currentTestLabel READ currentTestLabel NOTIFY currentTestChanged)
    Q_PROPERTY(QString currentGroup READ currentGroup NOTIFY groupChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY runStatusChanged)
    Q_PROPERTY(QStringList groupLabels READ groupLabels CONSTANT)
    Q_PROPERTY(QVariantList allGroupStats READ allGroupStats NOTIFY progressChanged)
    Q_PROPERTY(bool portScanCommon READ portScanCommon WRITE setPortScanCommon NOTIFY portScanConfigChanged)
    Q_PROPERTY(int portScanFrom READ portScanFrom WRITE setPortScanFrom NOTIFY portScanConfigChanged)
    Q_PROPERTY(int portScanTo READ portScanTo WRITE setPortScanTo NOTIFY portScanConfigChanged)
    Q_PROPERTY(int resultsVersion READ resultsVersion NOTIFY progressChanged)

public:
    explicit AppState(QObject* parent = nullptr);
    ~AppState() override;

    // ── Target ─────────────────────────────────────────────────────────────
    QString target() const { return m_target; }
    void setTarget(const QString& t);

    // ── Run status ─────────────────────────────────────────────────────────
    int runStatusInt() const { return static_cast<int>(m_runStatus); }
    RunStatus runStatus() const { return m_runStatus; }

    // ── Progress ───────────────────────────────────────────────────────────
    int totalCompleted() const { return m_totalCompleted; }
    int totalTests() const { return m_totalTests; }
    QString currentTestLabel() const;
    QString currentGroup() const { return m_currentGroup; }
    QString errorMessage() const { return m_errorMessage; }

    // ── Group labels ───────────────────────────────────────────────────────
    QStringList groupLabels() const;

    // ── Port scan config ───────────────────────────────────────────────────
    bool portScanCommon() const { return m_portScanCommon; }
    void setPortScanCommon(bool v);
    int portScanFrom() const { return m_portScanFrom; }
    void setPortScanFrom(int v);
    int portScanTo() const { return m_portScanTo; }
    void setPortScanTo(int v);

    // ── Invokable methods (callable from QML) ──────────────────────────────
    Q_INVOKABLE void runDiagnostics();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void reset();
    Q_INVOKABLE bool isTestEnabled(int testIdInt) const;
    Q_INVOKABLE void setTestEnabled(int testIdInt, bool enabled);
    Q_INVOKABLE void setGroupEnabled(int groupInt, bool enabled);
    Q_INVOKABLE bool isGroupAllEnabled(int groupInt) const;
    Q_INVOKABLE bool isGroupAnyEnabled(int groupInt) const;
    Q_INVOKABLE QVariantList resultsForGroup(int groupInt) const;
    Q_INVOKABLE QVariantList allTestsForGroup(int groupInt) const;
    Q_INVOKABLE QVariantList allTestIdsForGroup(int groupInt) const;
    Q_INVOKABLE QVariantMap groupStats(int groupInt) const;
    QVariantList allGroupStats() const;
    Q_INVOKABLE void showDetailDialog(int testIdInt);
    Q_INVOKABLE QVariantMap getDetailResult(int testIdInt) const;
    int resultsVersion() const { return m_resultsVersion; }

    // ── Target type helpers (centralised — used by QML canEnable and C++ logic) ─
    Q_INVOKABLE bool isTargetEmpty() const { return m_target.trimmed().isEmpty(); }
    // True if target contains :// with ANY scheme — classifies as URL type
    Q_INVOKABLE bool hasUrlScheme() const {
        return m_target.contains("://") && !isTargetEmpty();
    }
    // True if target is a valid http/https URL (G5-relevant)
    Q_INVOKABLE bool isTargetHttpUrl() const {
        const QString t = m_target.trimmed();
        if (!t.contains("://")) return false;
        const QString scheme = t.section("://", 0, 0).toLower();
        return (scheme == "http" || scheme == "https") && !isTargetEmpty();
    }
    // Deprecated alias — kept for backward compat, same as isTargetHttpUrl
    Q_INVOKABLE bool isTargetUrl() const { return isTargetHttpUrl(); }
    // True if non-empty and not a URL (no :// scheme)
    Q_INVOKABLE bool isTargetHost() const { return !isTargetEmpty() && !hasUrlScheme(); }

    // ── Target validation ──────────────────────────────────────────────────
    Q_INVOKABLE QString targetValidationError() const { return m_targetError; }

signals:
    void targetChanged();
    void runStatusChanged();
    void progressChanged();
    void currentTestChanged();
    void groupChanged();
    void portScanConfigChanged();
    void testCompleted(int testIdInt);
    void resultsReset();

private slots:
    void onTestFinished(TestId id, DiagnosticResult result);

private:
    void startNextGroup();
    void runTestInGroup(int groupIdx, int testIdx);
    Q_INVOKABLE QString testDisplayName(int testIdInt) const;
    static QString staticTestDisplayName(TestId id);

    DiagnosticEngine* m_engine = nullptr;

    QString m_target;
    RunStatus m_runStatus = RunStatus::Idle;
    QString m_currentGroup;
    QString m_currentTestName;
    QString m_errorMessage;
    QString m_targetError;
    int m_totalCompleted = 0;
    int m_totalTests = 0;

    bool m_portScanCommon = true;
    int m_portScanFrom = 0;
    int m_portScanTo = 0;

    QSet<TestId> m_enabledTests; // all enabled by default
    QMap<TestId, DiagnosticResult> m_results;
    QMap<TestGroup, int> m_completedPerGroup;
    QMap<TestGroup, int> m_totalPerGroup;

    // Group-sequential execution
    struct GroupTask { QList<TestId> testIds; TestGroup group; };
    QList<GroupTask> m_pendingGroups;
    int m_currentGroupIdx = 0;
    std::atomic<int> m_activeGroupDone{0};
    int m_resultsVersion = 0;
};
