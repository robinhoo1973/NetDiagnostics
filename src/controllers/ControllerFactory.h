// =============================================================================
// ControllerFactory.h — Platform dispatch for diagnostic controllers
// =============================================================================
#pragma once
#include <memory>
#include "controllers/INetworkController.h"
#include "controllers/IHttpClient.h"
#include "controllers/INetworkProbe.h"

class ControllerFactory {
public:
    static std::unique_ptr<INetworkController> createNetworkController();
    static std::unique_ptr<IHttpClient> createHttpClient();
    static std::unique_ptr<INetworkProbe> createNetworkProbe();
};
