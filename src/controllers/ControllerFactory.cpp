// =============================================================================
// ControllerFactory.cpp — Single #ifdef point for platform controller selection
// =============================================================================
#include "controllers/ControllerFactory.h"

#ifdef PLATFORM_IOS
#include "controllers/ios/IosNetworkController.h"
#include "controllers/ios/IosHttpClient.h"
#include "controllers/ios/IosNetworkProbe.h"
#elif defined(PLATFORM_ANDROID)
// Android: use iOS controller stubs (they delegate to shared G1G2G3Native/G5WebsiteUrl/NetworkProbe)
#include "controllers/ios/IosNetworkController.h"
#include "controllers/ios/IosHttpClient.h"
#include "controllers/ios/IosNetworkProbe.h"
#else
#include "controllers/desktop/DesktopNetworkController.h"
#include "controllers/desktop/DesktopHttpClient.h"
#include "controllers/desktop/DesktopNetworkProbe.h"
#endif

std::unique_ptr<INetworkController> ControllerFactory::createNetworkController() {
#ifdef PLATFORM_IOS
    return std::make_unique<IosNetworkController>();
#else
    return std::make_unique<DesktopNetworkController>();
#endif
}

std::unique_ptr<IHttpClient> ControllerFactory::createHttpClient() {
#ifdef PLATFORM_IOS
    return std::make_unique<IosHttpClient>();
#else
    return std::make_unique<DesktopHttpClient>();
#endif
}

std::unique_ptr<INetworkProbe> ControllerFactory::createNetworkProbe() {
#ifdef PLATFORM_IOS
    return std::make_unique<IosNetworkProbe>();
#else
    return std::make_unique<DesktopNetworkProbe>();
#endif
}
