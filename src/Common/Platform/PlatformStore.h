// =============================================================================
// PlatformStore.h — In-App Purchase abstraction (mobile only)
// =============================================================================
// Presents the native store purchase UI:
//   iOS     — StoreKit (SKProductsRequest + SKPaymentQueue)
//   Android — Google Play Billing (future)
// Not defined on desktop (desktop grants Premium directly).
#pragma once
#include <functional>

// Callback invoked on the main thread after a purchase completes.
// success=true  → purchase verified, caller should setPremium(true)
// success=false → user cancelled, payment restricted, or product not found
using StoreCallback = std::function<void(bool success)>;

// Callback invoked on the main thread when a purchase is deferred
// (iOS "Ask to Buy" — parent approval pending).  The caller should clear its
// busy indicator and inform the user; the deferred transaction still resolves
// later via StoreCallback (approval) or the unattended grant path.
using DeferredCallback = std::function<void()>;

// Callback invoked on the main thread when a purchased transaction for the
// Premium product arrives while NO purchase dialog is active — e.g. an
// "Ask to Buy" transaction approved while the app was closed, or a promoted
// purchase from the App Store product page.  Caller should setPremium(true).
using GrantCallback = std::function<void()>;

// Callback invoked after restore completes.
// restoredAny=true → at least one previous purchase was restored
// isError=true     → the restore operation itself failed (network, StoreKit error)
//                     restoredAny is false in this case
using RestoreCallback = std::function<void(bool restoredAny, bool isError)>;

// Install the StoreKit transaction observer once at app startup and register
// the unattended-grant handler.  MUST run before the UI is shown so pending /
// deferred transactions (approved while the app was closed, promoted
// purchases) are delivered after a relaunch.  Safe to call from any thread;
// idempotent.
void platformInitStore(GrantCallback unattendedGrant);

// Initiate the purchase flow for the Premium product.
// Safe to call from any thread; callbacks always invoked on the main thread.
// deferred is invoked if the store defers the transaction (Ask to Buy).
void platformStartPurchase(StoreCallback callback, DeferredCallback deferred);

// Restore previously purchased entitlements.
// Safe to call from any thread; callback always invoked on the main thread.
void platformRestorePurchases(RestoreCallback callback);
