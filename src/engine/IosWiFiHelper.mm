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
    // Store the SSID as a C++ QString (converted inside the handler) so no Objective-C
    // object owned by the handler's autorelease pool crosses the thread boundary.
    struct SsidCtx {
        dispatch_semaphore_t sem;
        QString ssid;
        std::atomic<int> refs;
    };
    auto ctx = std::make_shared<SsidCtx>();
    ctx->sem = dispatch_semaphore_create(0);
    ctx->refs.store(2, std::memory_order_relaxed);

    if (@available(iOS 14.0, *)) {
        [NEHotspotNetwork fetchCurrentWithCompletionHandler:^(NEHotspotNetwork* _Nullable network) {
            @autoreleasepool {
                if (network && network.SSID.length > 0) {
                    ctx->ssid = QString::fromNSString(network.SSID);
                }
                dispatch_semaphore_signal(ctx->sem);
                if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    dispatch_release(ctx->sem);
                }
            }
        }];
        long waited = dispatch_semaphore_wait(ctx->sem, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC));
        if (waited != 0) ctx->ssid.clear(); // timeout: handler may still be writing
    }

    QString result = ctx->ssid;
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
    // Store results as C++ QString (converted inside the handler) so no Objective-C
    // object owned by the handler's autorelease pool ever crosses the thread boundary.
    struct WifiCtx {
        dispatch_semaphore_t sem;
        QString ssid;
        QString bssid;
        std::atomic<int> refs;
    };
    auto ctx = std::make_shared<WifiCtx>();
    ctx->sem = dispatch_semaphore_create(0);
    ctx->refs.store(2, std::memory_order_relaxed);  // waiter + handler

    if (@available(iOS 14.0, *)) {
        [NEHotspotNetwork fetchCurrentWithCompletionHandler:^(NEHotspotNetwork* _Nullable network) {
            @autoreleasepool {
                if (network) {
                    if (network.SSID && network.SSID.length > 0)
                        ctx->ssid = QString::fromNSString(network.SSID);
                    if (network.BSSID && network.BSSID.length > 0)
                        ctx->bssid = QString::fromNSString(network.BSSID);
                }
                dispatch_semaphore_signal(ctx->sem);
                // Drop the handler's reference; last one out releases the semaphore.
                if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    dispatch_release(ctx->sem);
                }
            }
        }];
        long waited = dispatch_semaphore_wait(ctx->sem, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC));
        // Only read result on success; on timeout the handler may still be writing it.
        if (waited != 0) {
            ctx->ssid.clear();
            ctx->bssid.clear();
        }
    }

    info["ssid"] = ctx->ssid;
    info["bssid"] = ctx->bssid;

    // Add diagnostics
    if (ctx->ssid.isEmpty())
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

    // Runs on a QtConcurrent worker thread with no autorelease pool of its own.
    // CTTelephonyNetworkInfo, its provider dictionary, and the NSStrings it returns
    // are all autoreleased; without an explicit pool they leak (Apple requires each
    // secondary thread that makes Cocoa calls to provide its own @autoreleasepool).
    @autoreleasepool {
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
        // Enumerate EVERY SIM / eSIM line. Dual-SIM iPhones return one CTCarrier per
        // active subscription in serviceSubscriberCellularProviders, and
        // serviceCurrentRadioAccessTechnology is keyed by the SAME service identifiers,
        // so each SIM's radio-access type is matched by key.
        QVariantList sims;
        bool hasCarrier = false;
        if (@available(iOS 12.0, *)) {
            NSDictionary<NSString*, CTCarrier*>* providers = netInfo.serviceSubscriberCellularProviders;
            NSDictionary<NSString*, NSString*>* rats = netInfo.serviceCurrentRadioAccessTechnology;
            if (providers && providers.count > 0) {
                // Dictionary order is undefined; sort keys for stable SIM slot numbers.
                NSArray<NSString*>* keys = [providers.allKeys sortedArrayUsingSelector:@selector(compare:)];
                int slot = 0;
                for (NSString* key in keys) {
                    CTCarrier* carrier = providers[key];
                    QVariantMap sim;
                    sim["slot"] = ++slot;

                    QString mccStr, mncStr, carrierName;
                    if (carrier) {
                        if (carrier.carrierName && carrier.carrierName.length > 0) {
                            QString cn = QString::fromNSString(carrier.carrierName);
                            if (cn != "--" && cn != "65535") carrierName = cn;
                        }
                        if (carrier.mobileCountryCode) {
                            QString v = QString::fromNSString(carrier.mobileCountryCode);
                            if (v != "65535") { mccStr = v; sim["mcc"] = v; }
                        }
                        if (carrier.mobileNetworkCode) {
                            QString v = QString::fromNSString(carrier.mobileNetworkCode);
                            if (v != "65535") { mncStr = v; sim["mnc"] = v; }
                        }
                        if (carrier.isoCountryCode)
                            sim["isoCountry"] = QString::fromNSString(carrier.isoCountryCode);
                    }
                    // iOS 16+ hides the carrier name ("--"); fall back to MCC+MNC lookup.
                    if (carrierName.isEmpty() && !mccStr.isEmpty() && !mncStr.isEmpty()) {
                        QString looked = mccMncToCarrier(mccStr, mncStr);
                        if (!looked.isEmpty()) carrierName = looked + " (via MCC/MNC)";
                    }
                    if (!carrierName.isEmpty()) { sim["carrierName"] = carrierName; hasCarrier = true; }

                    // Per-SIM radio access technology, matched by the same service key.
                    NSString* rat = rats ? rats[key] : nil;
                    if (rat) {
                        sim["radioAccess"] = QString::fromNSString(radioAccessLabel(rat));
                        sim["radioAccessRaw"] = QString::fromNSString(rat);
                    }
                    sims.append(sim);
                }
            }
        }
#pragma clang diagnostic pop

        info["simCount"] = static_cast<int>(sims.size());
        if (!sims.isEmpty()) {
            info["sims"] = sims;
            // Flat "primary" keys for backward-compatible summary / identity checks:
            // prefer the first SIM that actually has a carrier or an active radio.
            QVariantMap primary = sims.first().toMap();
            for (const QVariant& v : sims) {
                const QVariantMap m = v.toMap();
                if (m.contains(QStringLiteral("carrierName")) || m.contains(QStringLiteral("radioAccess"))) {
                    primary = m; break;
                }
            }
            const QStringList flat = {QStringLiteral("carrierName"), QStringLiteral("mcc"),
                                      QStringLiteral("mnc"), QStringLiteral("isoCountry"),
                                      QStringLiteral("radioAccess"), QStringLiteral("radioAccessRaw")};
            for (const QString& k : flat)
                if (primary.contains(k)) info[k] = primary.value(k);
        }

        if (!info.contains(QStringLiteral("radioAccess")) && !hasCarrier)
            info["cellularStatus"] = QStringLiteral("No cellular service available (airplane mode or no SIM)");

        [netInfo release]; // MRC: balance the alloc/init above
    }

    // Signal strength is not available via public API (iOS restricts this)
    info["signalNotice"] = QStringLiteral("Signal strength: unavailable (Apple restricts public API access)");
    info["signalNote"] = QStringLiteral("To monitor signal: use Xcode -> Simulator -> I/O -> Cellular");

    return info;
}

#endif // PLATFORM_IOS
