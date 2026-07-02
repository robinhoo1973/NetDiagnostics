// =============================================================================
// IosWiFiHelper.mm — iOS WiFi + Cellular info retrieval (Objective-C++)
// =============================================================================
// WiFi: NEHotspotNetwork fetchCurrentWithCompletionHandler:
// Cellular: CTTelephonyNetworkInfo + CTCarrier
// =============================================================================

#ifdef PLATFORM_IOS

#include <QString>
#include <QVariantMap>
#include <atomic>
#include <memory>
#import <NetworkExtension/NetworkExtension.h>
#import <CoreLocation/CoreLocation.h>
#import <CoreTelephony/CTTelephonyNetworkInfo.h>
#import <CoreTelephony/CTCarrier.h>

// ── Authorization ────────────────────────────────────────────────────────────

void iosRequestWiFiAuthorization()
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            iosRequestWiFiAuthorization();
        });
        return;
    }

    if (@available(iOS 13.0, *)) {
        static CLLocationManager* mgr = nil;
        if (!mgr) {
            mgr = [[CLLocationManager alloc] init];
            [mgr requestWhenInUseAuthorization];
        }
    }
}

// ── SSID retrieval ───────────────────────────────────────────────────────────

QString iosCopyWiFiSSID()
{
    // Reference-counted context: waiter and completion handler both hold a ref (2 total).
    struct SsidCtx {
        dispatch_semaphore_t sem;
        NSString* ssid;
        std::atomic<int> refs;
    };
    auto ctx = std::make_shared<SsidCtx>();
    ctx->sem = dispatch_semaphore_create(0);
    ctx->ssid = nil;
    ctx->refs.store(2, std::memory_order_relaxed);

    if (@available(iOS 14.0, *)) {
        [NEHotspotNetwork fetchCurrentWithCompletionHandler:^(NEHotspotNetwork* _Nullable network) {
            if (network && network.SSID.length > 0) {
                ctx->ssid = [network.SSID copy];
            }
            dispatch_semaphore_signal(ctx->sem);
            if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                dispatch_release(ctx->sem);
            }
        }];
        long waited = dispatch_semaphore_wait(ctx->sem, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC));
        if (waited != 0) ctx->ssid = nil; // timeout: handler may still be writing
    }

    QString result;
    if (ctx->ssid && ctx->ssid.length > 0)
        result = QString::fromNSString(ctx->ssid);
    if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        dispatch_release(ctx->sem);
    }
    return result;
}

// ── Cellular info ────────────────────────────────────────────────────────────

static NSString* radioAccessLabel(NSString* rat)
{
    if (!rat) return @"Unknown";
    if ([rat isEqualToString:CTRadioAccessTechnologyNRNSA] ||
        [rat isEqualToString:CTRadioAccessTechnologyNR])
        return @"5G";
    if ([rat isEqualToString:CTRadioAccessTechnologyLTE])
        return @"LTE";
    if ([rat isEqualToString:CTRadioAccessTechnologyWCDMA])
        return @"3G (WCDMA)";
    if ([rat isEqualToString:CTRadioAccessTechnologyHSDPA])
        return @"3G (HSDPA)";
    if ([rat isEqualToString:CTRadioAccessTechnologyHSUPA])
        return @"3G (HSUPA)";
    if ([rat isEqualToString:CTRadioAccessTechnologyCDMA1x])
        return @"2G (CDMA)";
    if ([rat isEqualToString:CTRadioAccessTechnologyCDMAEVDORev0] ||
        [rat isEqualToString:CTRadioAccessTechnologyCDMAEVDORevA] ||
        [rat isEqualToString:CTRadioAccessTechnologyCDMAEVDORevB])
        return @"3G (EV-DO)";
    if ([rat isEqualToString:CTRadioAccessTechnologyEdge])
        return @"2G (EDGE)";
    if ([rat isEqualToString:CTRadioAccessTechnologyGPRS])
        return @"2G (GPRS)";
    if ([rat isEqualToString:CTRadioAccessTechnologyeHRPD])
        return @"3G (eHRPD)";
    return rat;
}

