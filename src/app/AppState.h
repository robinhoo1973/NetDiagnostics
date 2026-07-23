// =============================================================================
// AppState.h — Central state object bridging C++ engine ↔ QML UI
//
// Architecture (post-God-Object refactor):
//   AppState is a FACADE — it owns no domain logic directly.  Target parsing
//   lives in TargetModel; diagnostic execution lives in DiagnosticsController;
//   settings/premium live in SettingsController; report generation lives in
//   ReportEngine.  QML properties delegate to these sub-objects.
// =============================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QSet>
#include <QTimer>
#include <atomic>
#include <memory>
#include "Common/Model/DiagId.h"
#include "Common/Model/DiagnosticResult.h"
#include "Configuration/Model/DiagnosticConfig.h"
#include "Report/Model/ReportEngine.h"
#include "app/TargetModel.h"
#include "app/ResultsModel.h"

// Forward declarations for MVC Controllers
class DashboardController;
class DiagnosticsController;
class ConfigurationController;
class ReportController;
class SettingsController;
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
    // ── Structured target fields (derived from / assembled into m_target) ──
    Q_PROPERTY(QString targetScheme READ targetScheme WRITE setTargetScheme NOTIFY targetChanged)
    Q_PROPERTY(QString targetHost READ targetHost WRITE setTargetHost NOTIFY targetChanged)
    Q_PROPERTY(int targetPort READ targetPort WRITE setTargetPort NOTIFY targetChanged)
    Q_PROPERTY(QString targetUsername READ targetUsername WRITE setTargetUsername NOTIFY targetChanged)
    Q_PROPERTY(QString targetPassword READ targetPassword WRITE setTargetPassword NOTIFY targetChanged)
    Q_PROPERTY(QString targetPath READ targetPath WRITE setTargetPath NOTIFY targetChanged)
    Q_PROPERTY(QStringList supportedSchemes READ supportedSchemes CONSTANT)
    Q_PROPERTY(int defaultPortForScheme READ defaultPortForScheme NOTIFY targetChanged)
    Q_PROPERTY(int resultsVersion READ resultsVersion NOTIFY progressChanged)
    Q_PROPERTY(int stateVersion READ stateVersion NOTIFY stateVersionChanged)
    Q_PROPERTY(int languageIndex READ languageIndex NOTIFY languageChanged)
    Q_PROPERTY(int themeMode READ themeMode WRITE setThemeMode NOTIFY themeChanged)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString appEdition READ appEdition CONSTANT)
    Q_PROPERTY(QString buildNumber READ buildNumber CONSTANT)
    Q_PROPERTY(bool isPremium READ isPremium NOTIFY premiumChanged)
    Q_PROPERTY(bool purchaseInProgress READ purchaseInProgress NOTIFY purchaseInProgressChanged)
    // Crash report from the previous run (detected at startup). QML can show a
    // banner offering to share/upload the report when hasCrashReport is true.
    Q_PROPERTY(bool hasCrashReport READ hasCrashReport NOTIFY crashReportChanged)

