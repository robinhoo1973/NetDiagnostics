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

// Callback invoked after restore completes.
// restoredAny=true → at least one previous purchase was restored
// isError=true     → the restore operation itself failed (network, StoreKit error)
//                     restoredAny is false in this case
using RestoreCallback = std::function<void(bool restoredAny, bool isError)>;

// Initiate the purchase flow for the Premium product.
// Safe to call from any thread; callback always invoked on the main thread.
void platformStartPurchase(StoreCallback callback);

// Restore previously purchased entitlements.
// Safe to call from any thread; callback always invoked on the main thread.
void platformRestorePurchases(RestoreCallback callback);
