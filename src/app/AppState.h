// =============================================================================
// AppState.h — Central state object bridging C++ engine ↔ QML UI
//
// Architecture (post-God-Object refactor):
//   AppState is a FACADE — it owns no domain logic directly.  Target parsing
//   lives in TargetModel; diagnostic execution lives in AppState + TaskFactory;
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
class ConfigurationController;
class ReportController;
class SettingsController;
class Translator;
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
    // 5WHY: QML cannot track changes made through targetValidationError()
    // Q_INVOKABLE calls. Expose the same source through a notified property
    // so field borders and inline validation update with targetChanged.
    Q_PROPERTY(QString targetValidationErrorText READ targetValidationError NOTIFY targetChanged)
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
    Q_PROPERTY(int stateVersion READ stateVersion NOTIFY stateVersionChanged)
    Q_PROPERTY(int languageIndex READ languageIndex NOTIFY languageChanged)
    Q_PROPERTY(int themeMode READ themeMode WRITE setThemeMode NOTIFY themeChanged)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString appEdition READ appEdition CONSTANT)
    Q_PROPERTY(QString buildNumber READ buildNumber CONSTANT)
    Q_PROPERTY(QString gitHash READ gitHash CONSTANT)
    Q_PROPERTY(bool isPremium READ isPremium NOTIFY premiumChanged)
    Q_PROPERTY(bool purchaseInProgress READ purchaseInProgress NOTIFY purchaseInProgressChanged)
    // True on platforms that sell Premium (iOS/Android/macOS): sharing is
    // gated here; on Windows/Linux it is free.  Drives the share lock + the
    // Settings Premium card.
    Q_PROPERTY(bool isPremiumPlatform READ isPremiumPlatform CONSTANT)
    // True only on platforms with a real store backend (iOS StoreKit).  The
    // UI gates the Subscribe/restore CTA on this so Android/desktop never
    // offer a purchase that cannot complete.
    Q_PROPERTY(bool platformSupportsIap READ platformSupportsIap CONSTANT)
    // isMobile is normally true on iOS/Android; the ND_MOBILE=1 env var
    // forces it to true on desktop so the mobile layout can be tested and
    // screenshotted without a device / simulator.
    Q_PROPERTY(bool isMobile READ isMobile CONSTANT)
    // Crash report from the previous run (detected at startup). QML can show a
    // banner offering to share/upload the report when hasCrashReport is true.
    Q_PROPERTY(bool hasCrashReport READ hasCrashReport NOTIFY crashReportChanged)

