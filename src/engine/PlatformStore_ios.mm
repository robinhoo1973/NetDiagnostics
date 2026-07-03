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

#ifdef PLATFORM_IOS

#include "util/PlatformStore.h"

#import <StoreKit/StoreKit.h>
#import <Foundation/Foundation.h>

// ── Product ID (must match App Store Connect) ────────────────────────────
static NSString* const kPremiumProductID = @"com.netdiagnostic.app.premium";

// =========================================================================
// NetDiagStoreObserver — singleton that routes StoreKit events to C++
// =========================================================================
@interface NetDiagStoreObserver : NSObject <SKPaymentTransactionObserver>
+ (instancetype)shared;
/// Fired when a purchase started via platformStartPurchase completes.
/// YES = purchased, NO = cancelled / failed. Consumed on first fire.
@property (copy) void(^onPurchaseDone)(BOOL success);
/// Fired when restoreCompletedTransactions finishes (or fails).
/// YES = at least one previous purchase was restored.
@property (copy) void(^onRestoreDone)(BOOL restoredAny);
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
    [[SKPaymentQueue defaultQueue] addTransactionObserver:self];
}

- (void)startPurchase {
    SKMutablePayment* payment = [[SKMutablePayment alloc] init];
    payment.productIdentifier = kPremiumProductID;
    payment.quantity = 1;
    [[SKPaymentQueue defaultQueue] addPayment:payment];
}

- (void)startRestore {
    self.isRestoring  = YES;
    self.hasRestored  = NO;
    [[SKPaymentQueue defaultQueue] restoreCompletedTransactions];
}

// ── SKPaymentTransactionObserver ─────────────────────────────────────────

- (void)paymentQueue:(SKPaymentQueue *)queue
 updatedTransactions:(NSArray<SKPaymentTransaction*>*)transactions
{
    for (SKPaymentTransaction* txn in transactions) {

        switch (txn.transactionState) {

            case SKPaymentTransactionStatePurchasing:
                // User is still interacting with the payment sheet.
                break;

            case SKPaymentTransactionStatePurchased: {
                [queue finishTransaction:txn];
                if (self.onPurchaseDone) {
                    self.onPurchaseDone(YES);
                    self.onPurchaseDone = nil;
                }
                break;
            }

            case SKPaymentTransactionStateFailed: {
                [queue finishTransaction:txn];
                if (self.onPurchaseDone) {
                    // Treat user-cancelled and error the same — callback fires
                    // so AppState can clear the in-progress flag.
                    self.onPurchaseDone(NO);
                    self.onPurchaseDone = nil;
                }
                break;
            }

            case SKPaymentTransactionStateRestored: {
                // Restored transactions arrive here during
                // restoreCompletedTransactions. Finish and flag.
                [queue finishTransaction:txn];
                self.hasRestored = YES;
                break;
            }

            case SKPaymentTransactionStateDeferred:
                // "Ask to Buy" — parent approval pending.  Don't fire the
                // callback; a fresh .purchased or .failed will arrive later.
                break;
        }
    }
}

- (void)paymentQueueRestoreCompletedTransactionsFinished:(SKPaymentQueue *)queue {
    self.isRestoring = NO;
    if (self.onRestoreDone) {
        self.onRestoreDone(self.hasRestored);
        self.onRestoreDone = nil;
    }
}

- (void)paymentQueue:(SKPaymentQueue *)queue
restoreCompletedTransactionsFailedWithError:(NSError *)error
{
    self.isRestoring = NO;
    if (self.onRestoreDone) {
        self.onRestoreDone(NO);
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

        // Move the std::function to the heap so the block can capture it.
        auto cb = std::make_shared<StoreCallback>(std::move(callback));
        obs.onPurchaseDone = ^(BOOL success) {
            (*cb)(success);
        };

        [obs startPurchase];
    });
}

void platformRestorePurchases(StoreCallback callback) {
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
        obs.onRestoreDone = ^(BOOL restoredAny) {
            (*cb)(restoredAny);
        };

        [obs startRestore];
    });
}

#endif // PLATFORM_IOS
