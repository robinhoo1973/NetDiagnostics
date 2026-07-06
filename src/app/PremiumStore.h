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

signals:
    void premiumChanged();
    void premiumRequired();       // emitted when a premium-gated action is attempted
    void purchaseInProgressChanged();
    void restoreCompleted(bool restoredAny, bool isError);

private:
    bool m_isPremium = false;
    bool m_purchaseInProgress = false;
};
