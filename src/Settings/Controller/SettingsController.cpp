// =============================================================================
// SettingsController.cpp — Settings page controller implementation
// =============================================================================
#include "Settings/Controller/SettingsController.h"
#include "app/AppState.h"
#include "Common/Platform/PlatformShare.h"
#include "Common/Utils/DebugSwitch.h"
#include <QDir>
#include <QStandardPaths>
#include <QSettings>

static constexpr const char* kSettingsGroup = "AppSettings";

// Forward declare AppState methods that SettingsController needs
static bool isDarkMode();

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
    connect(&m_premium, &PremiumStore::restoreCompleted,
            this, &SettingsController::restoreCompleted);
}

void SettingsController::setLanguageIndex(int index) {
    if (index < 0 || index > 8) return;
    m_languageIndex = index;
    emit languageChanged();
    saveSettings();
    TRACE(" Language set to index %d\n", index);
}

void SettingsController::setThemeMode(int mode) {
    if (mode < 0 || mode > 2) return;
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
    if (!m_premium.isPremium()) { emit premiumRequired(); return; }
    const QString ext = (format == QLatin1String("pdf")) ? QStringLiteral("pdf")
                                                          : QStringLiteral("html");
#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID)
    const QString tmp = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("NetDiagnostics_report.%1").arg(ext));
    const QString saved = (ext == QLatin1String("pdf"))
        ? m_appState->exportPdf(tmp)
        : m_appState->exportHtml(tmp, isDarkMode());
    if (saved.isEmpty()) { emit m_appState->reportShared(false); return; }
    platformShareFile(saved,
                      ext == QLatin1String("pdf") ? QStringLiteral("application/pdf")
                                                  : QStringLiteral("text/html"),
                      QStringLiteral("Network Diagnostic Report"));
    QTimer::singleShot(5000, [saved]() { QFile::remove(saved); });
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
        : m_appState->exportHtml(tmp, isDarkMode());
    if (reportPath.isEmpty()) { emit m_appState->reportShared(false); return; }
    m_appState->emailReportDesktop(reportPath);
    emit m_appState->reportShared(true);
#endif
}

void SettingsController::loadSettings() {
    QSettings s;
    s.beginGroup(QString::fromLatin1(kSettingsGroup));
    int lang = s.value("language", 0).toInt();
    if (lang >= 0 && lang <= 8) {
        m_languageIndex = lang;
        emit languageChanged();
    }
    int theme = s.value("themeMode", 2).toInt();
    if (theme >= 0 && theme <= 2 && theme != m_themeMode) {
        m_themeMode = theme;
        emit themeChanged();
    }
    s.endGroup();
    s.sync();
}

void SettingsController::saveSettings() {
    QSettings s;
    s.beginGroup(QString::fromLatin1(kSettingsGroup));
    s.setValue("language", m_languageIndex);
    s.setValue("themeMode", m_themeMode);
    s.endGroup();
    s.sync();
}

// ── Stub: dark mode detection (AppState has isDarkMode()) ────────────────
static bool isDarkMode() {
#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
    return false; // mobile always dark
#else
    return true;  // desktop dark
#endif
}
