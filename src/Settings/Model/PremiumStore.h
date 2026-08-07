// =============================================================================
// PremiumStore.h — In-app purchase / subscription management.
// Extracted from AppState (~80 lines).
//
// Handles: premium unlock, purchase flow, restore, persistence.
// Platform-specific backends: StoreKit (iOS), Google Play Billing (Android/future),
// direct grant (Desktop development).
// =============================================================================
#pragma once

#include <QObject>

class PremiumStore : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool isPremium READ isPremium NOTIFY premiumChanged)
    Q_PROPERTY(bool purchaseInProgress READ purchaseInProgress NOTIFY purchaseInProgressChanged)

public:
    explicit PremiumStore(QObject* parent = nullptr);

    bool isPremium() const { return m_isPremium; }
    bool purchaseInProgress() const { return m_purchaseInProgress; }

    Q_INVOKABLE void setPremium(bool v);
    Q_INVOKABLE void requestSubscription();
    Q_INVOKABLE void restorePurchases();

    // True on platforms that SELL Premium: iOS/Android/macOS.  Sharing is
    // gated behind Premium only here; on Windows/Linux it is free.
    bool isPremiumPlatform() const;

    // True only on platforms with a REAL store backend (iOS StoreKit today;
    // Android GPB / macOS StoreKit are future).  The UI gates the Subscribe/
    // Restore buttons on this so a platform never offers a purchase that
    // cannot complete (Apple/Google review policy).
    bool supportsIap() const;

signals:
    void premiumChanged();
    void premiumRequired();       // emitted when a premium-gated action is attempted
    void purchaseInProgressChanged();
    // Emitted when a purchase is deferred ("Ask to Buy" — parent approval
    // pending).  The UI clears its busy indicator; the transaction still
    // grants premium when approved.
    void purchaseDeferred();
    // Emitted when a purchase fails or is cancelled by the user — the UI
    // should clear its busy state and show an explicit message.
    void purchaseFailed();
    void restoreCompleted(bool restoredAny, bool isError);

private:
    bool m_isPremium = false;
    bool m_purchaseInProgress = false;
    bool m_purchaseDeferred = false;  // Ask-to-Buy approval pending
};