// ── MCC/MNC to carrier name lookup (fallback for iOS 16+) ─────────────────────
// When CTCarrier.carrierName returns "--" on iOS 16+, query the MCC (Mobile Country
// Code) and MNC (Mobile Network Code) to identify the carrier from a mapping table.
// This lookup table contains the major carriers worldwide; you can extend it.
static QString mccMncToCarrier(const QString& mcc, const QString& mnc)
{
    // Format: "MCC-MNC" → "Carrier Name"
    static const QMap<QString, QString> carriers = {
        // China
        {"460-00", "中国移动 (China Mobile)"}, {"460-02", "中国移动 (China Mobile)"},
        {"460-01", "中国联通 (China Unicom)"},
        {"460-03", "中国电信 (China Telecom)"},
        // United States
        {"310-004", "Verizon"},   {"310-010", "Verizon"},   {"310-012", "Verizon"},
        {"310-013", "Verizon"},   {"310-014", "Verizon"},
        {"310-005", "AT&T"},      {"310-070", "AT&T"},      {"310-150", "AT&T"},
        {"310-160", "AT&T"},      {"310-170", "AT&T"},      {"310-200", "AT&T"},
        {"310-210", "AT&T"},      {"310-220", "AT&T"},
        {"310-026", "T-Mobile"},  {"310-160", "T-Mobile"},  {"310-200", "T-Mobile"},
        // UK
        {"234-03", "Vodafone"},   {"234-10", "Vodafone"},
        {"234-15", "Vodafone"},   {"234-30", "O2"},
        {"234-20", "Three"},      {"234-50", "Three"},
        // Germany
        {"262-01", "Telekom"},    {"262-02", "Vodafone"},   {"262-03", "E-Plus"},
        {"262-07", "Telefónica"},
        // France
        {"208-01", "Orange"},     {"208-02", "SFR"},        {"208-03", "Bouygues"},
        // Japan
        {"440-10", "docomo"},     {"440-20", "SoftBank"},   {"440-50", "SoftBank"},
        {"440-04", "au"},         {"440-06", "au"},
        // South Korea
        {"450-02", "KT"},         {"450-04", "SK Telecom"}, {"450-08", "LG U+"},
        // India
        {"404-01", "Airtel"},     {"404-02", "Vodafone"},   {"404-03", "IDEA"},
        {"404-05", "Vodafone"},   {"404-09", "Jio"},
    };
    return carriers.value(mcc + "-" + mnc, QString());
}

// ── WiFi info ───────────────────────────────────────────────────────────────

QVariantMap iosWiFiInfo()
{
    QVariantMap info;

    // Reference-counted context: waiter and completion handler both hold a ref (2 total).
    struct WifiCtx {
        dispatch_semaphore_t sem;
        NSString* ssid;
        NSString* bssid;
        std::atomic<int> refs;
    };
    auto ctx = std::make_shared<WifiCtx>();
    ctx->sem = dispatch_semaphore_create(0);
    ctx->ssid = nil;
    ctx->bssid = nil;
    ctx->refs.store(2, std::memory_order_relaxed);  // waiter + handler

    if (@available(iOS 14.0, *)) {
        [NEHotspotNetwork fetchCurrentWithCompletionHandler:^(NEHotspotNetwork* _Nullable network) {
            if (network) {
                if (network.SSID && network.SSID.length > 0)
                    ctx->ssid = [network.SSID copy];
                if (network.BSSID && network.BSSID.length > 0)
                    ctx->bssid = [network.BSSID copy];
            }
            dispatch_semaphore_signal(ctx->sem);
            // Drop the handler's reference; last one out releases the semaphore.
            if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                dispatch_release(ctx->sem);
            }
        }];
        long waited = dispatch_semaphore_wait(ctx->sem, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC));
        // Only read result on success; on timeout the handler may still be writing it.
        if (waited != 0) {
            ctx->ssid = nil;
            ctx->bssid = nil;
        }
    }

    if (ctx->ssid && ctx->ssid.length > 0)
        info["ssid"] = QString::fromNSString(ctx->ssid);
    else
        info["ssid"] = QString();

    if (ctx->bssid && ctx->bssid.length > 0)
        info["bssid"] = QString::fromNSString(ctx->bssid);
    else
        info["bssid"] = QString();

    // Add diagnostics
    if (!ctx->ssid || ctx->ssid.length == 0)
        info["wifiDiagnostics"] = QStringLiteral("WiFi: Not connected or permission denied (requires NSLocalNetworkUsageDescription + NSBonjourServiceTypes)");

    // Drop the waiter's reference; last one out releases the semaphore.
    if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        dispatch_release(ctx->sem);
    }

    return info;
}

