// =============================================================================
// PremiumStore.cpp — extracted from AppState.cpp (~80 lines)
// =============================================================================
#include "Settings/Model/PremiumStore.h"
#include <QSettings>

#if defined(PLATFORM_IOS)
#include "Common/Platform/PlatformStore.h"
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
    // 5WHY: Premium was granted directly on non-iOS — any Android/desktop
    // user got Premium free. Now gated: debug grants freely, release builds
    // show a Premium-required toast instead of silently granting.
#if defined(ND_DEBUG) || !defined(NDEBUG)
    // Debug/development: grant Premium directly so share flow is testable
    setPremium(true);
#else
    // Release: Premium is a paid feature. Emit required signal so the UI
    // shows the subscription prompt instead of silently granting access.
    emit premiumRequired();
#endif
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
