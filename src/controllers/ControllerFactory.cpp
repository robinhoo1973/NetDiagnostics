// =============================================================================
// ControllerFactory.cpp — Single #ifdef point for platform controller selection
// =============================================================================
#include "controllers/ControllerFactory.h"

// Mobile platforms (iOS + Android) use the iOS controller stubs.
// They delegate to shared G1G2G3Native/G5WebsiteUrl/NetworkProbe via #ifndef NO_CURL guards.
#if defined(PLATFORM_IOS) || defined(PLATFORM_ANDROID)
#include "controllers/ios/IosNetworkController.h"
#include "controllers/ios/IosHttpClient.h"
#include "controllers/ios/IosNetworkProbe.h"
#define FACTORY_IOS_BRANCH return std::make_unique<Ios
#else
#include "controllers/desktop/DesktopNetworkController.h"
#include "controllers/desktop/DesktopHttpClient.h"
#include "controllers/desktop/DesktopNetworkProbe.h"
#define FACTORY_IOS_BRANCH return std::make_unique<Desktop
#endif

std::unique_ptr<INetworkController> ControllerFactory::createNetworkController() {
    FACTORY_IOS_BRANCH NetworkController>();
}
std::unique_ptr<IHttpClient> ControllerFactory::createHttpClient() {
    FACTORY_IOS_BRANCH HttpClient>();
}
std::unique_ptr<INetworkProbe> ControllerFactory::createNetworkProbe() {
    FACTORY_IOS_BRANCH NetworkProbe>();
}

#undef FACTORY_IOS_BRANCH