// ── Cellular info ────────────────────────────────────────────────────────────

QVariantMap iosCellularInfo()
{
    QVariantMap info;

    CTTelephonyNetworkInfo* netInfo = [[CTTelephonyNetworkInfo alloc] init];
    if (!netInfo) {
        info["error"] = QStringLiteral("Failed to initialize CTTelephonyNetworkInfo");
        return info;
    }

    // iOS 12+: serviceSubscriberCellularProviders returns per-SIM carriers.
    // CTCarrier and its properties are deprecated since iOS 16.0 with no replacement.
    // We suppress the warnings and keep the best-effort implementation — the values
    // will eventually return placeholder strings ("--", "65535") on future iOS versions.
    // On iOS 16+, when carrierName becomes "--", we fall back to MCC+MNC lookup.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    bool hasCarrier = false;
    QString mccStr, mncStr;
    if (@available(iOS 12.0, *)) {
        NSDictionary<NSString*, CTCarrier*>* providers = netInfo.serviceSubscriberCellularProviders;
        if (providers && providers.count > 0) {
            for (NSString* key in providers) {
                CTCarrier* carrier = providers[key];
                if (carrier) {
                    // Try to get the carrier name
                    QString carrierName;
                    if (carrier.carrierName && carrier.carrierName.length > 0) {
                        carrierName = QString::fromNSString(carrier.carrierName);
                        // Check if it's the placeholder string (iOS 16+ default)
                        if (carrierName != "--" && carrierName != "65535") {
                            info["carrierName"] = carrierName;
                            hasCarrier = true;
                        }
                    }
                    // Always store MCC/MNC for fallback lookup
                    if (carrier.mobileCountryCode) {
                        mccStr = QString::fromNSString(carrier.mobileCountryCode);
                        info["mcc"] = mccStr;
                    }
                    if (carrier.mobileNetworkCode) {
                        mncStr = QString::fromNSString(carrier.mobileNetworkCode);
                        info["mnc"] = mncStr;
                    }
                    if (carrier.isoCountryCode)
                        info["isoCountry"] = QString::fromNSString(carrier.isoCountryCode);
                    break; // first active carrier
                }
            }
        }
    }
    // Fallback: if carrierName is empty or placeholder, try MCC+MNC lookup
    if (!hasCarrier && !mccStr.isEmpty() && !mncStr.isEmpty()) {
        QString looked = mccMncToCarrier(mccStr, mncStr);
        if (!looked.isEmpty()) {
            info["carrierName"] = looked + " (via MCC/MNC)";
            hasCarrier = true;
        }
    }
#pragma clang diagnostic pop

    // Radio access technology
    NSString* rat = netInfo.serviceCurrentRadioAccessTechnology.allValues.firstObject;
    if (rat) {
        info["radioAccess"] = QString::fromNSString(radioAccessLabel(rat));
        info["radioAccessRaw"] = QString::fromNSString(rat);
    } else if (!hasCarrier) {
        info["cellularStatus"] = QStringLiteral("No cellular service available (airplane mode or no SIM)");
    }

    // Signal strength is not available via public API (iOS restricts this)
    info["signalNotice"] = QStringLiteral("Signal strength: unavailable (Apple restricts public API access)");
    info["signalNote"] = QStringLiteral("To monitor signal: use Xcode -> Simulator -> I/O -> Cellular");

    return info;
}

#endif // PLATFORM_IOS
