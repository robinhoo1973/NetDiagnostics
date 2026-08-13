// =============================================================================
// Adapters.cpp — registerAllAdapters() central entry.
//
// All five groups (G1..G5) are real implementations with the NEW-1 platform
// bitmask, so verifyAllDiagIds() cross-checks registry against meta and
// suite totals are stable.
// =============================================================================
#include "Diagnostics/Model/Adapters.h"
#include "Common/Services/PlatformAdapter.h"

void registerAllAdapters() {
    registerG1Adapters();   // System & Adapters (G1/Adapters.cpp)
    registerG2Adapters();   // Connectivity & Security (G2/Adapters.cpp)
    registerG3Adapters();   // Internet & DNS (G3/Adapters.cpp)
    registerG4Adapters();   // Remote Host (G4/Adapters.cpp)
    registerG5Adapters();   // Protocol (G5/Adapters.cpp)
}
