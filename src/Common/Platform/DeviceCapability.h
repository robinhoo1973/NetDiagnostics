// =============================================================================
// DeviceCapability.h — Runtime device-probe authority (NEW-4)
//
// Single owner of hardware-presence probing (WiFi/ethernet/cellular).  Results
// are cached; invalidateCache() refreshes before a run (existing semantics).
// PlatformAdapter does NOT carry devicePredicate (NEW-4) — this is the only
// device-probe path.
// =============================================================================
#pragma once

#include "Common/Model/DiagId.h"

class DeviceCapability {
public:
    // Does the CURRENT device have the hardware a test requires?
    static bool diagSupportedOnDevice(DiagId id);

    // Refresh cached hardware probes (call before a diagnostic run).
    // NEW-4/R1-2: clears THE single probe cache — probes re-run lazily.
    static void invalidateCache();
};
