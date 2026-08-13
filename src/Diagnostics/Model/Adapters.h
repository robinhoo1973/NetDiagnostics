// =============================================================================
// Adapters.h — Central adapter registration point (DIAG-2/A1)
//
// registerAllAdapters() is called from main() BEFORE verifyAllDiagIds().
// Each group exposes its own registerGxAdapters() (explicit init — no SIOF,
// no linker dead-strip).  All five groups are real implementations.
// =============================================================================
#pragma once

void registerAllAdapters();
void registerG1Adapters();   // G1/Adapters.cpp (real)
void registerG2Adapters();   // G2/Adapters.cpp (real)
void registerG3Adapters();   // G3/Adapters.cpp (real)
void registerG4Adapters();   // G4/Adapters.cpp (real)
void registerG5Adapters();   // G5/Adapters.cpp (real)
