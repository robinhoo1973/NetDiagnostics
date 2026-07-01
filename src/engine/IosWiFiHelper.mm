// =============================================================================
// IosWiFiHelper.mm — iOS WiFi + Cellular info retrieval (Objective-C++)
// =============================================================================
// WiFi: NEHotspotNetwork fetchCurrentWithCompletionHandler:
// Cellular: CTTelephonyNetworkInfo + CTCarrier
// =============================================================================

#ifdef PLATFORM_IOS

#include <QString>
#include <QVariantMap>
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
    __block NSString* ssid = nil;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);

    if (@available(iOS 14.0, *)) {
        [NEHotspotNetwork fetchCurrentWithCompletionHandler:^(NEHotspotNetwork* _Nullable network) {
            if (network && network.SSID.length > 0) {
                ssid = [network.SSID copy];
            }
            dispatch_semaphore_signal(sem);
        }];
        dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC));
    }

    if (ssid && ssid.length > 0)
        return QString::fromNSString(ssid);
    return QString();
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

// ── WiFi info ───────────────────────────────────────────────────────────────

QVariantMap iosWiFiInfo()
{
    QVariantMap info;
    __block NSString* ssid = nil;
    __block NSString* bssid = nil;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);

    if (@available(iOS 14.0, *)) {
        [NEHotspotNetwork fetchCurrentWithCompletionHandler:^(NEHotspotNetwork* _Nullable network) {
            if (network) {
                if (network.SSID && network.SSID.length > 0)
                    ssid = [network.SSID copy];
                if (network.BSSID && network.BSSID.length > 0)
                    bssid = [network.BSSID copy];
            }
            dispatch_semaphore_signal(sem);
        }];
        dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC));
    }

    if (ssid && ssid.length > 0)
        info["ssid"] = QString::fromNSString(ssid);
    else
        info["ssid"] = QString();

    if (bssid && bssid.length > 0)
        info["bssid"] = QString::fromNSString(bssid);
    else
        info["bssid"] = QString();

    // Add diagnostics
    if (!ssid || ssid.length == 0)
        info["wifiDiagnostics"] = QStringLiteral("WiFi: Not connected or permission denied (requires NSLocalNetworkUsageDescription + NSBonjourServiceTypes)");

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
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    bool hasCarrier = false;
    if (@available(iOS 12.0, *)) {
        NSDictionary<NSString*, CTCarrier*>* providers = netInfo.serviceSubscriberCellularProviders;
        if (providers && providers.count > 0) {
            for (NSString* key in providers) {
                CTCarrier* carrier = providers[key];
                if (carrier.carrierName && carrier.carrierName.length > 0) {
                    hasCarrier = true;
                    info["carrierName"] = QString::fromNSString(carrier.carrierName);
                    if (carrier.mobileCountryCode)
                        info["mcc"] = QString::fromNSString(carrier.mobileCountryCode);
                    if (carrier.mobileNetworkCode)
                        info["mnc"] = QString::fromNSString(carrier.mobileNetworkCode);
                    if (carrier.isoCountryCode)
                        info["isoCountry"] = QString::fromNSString(carrier.isoCountryCode);
                    break; // first active carrier
                }
            }
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
