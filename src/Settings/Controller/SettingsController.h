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

    // Language (0=ZH_CN,1=ZH_TW,2=JA,3=KO,4=HI,5=VI,6=TR,7=EN,8=FR,9=DE,10=RU,11=IT,12=ES,13=PT,14=AR)
    int languageIndex() const { return m_languageIndex; }
    Q_INVOKABLE void setLanguageIndex(int idx);

    // Theme (1=light, 2=dark).  Value 0 (system) is accepted for backward
    // compatibility with saved settings but treated as dark — the system
    // theme option was removed from the UI in feat(settings).
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
