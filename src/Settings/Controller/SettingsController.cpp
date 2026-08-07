// =============================================================================
// SettingsController.cpp — Settings page controller implementation
// =============================================================================
#include "Settings/Controller/SettingsController.h"
#include "app/AppState.h"
#include "Common/Platform/PlatformShare.h"
#include "Common/Utils/DebugSwitch.h"
#include "Common/Utils/Translator.h"
#include <QDir>
#include <QStandardPaths>
#include <QSettings>

static constexpr const char* kSettingsGroup = "AppSettings";
static constexpr int kLanguageSchemaVersion = 2;

// 5WHY: Language indices were reordered when expanding from 8 to 15 languages.
// Old ordering: 0=EN,1=FR,2=DE,3=RU,4=IT,5=ZH_CN,6=ZH_TW,7=ES,8=PT
// New ordering: 0=ZH_CN,1=ZH_TW,2=JA,3=KO,4=HI,5=VI,6=TR,7=EN,8=FR,
//               9=DE,10=RU,11=IT,12=ES,13=PT,14=AR
// Users upgrading from the 8-language version have old indices saved.
// We migrate on first load when languageSchemaVersion < 2.
static int migrateLanguageIndex(int oldIndex) {
    // Old → New index mapping for the 9 original languages (0-8).
    // Indices 9+ did not exist in the old schema; left unchanged.
    switch (oldIndex) {
    case 0: return 7;   // EN
    case 1: return 8;   // FR
    case 2: return 9;   // DE
    case 3: return 10;  // RU
    case 4: return 11;  // IT
    case 5: return 0;   // ZH_CN
    case 6: return 1;   // ZH_TW
    case 7: return 12;  // ES
    case 8: return 13;  // PT
    default: return oldIndex;
    }
}


SettingsController::SettingsController(AppState* appState, QObject* parent)
    : QObject(parent), m_appState(appState)
{
    // Forward PremiumStore signals
    connect(&m_premium, &PremiumStore::premiumChanged,
            this, &SettingsController::premiumChanged);
    connect(&m_premium, &PremiumStore::purchaseInProgressChanged,
            this, &SettingsController::purchaseInProgressChanged);
    connect(&m_premium, &PremiumStore::premiumRequired,
            this, &SettingsController::premiumRequired);
    connect(&m_premium, &PremiumStore::purchaseDeferred,
            this, &SettingsController::purchaseDeferred);
    connect(&m_premium, &PremiumStore::restoreCompleted,
            this, &SettingsController::restoreCompleted);
}

void SettingsController::setLanguageIndex(int index) {
    // 15 languages: 0=ZH_CN…7=EN…14=AR
    // 5WHY: Missing same-value guard caused unnecessary QSettings writes
    // and signal cascade (languageChanged → stateVersionChanged) when the
    // user selects the already-active language in the Settings ComboBox.
    // This caused disk I/O and full QML binding re-evaluation for a no-op.
    // Compare with setThemeMode() which correctly guards against no-ops.
    if (index < 0 || index >= Translator::kMaxLanguages) return;
    if (m_languageIndex == index) return;
    m_languageIndex = index;
    emit languageChanged();
    saveSettings();
    TRACE(" Language set to index %d\n", index);
}

void SettingsController::setThemeMode(int mode) {
    // Mode 0 (system) was removed from the UI (commit fbaebe9).
    // Only valid modes are 1=light, 2=dark.  Mode 0 is rejected
    // here (setter) but silently migrated to dark in loadSettings()
    // for backward compatibility with stored QSettings.
    if (mode < 1 || mode > 2) return;
    if (m_themeMode == mode) return;
    m_themeMode = mode;
    emit themeChanged();
    saveSettings();
    TRACE(" Theme mode set to %d\n", mode);
}

void SettingsController::setPremium(bool v) {
    m_premium.setPremium(v);
}

void SettingsController::requestSubscription() {
    m_premium.requestSubscription();
}

void SettingsController::restorePurchases() {
    m_premium.restorePurchases();
}

