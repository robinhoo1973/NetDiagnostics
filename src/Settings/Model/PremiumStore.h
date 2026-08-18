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
    // RESERVED API: blocking restore that sets m_purchaseInProgress=true and
    // blocks Buy/Restore while in flight.  No UI entry currently calls this —
    // PremiumDialog/SettingsScreen use probeRestore() below (non-blocking
    // auto-probe).  Kept as a Q_INVOKABLE for future explicit "Restore" UI.
    Q_INVOKABLE void restorePurchases();
    // 5WHY (iOS b21294): PremiumDialog.openDialog() auto-probes the store for
    // a previous purchase.  It previously reused restorePurchases(), which
    // sets m_purchaseInProgress=true — while the probe was in flight, tapping
    // Buy or Restore hit `if (m_purchaseInProgress) return;` and did NOTHING
    // (buttons appeared dead).  probeRestore() is the SAME StoreKit call but
    // WITHOUT touching m_purchaseInProgress, so the Buy/Restore buttons stay
    // live while the probe runs.  It still emits restoreCompleted so the UI
    // can confirm a restored purchase.
    Q_INVOKABLE void probeRestore();

    // True on platforms that SELL Premium: iOS/Android/macOS.  Sharing is
    // gated behind Premium only here; on Windows/Linux it is free.
    bool isPremiumPlatform() const;

    // True only on platforms with a REAL store backend (iOS StoreKit today;
    // Android GPB / macOS StoreKit are future).  The UI gates the Subscribe/
    // Restore buttons on this so a platform never offers a purchase that
    // cannot complete (Apple/Google review policy).
    // 5WHY (复核 2026-08-18): 原为普通成员函数——QML 经 qmlRegisterSingletonInstance
    // 只能访问 Q_PROPERTY/Q_INVOKABLE，`PremiumStore.supportsIap` 绑定报
    // TypeError 且 visible 保持默认 true（fail-open）。补 Q_INVOKABLE。
    Q_INVOKABLE bool supportsIap() const;

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
