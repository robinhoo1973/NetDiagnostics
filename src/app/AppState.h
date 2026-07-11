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
#include "Common/Model/DiagId.h"
#include "Common/Model/DiagnosticResult.h"
#include "Configuration/Model/DiagnosticConfig.h"
#include "Report/Model/ReportEngine.h"
// PremiumStore now owned by SettingsController

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

public:
    explicit AppState(QObject* parent = nullptr);
    ~AppState() override;

    // ── MVC Controller accessors (for QML context injection) ────────────────
    DashboardController* dashboardController() const { return m_dashCtrl; }
    DiagnosticsController* diagnosticsController() const { return m_diagCtrl; }
    ConfigurationController* configurationController() const { return m_configCtrl; }
    ReportController* reportController() const { return m_reportCtrl; }
    SettingsController* settingsController() const { return m_settingsCtrl; }

    // ── App version / build ────────────────────────────────────────────────
    QString appVersion() const;
    QString appEdition() const;
    QString buildNumber() const;

    // ── Target ─────────────────────────────────────────────────────────────
    QString target() const { return m_target; }
    void setTarget(const QString& t);

    // ── Structured target accessors (derived from / assembled into m_target) ──
    QString targetScheme() const;
    void setTargetScheme(const QString& s);
    QString targetHost() const;
    void setTargetHost(const QString& h);
    int targetPort() const;
    void setTargetPort(int p);
    QString targetUsername() const;
    void setTargetUsername(const QString& u);
    QString targetPassword() const;
    void setTargetPassword(const QString& p);
    QString targetPath() const;
    void setTargetPath(const QString& p);
    QStringList supportedSchemes() const;
    int defaultPortForScheme() const;
    Q_INVOKABLE void parseUrlIntoFields(const QString& urlString);

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

    // ── Invokable methods (callable from QML) ──────────────────────────────
    Q_INVOKABLE void runDiagnostics();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void reset();
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

    Q_INVOKABLE QVariantList resultsForGroup(int groupInt) const;
    Q_INVOKABLE QVariantList allDiagsForGroup(int groupInt) const;
    Q_INVOKABLE QVariantList allDiagIdsForGroup(int groupInt) const;
    Q_INVOKABLE QVariantList visibleGroups() const;
    Q_INVOKABLE QVariantMap groupStats(int groupInt) const;
    QVariantList allGroupStats() const;
    Q_INVOKABLE void showDetailDialog(int diagIdInt);
    Q_INVOKABLE QVariantMap getDetailResult(int diagIdInt) const;

    // ── Simulator skip-policy bridge (Phase 2) ───────────────────────────
    // Accepts QVariantList of {diagId, testName, reason} maps from QML.
    // Called when the simulated OS/device changes so the policy engine can
    // enforce per-platform skip rules during diagnostic execution.
    Q_INVOKABLE void setSkipRules(const QVariantList& rules);
    Q_INVOKABLE QVariantList skipRules() const { return m_skipRules; }
    Q_PROPERTY(QVariantList policyRules READ skipRules NOTIFY skipRulesChanged)
    int stateVersion() const { return m_stateGeneration.load(std::memory_order_acquire); }
    int resultsVersion() const { return m_resultsVersion; }
    int languageIndex() const;
    Q_INVOKABLE void setLanguage(int index);

    // Theme mode — 0=system, 1=light, 2=dark (matches ThemeEngine.sysMode/litMode/drkMode)
    int themeMode() const;
    Q_INVOKABLE void setThemeMode(int mode);
    Q_INVOKABLE bool isDarkMode() const { return themeMode() != 1; }

    // ── Report export ────────────────────────────────────
    // buildReportHtml(false)=one-page summary; (true)=full detail per test.
    // darkBackground=true uses dark theme colours (QML preview);
    // false = light (PDF printing). Defaults to false.
    Q_INVOKABLE QString buildReportHtml(bool fullDetail, bool darkBackground = false) const;
    // Renders report HTML to a QImage (via QTextDocument) for pixel-perfect
    // in-app preview. Returns the image file path or empty on failure.
    Q_INVOKABLE QString renderPreviewImage(const QString& html, int width) const;
    // Rich, browser-quality standalone HTML document (collapsible per-test
    // details). Used by exportHtml; not for the in-app QML preview
    // (QTextDocument can't render its CSS). darkBackground defaults true
    // for backward compatibility with shared HTML emails.
    QString buildRichHtmlDocument(bool darkBackground = true) const;
    Q_INVOKABLE QString defaultReportPath(const QString& ext) const;
    // 5WHY: exportHtml received pre-generated HTML, so theme was baked in
    // by the caller. Now generates HTML internally with the current theme
    // so shared HTML matches the app theme like exportPdf does.
    Q_INVOKABLE QString exportHtml(const QString& filePath, bool darkBackground = true) const;
    Q_INVOKABLE QString exportPdf(const QString& filePath) const;
    // 5WHY: "PDF Preview" was a QTextDocument→QImage rendering, not an actual
    // PDF. Generates a real PDF to a temp file and opens it in the system's
    // native PDF viewer (Preview.app / Edge / Okular / etc.) for true WYSIWYG.
    Q_INVOKABLE void openPdfExternally() const;
    Q_INVOKABLE void openHtmlExternally() const;
    // Generate a real PDF to a temp file and return the file:// path for
    // in-app preview with PdfMultiPageView (QtPdf). Returns empty on failure.
    Q_INVOKABLE QString generatePreviewPdf() const;
    // Desktop: opens a native NON-modal save dialog, then emits savePathPicked.
    // Mobile: emits savePathPicked immediately with a Documents path.
    Q_INVOKABLE void requestSavePath(const QString& format);

    // ── Premium / sharing ──────────────────────────────────────────────────
    bool isPremium() const;
    Q_INVOKABLE void setPremium(bool v);
    Q_INVOKABLE void requestSubscription();
    Q_INVOKABLE void restorePurchases();
    bool purchaseInProgress() const;
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
    Q_INVOKABLE bool isTargetUrl() const { return hasUrlScheme() && !isTargetEmpty(); }
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
    void diagCompleted(int diagIdInt);
    void diagFailed(int diagIdInt);     // Phase 3: emitted when status is Fail or Error
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

