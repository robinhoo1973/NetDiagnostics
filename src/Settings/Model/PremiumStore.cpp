// =============================================================================
// PremiumStore.cpp — extracted from AppState.cpp (~80 lines)
// =============================================================================
#include "Settings/Model/PremiumStore.h"
#include <QSettings>
#include <QTimer>

#if defined(PLATFORM_IOS)
#include "Common/Platform/PlatformStore.h"
#endif

PremiumStore::PremiumStore(QObject* parent) : QObject(parent) {
    // Restore persisted premium entitlement (non-consumable IAP)
    QSettings s;
    m_isPremium = s.value(QStringLiteral("premium/unlocked"), false).toBool();

#if defined(PLATFORM_IOS)
    // 5WHY: The StoreKit transaction observer was only installed lazily when
    // a purchase/restore started.  A deferred ("Ask to Buy") transaction
    // approved while the app was closed, or a promoted purchase, was never
    // delivered after relaunch — the user paid but Premium stayed locked.
    // Install the observer at startup and grant Premium for any purchased
    // transaction that arrives outside an active purchase dialog.
    platformInitStore([this]() {
        setPremium(true);
    });

    // 5WHY: The entitlement lived only in QSettings.  A reinstall, device
    // change, or data clear lost a paid purchase with no automatic recovery —
    // users had to know to tap "Restore" in Settings.  A silent restore on
    // launch re-derives the entitlement from the App Store so the purchase
    // survives; the manual Restore button remains as a fallback.  Delayed so
    // it never competes with startup UI; no toast is shown (silent).
    QTimer::singleShot(1500, this, [this]() {
        if (m_isPremium || m_purchaseInProgress) return;
        platformRestorePurchases([this](bool restoredAny, bool /*isError*/) {
            if (restoredAny) setPremium(true);
            // Silent: intentionally do NOT emit restoreCompleted — that signal
            // drives the Settings "Restore" toast for user-initiated restores.
        });
    });
#endif
}

void PremiumStore::setPremium(bool v) {
    if (m_isPremium == v) return;
    m_isPremium = v;
    QSettings().setValue(QStringLiteral("premium/unlocked"), v);
    emit premiumChanged();
}

bool PremiumStore::supportsIap() const {
    // 5WHY: Premium is sold on iOS (StoreKit), Android (Google Play Billing —
    // backend still future) and macOS (Mac App Store).  On Windows/Linux there
    // is no store backend: report sharing is FREE — the UI must not lock the
    // share buttons or offer a Subscribe CTA there.
#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID) || defined(Q_OS_MACOS)
    return true;
#else
    return false;
#endif
}

void PremiumStore::requestSubscription() {
    if (m_isPremium) return;
    if (m_purchaseInProgress) return;
    if (m_purchaseDeferred) return;  // Ask-to-Buy still pending — no duplicate payment

#if defined(PLATFORM_IOS)
    m_purchaseInProgress = true;
    emit purchaseInProgressChanged();

    platformStartPurchase(
        [this](bool success) {
            // Success/failure (or the eventual resolution of a deferred txn).
            m_purchaseDeferred = false;
            m_purchaseInProgress = false;
            emit purchaseInProgressChanged();
            if (success) setPremium(true);
        },
        [this]() {
            // 5WHY: "Ask to Buy" defers the transaction (parent approval
            // pending).  The UI must not keep the spinner spinning
            // indefinitely — clear it and inform the user.  onPurchaseDone
            // stays alive on the ObjC side, so the approved transaction still
            // resolves through the success callback above.  m_purchaseDeferred
            // blocks a duplicate purchase while the first is pending.
            m_purchaseDeferred = true;
            m_purchaseInProgress = false;
            emit purchaseInProgressChanged();
            emit purchaseDeferred();
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
    if (m_purchaseDeferred) return;  // a pending approval is still in flight

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
