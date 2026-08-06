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

    // True when the platform has a real store backend (iOS StoreKit today).
    // Android/desktop have no purchase path yet — the UI must not offer a
    // Subscribe button that can never complete.
    bool supportsIap() const;

signals:
    void premiumChanged();
    void premiumRequired();       // emitted when a premium-gated action is attempted
    void purchaseInProgressChanged();
    // Emitted when a purchase is deferred ("Ask to Buy" — parent approval
    // pending).  The UI clears its busy indicator; the transaction still
    // grants premium when approved.
    void purchaseDeferred();
    void restoreCompleted(bool restoredAny, bool isError);

private:
    bool m_isPremium = false;
    bool m_purchaseInProgress = false;
    bool m_purchaseDeferred = false;  // Ask-to-Buy approval pending
};
