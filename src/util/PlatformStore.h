// =============================================================================
// PlatformStore.h — In-App Purchase abstraction (mobile only)
// =============================================================================
// Presents the native store purchase UI:
//   iOS     — StoreKit (SKProductsRequest + SKPaymentQueue)
//   Android — Google Play Billing (future)
// Not defined on desktop (desktop grants Premium directly).
#pragma once
#include <functional>

// Callback invoked on the main thread after a purchase or restore completes.
// success=true  → purchase verified, caller should setPremium(true)
// success=false → user cancelled, network error, or product not found
using StoreCallback = std::function<void(bool success)>;

// Initiate the purchase flow for the Premium product.
// Must be called from the main/Qt thread.
void platformStartPurchase(StoreCallback callback);

// Restore previously purchased entitlements.
// Must be called from the main/Qt thread.
void platformRestorePurchases(StoreCallback callback);
