// =============================================================================
// AppState.h — Central state object bridging C++ engine ↔ QML UI
// =============================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QSet>
#include <QThreadPool>
#include <QTimer>
#include <QElapsedTimer>
#include <atomic>
#include <cstdint>
#include <memory>
#include "models/DiagId.h"
#include "models/DiagnosticResult.h"

enum class RunStatus { Idle, Running, Completed, Cancelled, Error };

class AppState : public QObject {
    Q_OBJECT

    // ── Properties exposed to QML ──────────────────────────────────────────
    Q_PROPERTY(QString target READ target WRITE setTarget NOTIFY targetChanged)
    Q_PROPERTY(int runStatus READ runStatusInt NOTIFY runStatusChanged)
    Q_PROPERTY(int totalCompleted READ totalCompleted NOTIFY progressChanged)
    Q_PROPERTY(int totalDiags READ totalDiags NOTIFY progressChanged)
    Q_PROPERTY(QString currentDiagLabel READ currentDiagLabel NOTIFY currentDiagChanged)
    Q_PROPERTY(QString currentGroup READ currentGroup NOTIFY groupChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY runStatusChanged)
    Q_PROPERTY(QStringList groupLabels READ groupLabels CONSTANT)
    Q_PROPERTY(QVariantList allGroupStats READ allGroupStats NOTIFY progressChanged)
    Q_PROPERTY(bool portScanCommon READ portScanCommon WRITE setPortScanCommon NOTIFY portScanConfigChanged)
    Q_PROPERTY(int portScanFrom READ portScanFrom WRITE setPortScanFrom NOTIFY portScanConfigChanged)
    Q_PROPERTY(int portScanTo READ portScanTo WRITE setPortScanTo NOTIFY portScanConfigChanged)
    Q_PROPERTY(int resultsVersion READ resultsVersion NOTIFY progressChanged)
    Q_PROPERTY(int stateVersion READ stateVersion NOTIFY stateVersionChanged)
    Q_PROPERTY(int languageIndex READ languageIndex NOTIFY languageChanged)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString buildNumber READ buildNumber CONSTANT)
    Q_PROPERTY(bool isPremium READ isPremium NOTIFY premiumChanged)

