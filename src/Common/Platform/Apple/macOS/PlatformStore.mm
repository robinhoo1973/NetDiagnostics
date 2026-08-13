// =============================================================================
// PlatformStore_macOS.mm — macOS StoreKit in-app purchase (ARC)
// =============================================================================
// 5WHY: Premium must work on iOS, Android and macOS.  iOS already has a full
// StoreKit backend (Apple/IOS/PlatformStore.mm).  StoreKit's classic APIs
// (SKProductsRequest / SKPaymentQueue / delegate callbacks) are IDENTICAL on
// macOS (10.7+), and the macOS payment sheet is presented automatically by
// SKPaymentQueue — no NSWindow wiring needed.  This file reuses that flow
// verbatim so the restore/place-purchase UX behaves the same on iOS and macOS.
//
// Linked only on macOS (see CMakeLists.txt APPLE AND NOT IOS branch) and
// compiled with -fobjc-arc.
// =============================================================================

// 5WHY (macOS CI b21294 link failure): the guard was previously
// `defined(PLATFORM_IOS) || defined(Q_OS_MACOS)` evaluated at the TOP of the
// file BEFORE any Qt include — Q_OS_MACOS only becomes defined after including
// qglobal.h, so the whole file compiled to nothing and the three platform
// functions were undefined at link time.  Use the compiler built-in __APPLE__
// (always defined on Apple platforms, no include needed) matching WifiHelper.mm.
#if defined(__APPLE__) && !defined(PLATFORM_IOS)

#include "Common/Platform/PlatformStore.h"

#import <StoreKit/StoreKit.h>
#import <Foundation/Foundation.h>

// ── Product ID (must match App Store Connect / the macOS App Store) ─────
static NSString* const kPremiumProductID = @"com.netdiagnostic.app.premium";

// =========================================================================
// NetDiagStoreObserver — singleton that routes StoreKit events to C++
// =========================================================================
@interface NetDiagStoreObserver : NSObject
    <SKPaymentTransactionObserver, SKProductsRequestDelegate>
+ (instancetype)shared;
/// Fired when a purchase started via platformStartPurchase completes.
/// YES = purchased, NO = cancelled / failed. Consumed on first fire.
@property (copy) void(^onPurchaseDone)(BOOL success);
/// Fired when the purchase goes into "Ask to Buy" (parent approval pending).
/// The C++ side clears the progress spinner; onPurchaseDone is kept alive
/// so the deferred transaction can grant premium when approved later.
@property (copy) void(^onPurchaseDeferred)(void);
/// Fired when restoreCompletedTransactions finishes (or fails).
/// restoredAny=YES means ≥1 previous purchase was restored.
/// isError=YES means the restore operation itself failed (network, etc.).
@property (copy) void(^onRestoreDone)(BOOL restoredAny, BOOL isError);
// 5WHY (iOS/macOS b21294): a second restore request arriving while one is
// already in flight (auto-probe + a manual tap) was previously answered
// immediately with callback(false,false) — a false "no purchase found".
// Queue the new callback here and resolve it with the CURRENT restore result.
@property (copy) void(^pendingRestoreDone)(BOOL restoredAny, BOOL isError);
/// Fired (once per session) when a purchased transaction for the Premium
/// product arrives while no purchase dialog is active — an "Ask to Buy"
/// approved while the app was closed, or a promoted purchase.  Installed by
/// platformInitStore at app startup.
@property (copy) void(^onUnattendedGrant)(void);
@property (assign) BOOL didGrantUnattended;  // guard: unattended grant fires once
@property (assign) BOOL hasRequestCompleted; // disarms the products-request watchdog
@property (assign) BOOL isRestoring;    // guard against concurrent restore
@property (assign) BOOL hasRestored;    // set when ≥1 .restored txn arrives
- (void)ensureObserving;
- (void)startPurchase;
- (void)startRestore;
@end

@implementation NetDiagStoreObserver

+ (instancetype)shared {
    static NetDiagStoreObserver* instance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        instance = [[self alloc] init];
    });
    return instance;
}

- (void)ensureObserving {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        [[SKPaymentQueue defaultQueue] addTransactionObserver:self];
    });
}

- (void)startPurchase {
    // ── Guard: payments disabled via Screen Time / parental controls ────
    if (![SKPaymentQueue canMakePayments]) {
        if (self.onPurchaseDone) {
            self.onPurchaseDone(NO);
            self.onPurchaseDone = nil;
        }
        return;
    }

    // ── Watchdog: if the products request neither responds nor fails within
    //    30s (e.g. no connectivity), fail the purchase so the UI is never
    //    stuck in "Processing…" forever.  Disarmed once the request returns
    //    (payment sheet presented) — long-running states after that point are
    //    handled by their own callbacks (deferred/success/failure). ──────
    self.hasRequestCompleted = NO;
    __weak typeof(self) weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(30 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
        typeof(self) strongSelf = weakSelf;
        if (strongSelf && !strongSelf.hasRequestCompleted && strongSelf.onPurchaseDone) {
            strongSelf.onPurchaseDone(NO);
            strongSelf.onPurchaseDone = nil;
        }
    });

    // ── Fetch the product so the payment sheet shows the localized price ─
    NSSet* productIDs = [NSSet setWithObject:kPremiumProductID];
    SKProductsRequest* req = [[SKProductsRequest alloc] initWithProductIdentifiers:productIDs];
    req.delegate = self;
    [req start];
}

