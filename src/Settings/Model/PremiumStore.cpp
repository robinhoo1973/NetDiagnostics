// =============================================================================
// PremiumStore.cpp — extracted from AppState.cpp (~80 lines)
// =============================================================================
#include "Settings/Model/PremiumStore.h"
#include <QSettings>
#include <QTimer>

// 5WHY: StoreKit is available on both iOS and macOS (same SKPaymentQueue
// APIs); Android GPB remains future work.  The macOS build links the
// macOS PlatformStore.mm which reuses the identical StoreKit flow.
#if defined(PLATFORM_IOS) || defined(Q_OS_MACOS)
#include "Common/Platform/PlatformStore.h"
#endif

PremiumStore::PremiumStore(QObject* parent) : QObject(parent) {
    // Restore persisted premium entitlement (non-consumable IAP)
    QSettings s;
    m_isPremium = s.value(QStringLiteral("premium/unlocked"), false).toBool();

#if defined(PLATFORM_IOS) || defined(Q_OS_MACOS)
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
    // 5WHY (复核 2026-08-18): 自动恢复在每次启动无条件执行——受限设备
    // （Screen Time/MDM）上该调用无意义。增加 canMakePayments 守卫：
    // 不可支付的环境跳过自动恢复（用户仍可手动点 Restore）。
    // 注：Apple 未文档化"未登录 Apple ID"为 canMakePayments=false 的条件，
    // 签名用户的系统登录 sheet 行为不受此守卫约束。
    QTimer::singleShot(1500, this, [this]() {
        if (m_isPremium || m_purchaseInProgress) return;
        if (!platformCanMakePayments()) return;
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

bool PremiumStore::isPremiumPlatform() const {
    // 5WHY: Premium is sold on iOS (StoreKit), Android (Google Play Billing)
    // and macOS (Mac App Store).  On Windows/Linux there is no store and no
    // subscription model: report sharing is FREE.  This flag drives whether
    // the share buttons are locked and whether Settings shows the Premium
    // card — NOT whether a purchase button is shown (that is supportsIap()).
#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID) || defined(Q_OS_MACOS)
    return true;
#else
    return false;
#endif
}

bool PremiumStore::supportsIap() const {
    // 5WHY: iOS and macOS have a real StoreKit backend (same APIs).  Android
    // GPB is future work — offering a Subscribe button there would dead-end
    // the user (Apple/Google review policy forbids UI for purchases that
    // cannot complete).  Buy/Restore buttons gate on this, while
    // isPremiumPlatform() gates the share lock.
#if defined(PLATFORM_IOS) || defined(Q_OS_MACOS)
    return true;
#else
    return false;
#endif
}

void PremiumStore::requestSubscription() {
    if (m_isPremium) return;
    if (m_purchaseInProgress) return;
    if (m_purchaseDeferred) return;  // Ask-to-Buy still pending — no duplicate payment

#if defined(PLATFORM_IOS) || defined(Q_OS_MACOS)
    m_purchaseInProgress = true;
    emit purchaseInProgressChanged();

    platformStartPurchase(
        [this](bool success) {
            // Success/failure (or the eventual resolution of a deferred txn).
            m_purchaseDeferred = false;
            m_purchaseInProgress = false;
            emit purchaseInProgressChanged();
            if (success) setPremium(true);
            // 5WHY: A cancelled/failed purchase was silent — the user tapped
            // "Buy", StoreKit dismissed, and nothing told them what happened.
            // Emit an explicit signal so the UI can show "Purchase cancelled /
            // failed" feedback instead of appearing to ignore the tap.
            else emit purchaseFailed();
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
    // 5WHY (复核 2026-08-18 fail-open 收口): !defined(ND_RELEASE) 是 fail-open
    // ——任何未定义 ND_RELEASE 的打包路径（未来以 Debug 配置误打包、自定义
    // 流水线遗漏 genex）都静默永久解锁。改为显式 ND_DEV（仅 Debug 配置由
    // netdiag-target.cmake 定义）：无 ND_DEV 一律 premiumRequired，判门
    // fail-closed，付费方向不再有静默放行面。
#if defined(ND_DEV)
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

#if defined(PLATFORM_IOS) || defined(Q_OS_MACOS)
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

void PremiumStore::probeRestore() {
    if (m_isPremium) return;
    if (m_purchaseInProgress) return;  // user is actively buying — don't probe
    if (m_purchaseDeferred) return;

#if defined(PLATFORM_IOS) || defined(Q_OS_MACOS)
    // 5WHY (iOS b21294): Unlike restorePurchases(), this does NOT set
    // m_purchaseInProgress — the Buy/Restore buttons stay enabled while the
    // store is probed.  The 30s watchdog in PlatformStore.mm still bounds the
    // call, and the result is surfaced via restoreCompleted so the UI can
    // confirm ("Purchases Restored") when a previous purchase is found.
    platformRestorePurchases([this](bool restoredAny, bool isError) {
        if (restoredAny) setPremium(true);
        emit restoreCompleted(restoredAny, isError);
    });
#else
    emit restoreCompleted(false, false);
#endif
}