public:
    explicit AppState(QObject* parent = nullptr);
    ~AppState() override;

    // ── App version / build ────────────────────────────────────────────────
    QString appVersion() const;
    QString buildNumber() const;

    // ── Target ─────────────────────────────────────────────────────────────
    QString target() const { return m_target; }
    void setTarget(const QString& t);

    // ── Run status ─────────────────────────────────────────────────────────
    int runStatusInt() const { return static_cast<int>(m_runStatus); }
    RunStatus runStatus() const { return m_runStatus; }

    // ── Progress ───────────────────────────────────────────────────────────
    int totalCompleted() const { return m_totalCompleted; }
    int totalDiags() const { return m_totalDiags; }
    QString currentDiagLabel() const;
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
    Q_INVOKABLE bool isDiagEnabled(int diagIdInt) const;
    Q_INVOKABLE void setDiagEnabled(int diagIdInt, bool enabled);
    Q_INVOKABLE void setGroupEnabled(int groupInt, bool enabled);
    Q_INVOKABLE bool isGroupAllEnabled(int groupInt) const;
    Q_INVOKABLE bool isGroupAnyEnabled(int groupInt) const;
    Q_INVOKABLE QVariantList resultsForGroup(int groupInt) const;
    Q_INVOKABLE QVariantList allDiagsForGroup(int groupInt) const;
    Q_INVOKABLE QVariantList allDiagIdsForGroup(int groupInt) const;
    Q_INVOKABLE QVariantList visibleGroups() const;
    Q_INVOKABLE QVariantMap groupStats(int groupInt) const;
    QVariantList allGroupStats() const;
    Q_INVOKABLE void showDetailDialog(int diagIdInt);
    Q_INVOKABLE QVariantMap getDetailResult(int diagIdInt) const;
    int stateVersion() const { return m_stateGeneration.load(std::memory_order_acquire); }
    int resultsVersion() const { return m_resultsVersion; }
    int languageIndex() const { return m_languageIndex; }
    Q_INVOKABLE void setLanguage(int index);

    // ── Report export ────────────────────────────────────
    // buildReportHtml(false)=one-page summary; (true)=full detail per test.
    Q_INVOKABLE QString buildReportHtml(bool fullDetail) const;
    Q_INVOKABLE QString defaultReportPath(const QString& ext) const;
    Q_INVOKABLE QString exportHtml(const QString& filePath) const;
    Q_INVOKABLE QString exportPdf(const QString& filePath) const;
    // Desktop: opens a native NON-modal save dialog, then emits savePathPicked.
    // Mobile: emits savePathPicked immediately with a Documents path.
    Q_INVOKABLE void requestSavePath(const QString& format);

    // ── Premium / sharing ──────────────────────────────────────────────────
    bool isPremium() const { return m_isPremium; }
    Q_INVOKABLE void setPremium(bool v);
    // Premium-gated. Mobile: OS share sheet; desktop: default mail client.
    Q_INVOKABLE void shareReport(const QString& format);

    // ── Target type helpers ────────────────────────────────────────────────
    Q_INVOKABLE bool isTargetEmpty() const { return m_target.trimmed().isEmpty(); }
    Q_INVOKABLE bool hasUrlScheme() const {
        return m_target.contains("://") && !isTargetEmpty();
    }
    Q_INVOKABLE bool isTargetHttpUrl() const {
        const QString t = m_target.trimmed();
        if (!t.contains("://")) return false;
        const QString scheme = t.section("://", 0, 0).toLower();
        return (scheme == "http" || scheme == "https") && !isTargetEmpty();
    }
    Q_INVOKABLE bool isTargetUrl() const { return isTargetHttpUrl(); }
    Q_INVOKABLE bool isTargetHost() const { return !isTargetEmpty() && !hasUrlScheme(); }
    Q_INVOKABLE bool canRun() const {
        if (m_runStatus == RunStatus::Running) return false;
        for (int g = 0; g < 5; ++g) {
            if (isGroupAnyEnabled(g)) return true;
        }
        return false;
    }
    Q_INVOKABLE QString targetValidationError() const { return m_targetError; }

signals:
    void targetChanged();
    void runStatusChanged();
    void progressChanged();
    void currentDiagChanged();
    void groupChanged();
    void portScanConfigChanged();
    void diagCompleted(int diagIdInt);
    void resultsReset();
    void stateVersionChanged();
    void languageChanged();
    void savePathPicked(const QString& format, const QString& path);
    void premiumChanged();
    void premiumRequired();
    void reportShared(bool ok);

private slots:
    void onDiagFinished(DiagId id, DiagnosticResult result);

private:
    void startNextGroup();
    void runDiagInGroup(int groupIdx, int diagIdx);
    Q_INVOKABLE QString diagDisplayName(int diagIdInt) const;
    static QString staticDiagDisplayName(DiagId id);
    void bumpVersion();
    void emailReportDesktop(const QString& path);

    QString m_target;
    RunStatus m_runStatus = RunStatus::Idle;
    QString m_currentGroup;
    QString m_currentDiagName;
    QString m_errorMessage;
    QString m_targetError;
    int m_totalCompleted = 0;
    int m_totalDiags = 0;

    bool m_portScanCommon = true;
    int m_portScanFrom = 0;
    int m_portScanTo = 0;

    QSet<DiagId> m_enabledDiags;
    QMap<DiagId, DiagnosticResult> m_results;
    QMap<DiagGroup, int> m_completedPerGroup;
    QMap<DiagGroup, int> m_totalPerGroup;

    // Group-sequential execution
    struct GroupTask { QList<DiagId> diagIds; DiagGroup group; };
    QList<GroupTask> m_pendingGroups;

    int m_currentGroupIdx = 0;
    std::atomic<int> m_activeGroupDone{0};
    std::atomic<int> m_stateGeneration{0};
    std::atomic<int> m_runGeneration{0};
    int m_resultsVersion = 0;
    int m_languageIndex = 0; // 0=EN,1=FR,2=DE,3=RU,4=IT,5=ZH_CN,6=ZH_TW
    bool m_isPremium = false;

    // Cached group stats — invalidated on progressChanged
    mutable QVariantList m_cachedGroupStats;
    mutable int m_cachedStatsVersion = -1;
};
