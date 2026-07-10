// =============================================================================
// PlatformStore_ios.mm — iOS StoreKit in-app purchase (ARC)
// =============================================================================
// Links against StoreKit.framework on iOS. Provides two functions:
//   platformStartPurchase(cb)   — present the purchase dialog for Premium
//   platformRestorePurchases(cb) — restore a previous non-consumable purchase
//
// Uses the classic StoreKit APIs (SKProductsRequest / SKPaymentQueue / delegate
// callbacks) because StoreKit 2's Product/Transaction types are Swift-only and
// this project is Objective-C++.
//
// Threading: all StoreKit delegate callbacks arrive on the main queue, so the
// C++ StoreCallback functors are invoked there as well.
// =============================================================================

#if defined(PLATFORM_IOS)

#include "platform/PlatformStore.h"

#import <StoreKit/StoreKit.h>
#import <Foundation/Foundation.h>

// ── Product ID (must match App Store Connect) ────────────────────────────
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
/// AppState should clear the progress spinner. onPurchaseDone is kept alive
/// so the deferred transaction can grant premium when approved later.
@property (copy) void(^onPurchaseDeferred)(void);
/// Fired when restoreCompletedTransactions finishes (or fails).
/// restoredAny=YES means ≥1 previous purchase was restored.
/// isError=YES means the restore operation itself failed (network, etc.).
@property (copy) void(^onRestoreDone)(BOOL restoredAny, BOOL isError);
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

    // ── Fetch the product so the payment sheet shows the localized price ─
    NSSet* productIDs = [NSSet setWithObject:kPremiumProductID];
    SKProductsRequest* req = [[SKProductsRequest alloc] initWithProductIdentifiers:productIDs];
    req.delegate = self;
    [req start];
}

- (void)startRestore {
    self.isRestoring  = YES;
    self.hasRestored  = NO;
    [[SKPaymentQueue defaultQueue] restoreCompletedTransactions];
}

// ── SKProductsRequestDelegate ─────────────────────────────────────────────

- (void)productsRequest:(SKProductsRequest *)request
     didReceiveResponse:(SKProductsResponse *)response
{
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
    if (self.onPurchaseDone) {
        self.onPurchaseDone(NO);
        self.onPurchaseDone = nil;
    }
}

// ── SKPaymentTransactionObserver ─────────────────────────────────────────

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
                // Notify AppState to clear the progress spinner but keep
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
        // Finish ALL purchasing/failed/purchased transactions.
        for (SKPaymentTransaction* txn in transactions) {
            if (txn.transactionState == SKPaymentTransactionStatePurchased ||
                txn.transactionState == SKPaymentTransactionStateFailed) {
                [queue finishTransaction:txn];
            }
        }

        if (self.onPurchaseDone) {
            self.onPurchaseDone(hasPurchased);
            self.onPurchaseDone = nil;
        }
    }
}

- (void)paymentQueueRestoreCompletedTransactionsFinished:(SKPaymentQueue *)queue {
    self.isRestoring = NO;
    if (self.onRestoreDone) {
        self.onRestoreDone(self.hasRestored, /*isError=*/NO);
        self.onRestoreDone = nil;
    }
}

- (void)paymentQueue:(SKPaymentQueue *)queue
restoreCompletedTransactionsFailedWithError:(NSError *)error
{
    self.isRestoring = NO;
    if (self.onRestoreDone) {
        self.onRestoreDone(/*restoredAny=*/NO, /*isError=*/YES);
        self.onRestoreDone = nil;
    }
}

@end

// =========================================================================
// C++ bridge — called from AppState on the Qt (main) thread
// =========================================================================

void platformStartPurchase(StoreCallback callback) {
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
        obs.onPurchaseDone = ^(BOOL success) {
            (*cb)(success);
        };

        // Wire the deferred notification — AppState clears the spinner
        // while the transaction waits for parent approval.
        obs.onPurchaseDeferred = ^{
            // The C++ callback is NOT consumed — we keep onPurchaseDone
            // alive.  AppState will reset purchaseInProgress externally
            // via the success/failure path when the deferred txn resolves.
        };

        [obs startPurchase];
    });
}

void platformRestorePurchases(RestoreCallback callback) {
    dispatch_async(dispatch_get_main_queue(), ^{
        NetDiagStoreObserver* obs = [NetDiagStoreObserver shared];

        // Prevent overlapping operations
        if (obs.onPurchaseDone != nil) {
            callback(false, /*isError=*/false);
            return;
        }
        if (obs.isRestoring) {
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

#endif // PLATFORM_IOS