private slots:
    void onDiagFinished(DiagId id, DiagnosticResult result);

private:
    friend class SettingsController;  // needs access to emailReportDesktop, buildReportData
    friend class ConfigurationController;
    friend class DashboardController;
    friend class ReportController;
    friend class DiagnosticsController;

    void startNextGroup();
    void runDiagInGroup(int groupIdx, int diagIdx);
    Q_INVOKABLE QString diagDisplayName(int diagIdInt) const;
    static QString staticDiagDisplayName(DiagId id);
    void bumpVersion();

    // ── Internal helpers (used by Controllers) ──────────────────────────────
    void emailReportDesktop(const QString& path);
    ReportData buildReportData() const;  // snapshot for ReportEngine
    void assembleTargetUrl();            // rebuild m_target from structured fields
    void syncFieldsFromTarget();         // parse m_target → structured fields

    // Canonical target string (existing)
    QString m_target;
    // Structured target fields (derived)
    QString m_targetScheme;
    QString m_targetHost;
    int m_targetPort = -1;              // -1 = use scheme default
    QString m_targetUsername;
    QString m_targetPassword;
    QString m_targetPath;
    bool m_assembling = false;          // guard against re-entrant setTarget

    RunStatus m_runStatus = RunStatus::Idle;
    QString m_currentGroup;
    QString m_currentDiagName;
    QString m_errorMessage;
    QString m_targetError;
    int m_totalCompleted = 0;
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
    // m_languageIndex, m_themeMode, m_premium → now owned by SettingsController
    QSet<int> m_activeGroups; // G1-G3 active by default; G4/G5 auto-managed via setTarget()

    // ── Simulator skip-policy state ──────────────────────────────────────
    QVariantList       m_skipRules;       // exposed to QML via policyRules
    QHash<int, QString> m_skipReasonMap;   // fast diagId → reason lookup

    // Cached group stats — invalidated on progressChanged
    mutable QVariantList m_cachedGroupStats;
    mutable int m_cachedStatsVersion = -1;
};