- (void)startRestore {
    self.isRestoring  = YES;
    self.hasRestored  = NO;
    // 5WHY (iOS/macOS b21294): restoreCompletedTransactions may never call
    // back if the store is unreachable — neither the Finished nor the
    // FailedWithError delegate fires.  Without a watchdog the C++ side's
    // m_purchaseInProgress stays true forever, silently disabling BOTH the
    // Buy and Restore buttons.  Mirror the purchase watchdog: fail after 30s
    // so the UI recovers and the user can retry.
    __weak typeof(self) weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(30 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
        typeof(self) strongSelf = weakSelf;
        if (strongSelf && strongSelf.isRestoring) {
            // Resolve both the active and any queued restore callback so no
            // button stays stuck in "Processing…" after a silent timeout.
            [strongSelf resolveRestoreWithResult:NO isError:YES];
        }
    });
    [[SKPaymentQueue defaultQueue] restoreCompletedTransactions];
}

// ── SKProductsRequestDelegate ─────────────────────────────────────────────

- (void)productsRequest:(SKProductsRequest *)request
     didReceiveResponse:(SKProductsResponse *)response
{
    self.hasRequestCompleted = YES;  // disarm the products-request watchdog
    SKProduct* product = response.products.firstObject;
    if (!product) {
        // Product not found in App Store Connect — invalid Product ID.
        if (self.onPurchaseDone) {
            self.onPurchaseDone(NO);
            self.onPurchaseDone = nil;
        }
        return;
    }

    SKPayment* payment = [SKPayment paymentWithProduct:product];
    [[SKPaymentQueue defaultQueue] addPayment:payment];
}

- (void)request:(SKRequest *)request didFailWithError:(NSError *)error {
    self.hasRequestCompleted = YES;  // disarm the products-request watchdog
    if (self.onPurchaseDone) {
        self.onPurchaseDone(NO);
        self.onPurchaseDone = nil;
    }
}

// ── Promoted purchases (App Store product-page "Buy" button) ───────────
// Return YES so the payment flows through the normal observer path; the
// resulting .purchased transaction grants premium via the unattended path.
- (BOOL)paymentQueue:(SKPaymentQueue *)queue
shouldAddStorePayment:(SKPayment *)payment
             forProduct:(SKProduct *)product
{
    return YES;
}

// ── SKPaymentTransactionObserver ─────────────────────────────────────────

// Only the Premium product unlocks the feature.  Transactions for any other
// product ID are finished to keep the queue clean but never touch callbacks
// or grant state — otherwise adding a second IAP product would accidentally
// unlock Premium for a different purchase.
static BOOL isPremiumTxn(SKPaymentTransaction* txn) {
    return [txn.payment.productIdentifier isEqualToString:kPremiumProductID];
}

- (void)paymentQueue:(SKPaymentQueue *)queue
 updatedTransactions:(NSArray<SKPaymentTransaction*>*)transactions
{
    // ── Scan the full batch before invoking any callback.
    //     If a stale .failed and a fresh .purchased arrive together,
    //     .purchased wins — otherwise the callback would be consumed by
    //     the .failed handler before the .purchased is seen. ──────────
    BOOL hasPurchased = NO;
    BOOL hasFailed    = NO;

    for (SKPaymentTransaction* txn in transactions) {
        if (!isPremiumTxn(txn)) {
            // Foreign / unrelated transaction — finish and ignore it so the
            // payment queue never blocks on stale transactions.
            if (txn.transactionState == SKPaymentTransactionStatePurchased ||
                txn.transactionState == SKPaymentTransactionStateFailed ||
                txn.transactionState == SKPaymentTransactionStateRestored) {
                [queue finishTransaction:txn];
            }
            continue;
        }

        switch (txn.transactionState) {

            case SKPaymentTransactionStatePurchasing:
                break;

            case SKPaymentTransactionStatePurchased:
                hasPurchased = YES;
                break;

            case SKPaymentTransactionStateFailed:
                hasFailed = YES;
                break;

            case SKPaymentTransactionStateRestored:
                [queue finishTransaction:txn];
                self.hasRestored = YES;
                break;

            case SKPaymentTransactionStateDeferred:
                // "Ask to Buy" — parent approval pending.
                // Notify the C++ side to clear the progress spinner but keep
                // onPurchaseDone alive so the deferred transaction will
                // grant premium when the parent approves later.
                if (self.onPurchaseDeferred) {
                    self.onPurchaseDeferred();
                }
                break;
        }
    }

    // ── Batch outcome: .purchased wins over .failed ────────────────────
    if (hasPurchased || hasFailed) {
        // Finish ALL premium purchasing/failed/purchased transactions.
        for (SKPaymentTransaction* txn in transactions) {
            if (isPremiumTxn(txn) &&
                (txn.transactionState == SKPaymentTransactionStatePurchased ||
                 txn.transactionState == SKPaymentTransactionStateFailed)) {
                [queue finishTransaction:txn];
            }
        }

        if (hasPurchased && self.onPurchaseDone) {
            // Active purchase flow resolved successfully.
            self.onPurchaseDone(YES);
            self.onPurchaseDone = nil;
        } else if (hasPurchased && !self.onPurchaseDone && !self.didGrantUnattended) {
            // No active purchase dialog (deferred txn approved after relaunch,
            // or a promoted purchase) — grant through the startup-installed
            // unattended path, once per session.
            self.didGrantUnattended = YES;
            if (self.onUnattendedGrant) {
                self.onUnattendedGrant();
            }
        }

        if (hasFailed && self.onPurchaseDone) {
            self.onPurchaseDone(NO);
            self.onPurchaseDone = nil;
        }
    }
}

