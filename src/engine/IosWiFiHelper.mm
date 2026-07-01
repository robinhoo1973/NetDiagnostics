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

QVariantMap iosCellularInfo()
{
    QVariantMap info;

    CTTelephonyNetworkInfo* netInfo = [[CTTelephonyNetworkInfo alloc] init];
    if (!netInfo) return info;

    // iOS 12+: serviceSubscriberCellularProviders returns per-SIM carriers.
    // CTCarrier and its properties are deprecated since iOS 16.0 with no replacement.
    // We suppress the warnings and keep the best-effort implementation — the values
    // will eventually return placeholder strings ("--", "65535") on future iOS versions.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (@available(iOS 12.0, *)) {
        NSDictionary<NSString*, CTCarrier*>* providers = netInfo.serviceSubscriberCellularProviders;
        if (providers && providers.count > 0) {
            for (NSString* key in providers) {
                CTCarrier* carrier = providers[key];
                if (carrier.carrierName && carrier.carrierName.length > 0) {
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
    }

    info["signalNotice"] = QStringLiteral("Signal strength unavailable on iOS (public API restriction)");

    return info;
}

#endif // PLATFORM_IOS
