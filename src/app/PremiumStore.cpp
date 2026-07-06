// =============================================================================
// PremiumStore.cpp — extracted from AppState.cpp (~80 lines)
// =============================================================================
#include "app/PremiumStore.h"
#include <QSettings>

#if defined(PLATFORM_IOS)
#include "util/PlatformStore.h"
#endif

PremiumStore::PremiumStore(QObject* parent) : QObject(parent) {
    // Restore persisted premium entitlement (non-consumable IAP)
    QSettings s;
    m_isPremium = s.value(QStringLiteral("premium/unlocked"), false).toBool();
}

void PremiumStore::setPremium(bool v) {
    if (m_isPremium == v) return;
    m_isPremium = v;
    QSettings().setValue(QStringLiteral("premium/unlocked"), v);
    emit premiumChanged();
}

void PremiumStore::requestSubscription() {
    if (m_isPremium) return;
    if (m_purchaseInProgress) return;

#if defined(PLATFORM_IOS)
    m_purchaseInProgress = true;
    emit purchaseInProgressChanged();

    platformStartPurchase([this](bool success) {
        m_purchaseInProgress = false;
        emit purchaseInProgressChanged();
        if (success) setPremium(true);
    });
#else
    // Android / Desktop: store SDK not wired yet — grant Premium directly
    // so the share flow remains usable. Replace with platformStartPurchase
    // once Google Play Billing is integrated.
    setPremium(true);
#endif
}

void PremiumStore::restorePurchases() {
    if (m_isPremium) return;
    if (m_purchaseInProgress) return;

#if defined(PLATFORM_IOS)
    m_purchaseInProgress = true;
    emit purchaseInProgressChanged();

    platformRestorePurchases([this](bool restoredAny, bool isError) {
        m_purchaseInProgress = false;
        emit purchaseInProgressChanged();
        if (restoredAny) setPremium(true);
        emit restoreCompleted(restoredAny, isError);
    });
#else
    emit restoreCompleted(false, false);
#endif
}