public:
    explicit AppState(QObject* parent = nullptr);
    ~AppState() override;

    // ── MVC Controller & Model accessors (for QML context injection) ──────
    ConfigurationController* configurationController() const { return m_configCtrl; }
    ReportController* reportController() const { return m_reportCtrl; }
    SettingsController* settingsController() const { return m_settingsCtrl; }
    TargetModel* targetModel() const { return m_targetModel; }
    ResultsModel* resultsModel() const { return m_resultsModel; }

    Translator* translator() const { return m_translator; }

    // ── App version / build ────────────────────────────────────────────────
    QString appVersion() const;
    QString appEdition() const;
    QString buildNumber() const;
    QString gitHash() const;

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
    // 5WHY: AGENTS.md §Q_PROPERTY setters requires unconditional NOTIFY
    // emission so QML bindings re-evaluate even when the value is unchanged.
    // The old code guarded the emit behind if (v != _cellularWarnVisible),
    // which silently dropped the signal on same-value sets.
    void setCellularWarnVisible(bool v) { _cellularWarnVisible = v; emit cellularWarnVisibleChanged(); }
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
    Q_INVOKABLE QVariantMap getDetailResult(int diagIdInt) const { return m_resultsModel->getDetailResult(diagIdInt); }
    ReportData buildReportData() const;  // snapshot for ReportEngine

    int stateVersion() const { return m_stateGeneration.load(std::memory_order_acquire); }
    int languageIndex() const;
    Q_INVOKABLE void setLanguage(int index);

    // Theme mode — 1=light, 2=dark (matches ThemeEngine.litMode/drkMode).
    // Mode 0 (system) was removed from the UI (commit fbaebe9) and
    // is treated as dark for backward compatibility with stored settings.
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
    bool isMobile() const;
    bool isPremiumPlatform() const;
    bool platformSupportsIap() const;
    Q_INVOKABLE void setPremium(bool v);
    Q_INVOKABLE void requestSubscription();
    // RESERVED API: blocking restore (m_purchaseInProgress=true). No UI entry
    // currently calls it — see PremiumStore::restorePurchases().
    Q_INVOKABLE void restorePurchases();
    // 5WHY (iOS b21294): non-blocking auto-probe — see PremiumStore::probeRestore().
    Q_INVOKABLE void probeRestore();
    bool purchaseInProgress() const;
    // Premium-gated. Mobile: OS share sheet; desktop: default mail client.
    Q_INVOKABLE void shareReport(const QString& format);
    Q_INVOKABLE void shareExistingReport(const QString& filePath, const QString& format);
    Q_INVOKABLE void deleteFile(const QString& filePath);  // cleanup preview files
    Q_INVOKABLE void emailReportDesktop(const QString& path);

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
    Q_INVOKABLE QString targetValidationError() const { return m_targetModel->validationError(); }
    // Scheme authentication policy — delegated to TargetModel (C++ single source).
    Q_INVOKABLE bool schemeSupportsUsername(const QString& scheme) const { return m_targetModel->schemeSupportsUsername(scheme); }
    Q_INVOKABLE bool schemeSupportsPassword(const QString& scheme) const { return m_targetModel->schemeSupportsPassword(scheme); }
    Q_INVOKABLE bool canRun() const {
        // 5WHY: canRun() only checked isGroupAnyEnabled() — it ignored
        // m_activeGroups (deactivated groups) and the G4/G5 target
        // requirement, so the Run button lit up even when runDiagnostics()
        // would immediately block ("No diagnostic tests are enabled").
        // Mirror runDiagnostics()'s exact gating so the button state and
        // the actual run agree.
        if (m_runStatus == RunStatus::Running) return false;
        const bool hasTarget = !m_targetModel->isEmpty();
        for (int g = 0; g < 5; ++g) {
            if (!m_activeGroups.contains(g)) continue;      // user deactivated
            if ((g == 3 || g == 4) && !hasTarget) continue; // G4/G5 need a target
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
    void purchaseDeferred();
    void purchaseFailed();
    void restoreCompleted(bool restoredAny, bool isError);
    void groupActiveChanged();
    void crashReportChanged();

private slots:
    void onDiagFinished(DiagId id, const DiagnosticResult& result);

private:
    void reset();                       // internal: clears state before each run
    void startNextGroup();
    void runDiagInGroup(int groupIdx, int diagIdx);
    // Memory-only enable for a group's tests when NONE are enabled — used at
    // target time so upgraded installs (persisted G1-G3-only config) still get
    // G4/G5. Does nothing if the user partially configured the group.
    void ensureGroupTestsAvailable(DiagGroup g);
    Q_INVOKABLE QString diagDisplayName(int diagIdInt) const;
    void bumpVersion();

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
    bool _cellularWarnVisible = false;
    bool _bypassCellularCheck = false;   // one-shot: suppress check for current G3 entry only
    int m_totalDiags = 0;

    // MVC Controllers (own page-specific logic and sub-objects)
    ConfigurationController* m_configCtrl = nullptr;
    ReportController* m_reportCtrl = nullptr;
    SettingsController* m_settingsCtrl = nullptr;
    Translator* m_translator = nullptr;

    // DiagnosticConfig now owned by ConfigurationController (m_configCtrl)

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
    // 5WHY: the SAME m_activeGroups is written by two actors with conflicting
    // intent — the targetChanged lambda (runtime: "a target is present → G4/G5
    // usable") and the Config green dot (user preference: "I deactivated this
    // group", persisted). Without arbitration the lambda re-inserted a group
    // the user explicitly deactivated. m_userDeactivatedGroups records the
    // user's choice so the lambda skips those groups.
    QSet<int> m_userDeactivatedGroups;   // groups the user green-dotted OFF this session
    QSet<int> m_userConfiguredGroups;    // groups the user explicitly configured this session
    bool m_isMobile = false;  // computed in ctor: platform env OR ND_MOBILE=1 override

};