public:
    explicit AppState(QObject* parent = nullptr);
    ~AppState() override;

    // ── MVC Controller & Model accessors (for QML context injection) ──────
    DashboardController* dashboardController() const { return m_dashCtrl; }
    DiagnosticsController* diagnosticsController() const { return m_diagCtrl; }
    ConfigurationController* configurationController() const { return m_configCtrl; }
    ReportController* reportController() const { return m_reportCtrl; }
    SettingsController* settingsController() const { return m_settingsCtrl; }
    TargetModel* targetModel() const { return m_targetModel; }
    ResultsModel* resultsModel() const { return m_resultsModel; }

    // ── App version / build ────────────────────────────────────────────────
    QString appVersion() const;
    QString appEdition() const;
    QString buildNumber() const;

    // ── Target (delegated to TargetModel) ────────────────────────────────
    QString target() const { return m_targetModel->target(); }
    void setTarget(const QString& t);
    QString targetScheme() const { return m_targetModel->scheme(); }
    void setTargetScheme(const QString& s);
    QString targetHost() const { return m_targetModel->host(); }
    void setTargetHost(const QString& h);
    int targetPort() const { return m_targetModel->port(); }
    void setTargetPort(int p);
    QString targetUsername() const { return m_targetModel->username(); }
    void setTargetUsername(const QString& u);
    QString targetPassword() const { return m_targetModel->password(); }
    void setTargetPassword(const QString& p);
    QString targetPath() const { return m_targetModel->path(); }
    void setTargetPath(const QString& p);
    QStringList supportedSchemes() const { return m_targetModel->supportedSchemes(); }
    int defaultPortForScheme() const { return m_targetModel->defaultPort(); }
    Q_INVOKABLE void parseUrlIntoFields(const QString& urlString) { m_targetModel->parseUrlIntoFields(urlString); }

    // ── Run status ─────────────────────────────────────────────────────────
    int runStatusInt() const { return static_cast<int>(m_runStatus); }
    RunStatus runStatus() const { return m_runStatus; }

    // ── Progress ───────────────────────────────────────────────────────────
    int totalCompleted() const { return m_resultsModel->totalCompleted(); }
    int totalDiags() const { return m_resultsModel->totalDiags(); }
    QString currentDiagLabel() const;
    QString currentGroup() const { return m_currentGroup; }
    QString errorMessage() const { return m_errorMessage; }

    // ── Group labels ───────────────────────────────────────────────────────
    QStringList groupLabels() const;

    // ── Invokable methods (callable from QML) ──────────────────────────────
    Q_INVOKABLE bool isCellularData() const;  // true if active connection is cellular
    Q_PROPERTY(bool cellularWarnVisible READ cellularWarnVisible WRITE setCellularWarnVisible NOTIFY cellularWarnVisibleChanged)
    bool cellularWarnVisible() const { return _cellularWarnVisible; }
    void setCellularWarnVisible(bool v) { if (v != _cellularWarnVisible) { _cellularWarnVisible = v; emit cellularWarnVisibleChanged(); } }
    Q_INVOKABLE void continueAfterCellularWarn();
    Q_INVOKABLE void runDiagnostics();
    Q_INVOKABLE void cancel();
    // NOTE: diag/group config → delegated to ConfigurationController::config()
    Q_INVOKABLE bool isDiagEnabled(int diagIdInt) const;
    Q_INVOKABLE void setDiagEnabled(int diagIdInt, bool enabled);
    Q_INVOKABLE void setGroupEnabled(int groupInt, bool enabled);
    Q_INVOKABLE bool isGroupAllEnabled(int groupInt) const;
    Q_INVOKABLE bool isGroupAnyEnabled(int groupInt) const;
    Q_INVOKABLE void setGroupActive(int groupInt, bool active);
    Q_INVOKABLE bool isGroupActive(int groupInt) const;

    // QSettings persistence for language, active groups, and enabled diags
    Q_INVOKABLE void saveSettings();
    void loadSettings();

    // NOTE: result access → delegated to ResultsModel
    Q_INVOKABLE QVariantList resultsForGroup(int groupInt) const { return m_resultsModel->resultsForGroup(groupInt); }
    Q_INVOKABLE QVariantList allDiagsForGroup(int groupInt) const { return m_resultsModel->allDiagsForGroup(groupInt); }
    Q_INVOKABLE QVariantList allDiagIdsForGroup(int groupInt) const { return m_resultsModel->allDiagIdsForGroup(groupInt); }
    Q_INVOKABLE QVariantList visibleGroups() const { return m_resultsModel->visibleGroups(); }
    Q_INVOKABLE QVariantMap groupStats(int groupInt) const { return m_resultsModel->groupStats(groupInt); }
    QVariantList allGroupStats() const { return m_resultsModel->allGroupStats(); }
    Q_INVOKABLE void showDetailDialog(int diagIdInt);
    Q_INVOKABLE QVariantMap getDetailResult(int diagIdInt) const { return m_resultsModel->getDetailResult(diagIdInt); }

    // ── Simulator skip-policy bridge (Phase 2) ───────────────────────────
    // Accepts QVariantList of {diagId, testName, reason} maps from QML.
    // Called when the simulated OS/device changes so the policy engine can
    // enforce per-platform skip rules during diagnostic execution.
    Q_INVOKABLE void setSkipRules(const QVariantList& rules);
    Q_INVOKABLE QVariantList skipRules() const { return m_skipRules; }
    Q_PROPERTY(QVariantList policyRules READ skipRules NOTIFY skipRulesChanged)
    int stateVersion() const { return m_stateGeneration.load(std::memory_order_acquire); }
    int resultsVersion() const { return m_resultsModel->resultsVersion(); }
    int languageIndex() const;
    Q_INVOKABLE void setLanguage(int index);

    // Theme mode — 0=system, 1=light, 2=dark (matches ThemeEngine.sysMode/litMode/drkMode)
    int themeMode() const;
    Q_INVOKABLE void setThemeMode(int mode);
    Q_INVOKABLE bool isDarkMode() const { return themeMode() != 1; }

    // ── Report export (delegated to ReportEngine via ReportController) ──
    // NOTE: buildReportHtml/exportHtml/exportPdf → mostly in ReportEngine already;
    // remaining AppState methods are thin wrappers.  TODO: move to ReportController.
    Q_INVOKABLE QString buildReportHtml(bool fullDetail, bool darkBackground = false) const;
    Q_INVOKABLE QString renderPreviewImage(const QString& html, int width) const;
    QString buildRichHtmlDocument(bool darkBackground = true) const;
    Q_INVOKABLE QString defaultReportPath(const QString& ext) const;
    Q_INVOKABLE QString exportHtml(const QString& filePath, bool darkBackground = true) const;
    Q_INVOKABLE QString exportPdf(const QString& filePath) const;
    Q_INVOKABLE void openPdfExternally() const;
    Q_INVOKABLE void openHtmlExternally() const;
    Q_INVOKABLE QString generatePreviewPdf() const;
    Q_INVOKABLE void requestSavePath(const QString& format);

    // ── Premium / sharing ──────────────────────────────────────────────────
    bool isPremium() const;
    Q_INVOKABLE void setPremium(bool v);
    Q_INVOKABLE void requestSubscription();
    Q_INVOKABLE void restorePurchases();
    bool purchaseInProgress() const;
    // Premium-gated. Mobile: OS share sheet; desktop: default mail client.
    Q_INVOKABLE void shareReport(const QString& format);
    Q_INVOKABLE void shareExistingReport(const QString& filePath, const QString& format);
    Q_INVOKABLE void deleteFile(const QString& filePath);  // cleanup preview files

    // ── Crash report (from previous run) ───────────────────────────────────
    // Populated at startup by main.cpp when a leftover crash log is found.
    bool hasCrashReport() const { return !m_crashReportPath.isEmpty(); }
    Q_INVOKABLE QString crashReportPath() const { return m_crashReportPath; }
    void setCrashReportPath(const QString& path);
    // Opens the OS share sheet (iOS/Android) so the user can upload/email the
    // crash log; on desktop reveals the file in the system file manager.
    Q_INVOKABLE void shareCrashReport();

    // ── Target type helpers (delegated to TargetModel) ───────────────────
    Q_INVOKABLE bool isTargetEmpty() const { return m_targetModel->isEmpty(); }
    Q_INVOKABLE bool hasUrlScheme() const { return m_targetModel->hasUrlScheme(); }
    Q_INVOKABLE bool isTargetHttpUrl() const { return m_targetModel->isHttpUrl(); }
    Q_INVOKABLE bool isTargetUrl() const { return m_targetModel->isUrl(); }
    Q_INVOKABLE bool isTargetHost() const { return m_targetModel->isHost(); }
    Q_INVOKABLE QString targetValidationError() const { return m_targetModel->validationError(); }
    Q_INVOKABLE bool canRun() const {
        if (m_runStatus == RunStatus::Running) return false;
        for (int g = 0; g < 5; ++g) {
            if (isGroupAnyEnabled(g)) return true;
        }
        return false;
    }

