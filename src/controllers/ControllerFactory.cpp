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
#define MAKE_CTRL(prefix, suffix) return std::make_unique<prefix ## suffix>()
std::unique_ptr<INetworkController> ControllerFactory::createNetworkController() { MAKE_CTRL(Ios, NetworkController); }
std::unique_ptr<IHttpClient> ControllerFactory::createHttpClient()             { MAKE_CTRL(Ios, HttpClient); }
std::unique_ptr<INetworkProbe> ControllerFactory::createNetworkProbe()        { MAKE_CTRL(Ios, NetworkProbe); }
#undef MAKE_CTRL
#else
#include "controllers/desktop/DesktopNetworkController.h"
#include "controllers/desktop/DesktopHttpClient.h"
#include "controllers/desktop/DesktopNetworkProbe.h"
#define MAKE_CTRL(prefix, suffix) return std::make_unique<prefix ## suffix>()
std::unique_ptr<INetworkController> ControllerFactory::createNetworkController() { MAKE_CTRL(Desktop, NetworkController); }
std::unique_ptr<IHttpClient> ControllerFactory::createHttpClient()             { MAKE_CTRL(Desktop, HttpClient); }
std::unique_ptr<INetworkProbe> ControllerFactory::createNetworkProbe()        { MAKE_CTRL(Desktop, NetworkProbe); }
#undef MAKE_CTRL
#endif
