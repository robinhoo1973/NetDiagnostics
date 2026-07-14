// =============================================================================
// SettingsController.h — Settings page controller
//
// Owns: PremiumStore, language/theme settings, persistence (loadSettings/saveSettings)
// Accesses: AppState for shared diagnostic state
// =============================================================================
#pragma once

#include <QObject>
#include <QString>
#include <QSettings>
#include "Settings/Model/PremiumStore.h"

class AppState;

class SettingsController : public QObject {
    Q_OBJECT
    Q_PROPERTY(int languageIndex READ languageIndex NOTIFY languageChanged)
    Q_PROPERTY(int themeMode READ themeMode WRITE setThemeMode NOTIFY themeChanged)
    Q_PROPERTY(bool isPremium READ isPremium NOTIFY premiumChanged)
    Q_PROPERTY(bool purchaseInProgress READ purchaseInProgress NOTIFY purchaseInProgressChanged)

public:
    explicit SettingsController(AppState* appState, QObject* parent = nullptr);

    // Language (0=EN,1=FR,2=DE,3=RU,4=IT,5=ZH_CN,6=ZH_TW,7=ES,8=PT)
    int languageIndex() const { return m_languageIndex; }
    Q_INVOKABLE void setLanguageIndex(int idx);

    // Theme (0=system, 1=light, 2=dark)
    int themeMode() const { return m_themeMode; }
    Q_INVOKABLE void setThemeMode(int mode);

    // Premium / IAP
    bool isPremium() const { return m_premium.isPremium(); }
    bool purchaseInProgress() const { return m_premium.purchaseInProgress(); }
    Q_INVOKABLE void setPremium(bool v);
    Q_INVOKABLE void requestSubscription();
    Q_INVOKABLE void restorePurchases();
    Q_INVOKABLE void shareReport(const QString& format);
    // Share an already-generated report file (no regeneration).
    // Caller manages file lifecycle — SettingsController does NOT delete it.
    Q_INVOKABLE void shareExistingReport(const QString& filePath, const QString& format);

    // Persistence
    void loadSettings();
    void saveSettings();

signals:
    void languageChanged();
    void themeChanged();
    void premiumChanged();
    void premiumRequired();
    void purchaseInProgressChanged();
    void restoreCompleted(bool restoredAny, bool isError);

private:
    AppState* m_appState;
    PremiumStore m_premium;
    int m_languageIndex = 0;
    int m_themeMode = 2; // dark default
};