signals:
    void targetChanged();
    void runStatusChanged();
    void progressChanged();
    void currentDiagChanged();
    void groupChanged();
    void diagCompleted(int diagIdInt);
    void diagFailed(int diagIdInt);     // Phase 3: emitted when status is Fail or Error
    void cellularWarnVisibleChanged();
    void resultsReset();
    void stateVersionChanged();
    void languageChanged();
    void themeChanged();
    void savePathPicked(const QString& format, const QString& path);
    void premiumChanged();
    void premiumRequired();
    void reportShared(bool ok);
    void purchaseInProgressChanged();
    void restoreCompleted(bool restoredAny, bool isError);
    void groupActiveChanged();
    void skipRulesChanged();
    void crashReportChanged();

private slots:
    void onDiagFinished(DiagId id, const DiagnosticResult& result);

private:
    friend class SettingsController;  // needs access to emailReportDesktop, buildReportData
    friend class ConfigurationController;
    friend class DashboardController;
    friend class ReportController;
    friend class DiagnosticsController;

    void reset();                       // internal: clears state before each run
    void startNextGroup();
    void runDiagInGroup(int groupIdx, int diagIdx);
    Q_INVOKABLE QString diagDisplayName(int diagIdInt) const;
    static QString staticDiagDisplayName(DiagId id);
    // 5WHY: Extracted from resultsForGroup/allDiagsForGroup to prevent
    // DRY violation.  Must stay as private static member (not file-scope)
    // because staticDiagDisplayName() is also private.
    static QVariantMap resultToVariantMap(const DiagnosticResult& r, bool includeProperties);
    void bumpVersion();

    // ── Internal helpers (used by Controllers) ──────────────────────────────
    void emailReportDesktop(const QString& path);
    ReportData buildReportData() const;  // snapshot for ReportEngine

    // Target URL parsing → extracted to TargetModel
    TargetModel* m_targetModel = nullptr;
    // Result formatting → extracted to ResultsModel
    ResultsModel* m_resultsModel = nullptr;
    // Path to a crash log left by the previous run (empty if none)
    QString m_crashReportPath;

    RunStatus m_runStatus = RunStatus::Idle;
    QString m_currentGroup;
    QString m_currentDiagName;
    QString m_errorMessage;
    QString m_targetError;
    bool _cellularWarnVisible = false;
    bool _cellularApproved = false;   // suppress cellular check on re-entry
    int m_totalDiags = 0;

    // MVC Controllers (own page-specific logic and sub-objects)
    DashboardController* m_dashCtrl = nullptr;
    DiagnosticsController* m_diagCtrl = nullptr;
    ConfigurationController* m_configCtrl = nullptr;
    ReportController* m_reportCtrl = nullptr;
    SettingsController* m_settingsCtrl = nullptr;

    // DiagnosticConfig now owned by ConfigurationController (m_configCtrl)
    // ReportEngine handles HTML/PDF generation + file dialogs
    ReportEngine m_reportEngine;

    QMap<DiagId, DiagnosticResult> m_results;
    QMap<DiagGroup, int> m_totalPerGroup;

    // Group-sequential execution
    struct GroupTask { QList<DiagId> diagIds; DiagGroup group; };
    QList<GroupTask> m_pendingGroups;

    int m_currentGroupIdx = 0;
    std::atomic<int> m_activeGroupDone{0};
    std::atomic<int> m_stateGeneration{0};
    std::atomic<int> m_runGeneration{0};
    // m_languageIndex, m_themeMode, m_premium → now owned by SettingsController
    QSet<int> m_activeGroups; // G1-G3 active by default; G4/G5 auto-managed via setTarget()

    // ── Simulator skip-policy state ──────────────────────────────────────
    QVariantList       m_skipRules;       // exposed to QML via policyRules
    QHash<int, QString> m_skipReasonMap;   // fast diagId → reason lookup
};
