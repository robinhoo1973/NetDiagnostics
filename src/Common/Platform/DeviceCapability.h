// =============================================================================
// DeviceCapability.h — Runtime (device/hardware) capability probing
//
// Complements the OS-level manifest (DiagCapability.h).  While the OS
// manifest is compile-time, device presence is runtime: a desktop without a
// WiFi adapter, a WiFi-only tablet (no cellular modem), or a Mac without an
// Ethernet port cannot produce real results for the corresponding tests.
//
//   G1WifiDiagnostics   → requires a WiFi interface
//   G1CellularInfo      → requires a cellular modem
//   G1WiredDiagnostics  → requires a wired Ethernet interface
//   everything else     → device-independent (returns true)
//
// Results are cached and refreshed via invalidateCache() at the start of
// each diagnostic run.  The combined OS+device check is diagRunnable().
// =============================================================================
#pragma once

#include "Common/Model/DiagId.h"
#include "Common/Model/DiagCapability.h"

namespace DeviceCapability {

// Re-probe hardware on the next query (call at run start).
void invalidateCache();

// True when the CURRENT device has the hardware the diagnostic needs.
bool diagSupportedOnDevice(DiagId id);

// Combined OS + device check — the single predicate used everywhere a test
// list is built (Config page), scheduled (run) or counted (stats).
inline bool diagRunnable(DiagId id) {
    return diagSupportedOnPlatform(id) && diagSupportedOnDevice(id);
}

} // namespace DeviceCapability