// ── Helper: resolve the current restore + any queued restore request ────
// 5WHY (iOS/macOS b21294): auto-probe + manual tap can overlap.  Resolve both
// the active onRestoreDone and any pendingRestoreDone with the same result so
// a queued manual tap never gets a spurious "no purchase found".
- (void)resolveRestoreWithResult:(BOOL)restoredAny isError:(BOOL)isError {
    self.isRestoring = NO;
    if (self.onRestoreDone) {
        self.onRestoreDone(restoredAny, isError);
        self.onRestoreDone = nil;
    }
    if (self.pendingRestoreDone) {
        self.pendingRestoreDone(restoredAny, isError);
        self.pendingRestoreDone = nil;
    }
}

- (void)paymentQueueRestoreCompletedTransactionsFinished:(SKPaymentQueue *)queue {
    [self resolveRestoreWithResult:self.hasRestored isError:NO];
}

- (void)paymentQueue:(SKPaymentQueue *)queue
restoreCompletedTransactionsFailedWithError:(NSError *)error
{
    [self resolveRestoreWithResult:NO isError:YES];
}

@end

// =========================================================================
// C++ bridge — called from AppState on the Qt (main) thread
// =========================================================================

void platformInitStore(GrantCallback unattendedGrant) {
    dispatch_async(dispatch_get_main_queue(), ^{
        NetDiagStoreObserver* obs = [NetDiagStoreObserver shared];
        [obs ensureObserving];

        // 5WHY: The observer was only installed lazily when a purchase/restore
        // started, so a deferred ("Ask to Buy") transaction approved while the
        // app was closed was never delivered on relaunch — the user had paid
        // but Premium stayed locked until they manually bought/restored again.
        // Installing at startup + this unattended grant covers that case.
        auto grant = std::make_shared<GrantCallback>(std::move(unattendedGrant));
        obs.onUnattendedGrant = ^{
            (*grant)();
        };
    });
}

void platformStartPurchase(StoreCallback callback, DeferredCallback deferred) {
    dispatch_async(dispatch_get_main_queue(), ^{
        NetDiagStoreObserver* obs = [NetDiagStoreObserver shared];

        // Prevent overlapping operations
        if (obs.onPurchaseDone != nil) {
            callback(false);
            return;
        }
        if (obs.isRestoring) {
            callback(false);
            return;
        }

        [obs ensureObserving];

        auto cb = std::make_shared<StoreCallback>(std::move(callback));
        auto dcb = std::make_shared<DeferredCallback>(std::move(deferred));
        obs.onPurchaseDone = ^(BOOL success) {
            (*cb)(success);
        };

        // Wire the deferred notification — the C++ side clears the spinner
        // and informs the user; onPurchaseDone stays alive so the deferred
        // transaction grants premium when the parent approves later.
        obs.onPurchaseDeferred = ^{
            (*dcb)();
        };

        [obs startPurchase];
    });
}

void platformRestorePurchases(RestoreCallback callback) {
    dispatch_async(dispatch_get_main_queue(), ^{
        NetDiagStoreObserver* obs = [NetDiagStoreObserver shared];

        // 5WHY (iOS/macOS b21294): a restore already in flight (auto-probe
        // from PremiumDialog, or a prior manual tap) must NOT abort the new
        // request with a spurious callback(false,false).  Queue it and resolve
        // with the current restore's outcome (see resolveRestoreWithResult).
        if (obs.isRestoring) {
            auto cb = std::make_shared<RestoreCallback>(std::move(callback));
            obs.pendingRestoreDone = ^(BOOL restoredAny, BOOL isError) {
                (*cb)(restoredAny, isError);
            };
            return;
        }

        // Prevent overlapping with an active PURCHASE (payment sheet up).
        if (obs.onPurchaseDone != nil) {
            callback(false, /*isError=*/false);
            return;
        }

        [obs ensureObserving];

        auto cb = std::make_shared<RestoreCallback>(std::move(callback));
        obs.onRestoreDone = ^(BOOL restoredAny, BOOL isError) {
            (*cb)(restoredAny, isError);
        };

        [obs startRestore];
    });
}

#endif // __APPLE__ && !PLATFORM_IOS