void SettingsController::shareReport(const QString& format) {
    // 5WHY: Premium is sold only on iOS/Android/macOS.  On Windows/Linux the
    // OS send/mail API (emailReportDesktop → QDesktopServices mailto) is free —
    // do NOT gate it behind Premium there.
    if (m_premium.supportsIap() && !m_premium.isPremium()) { emit premiumRequired(); return; }
    const QString ext = (format == QLatin1String("pdf")) ? QStringLiteral("pdf")
                                                          : QStringLiteral("html");
#if defined(PLATFORM_MOBILE)
    const QString tmp = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("NetDiagnostics_report.%1").arg(ext));
    const QString saved = (ext == QLatin1String("pdf"))
        ? m_appState->exportPdf(tmp)
        : m_appState->exportHtml(tmp, m_appState->isDarkMode());
    if (saved.isEmpty()) { emit m_appState->reportShared(false); return; }
    platformShareFile(saved,
                      ext == QLatin1String("pdf") ? QStringLiteral("application/pdf")
                                                  : QStringLiteral("text/html"),
                      QStringLiteral("Network Diagnostic Report"));
    // 5WHY: QTimer::singleShot(5000) deleted the temp file after 5s, but
    // UIActivityViewController (iOS) accesses the file at its original URL
    // when the user selects a share target — NOT at presentation time.
    // If the user takes >5s to choose, the file is gone and the share fails.
    // Android FileProvider also needs the file to exist until the receiving
    // app reads it. 120s gives ample time; TempLocation is OS-cleaned anyway.
    QTimer::singleShot(120000, [saved]() { QFile::remove(saved); });
    emit m_appState->reportShared(true);
#else
    // 5WHY: Desktop path passed format string ("pdf"/"html") directly
    // to emailReportDesktop() which expects a file path. The file was
    // never generated — the email body contained "pdf" as the path.
    // Generate the report file first (same logic as mobile path), then
    // pass the actual file path to the email composer.
    const QString tmp = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("NetDiagnostics_report.%1").arg(ext));
    const QString reportPath = (ext == QLatin1String("pdf"))
        ? m_appState->exportPdf(tmp)
        : m_appState->exportHtml(tmp, m_appState->isDarkMode());
    if (reportPath.isEmpty()) { emit m_appState->reportShared(false); return; }
    m_appState->emailReportDesktop(reportPath);
    emit m_appState->reportShared(true);
#endif
}

// 5WHY: shareReport() generated a second copy of the report file for sharing,
// wasting time and storage.  The in-app preview already generates a file
// (via generatePreviewPdf / exportHtml).  shareExistingReport() reuses that
// existing file so the share is instant — no regeneration, no race condition,
// no timer-based cleanup.  The caller (ReportScreen.qml) owns the file
// lifecycle and deletes it when the preview is dismissed.
void SettingsController::shareExistingReport(const QString& filePath, const QString& format) {
    // 5WHY: same free-share rule as shareReport() — only IAP platforms pay.
    if (m_premium.supportsIap() && !m_premium.isPremium()) { emit premiumRequired(); return; }
    if (!QFile::exists(filePath)) {
        // Fall back to generating a fresh file if the preview file is missing
        shareReport(format);
        return;
    }
    const QString mimeType = (format == QLatin1String("pdf"))
        ? QStringLiteral("application/pdf") : QStringLiteral("text/html");
#if defined(PLATFORM_MOBILE)
    platformShareFile(filePath, mimeType, QStringLiteral("Network Diagnostic Report"));
    emit m_appState->reportShared(true);
#else
    m_appState->emailReportDesktop(filePath);
    emit m_appState->reportShared(true);
#endif
}

void SettingsController::loadSettings() {
    QSettings s;
    s.beginGroup(QString::fromLatin1(kSettingsGroup));

    // ── Language migration ────────────────────────────────────────────
    int schemaVer = s.value("languageSchemaVersion", 1).toInt();
    int lang = s.value("language", 7).toInt();  // default 7 = English
    // Only migrate if the user has a saved language preference from the
    // old 8-language ordering.  On fresh install, s.contains("language")
    // is false and the default index 7 (English) is already correct.
    if (schemaVer < kLanguageSchemaVersion && s.contains("language")) {
        lang = migrateLanguageIndex(lang);
        s.setValue("language", lang);
        s.setValue("languageSchemaVersion", kLanguageSchemaVersion);
    }
    if (lang >= 0 && lang < Translator::kMaxLanguages && lang != m_languageIndex) {
        m_languageIndex = lang;
        emit languageChanged();
    }

    // ── Theme migration ───────────────────────────────────────────────
    int theme = s.value("themeMode", 2).toInt();
    // Migrate legacy mode=0 (system theme) to dark to prevent
    // UX deadlock where neither light/dark button appears selected.
    if (theme == 0) theme = 2;
    if (theme >= 1 && theme <= 2 && theme != m_themeMode) {
        m_themeMode = theme;
        emit themeChanged();
    }
    s.endGroup();
}

void SettingsController::saveSettings() {
    QSettings s;
    s.beginGroup(QString::fromLatin1(kSettingsGroup));
    s.setValue("language", m_languageIndex);
    s.setValue("languageSchemaVersion", kLanguageSchemaVersion);
    s.setValue("themeMode", m_themeMode);
    s.endGroup();
}

