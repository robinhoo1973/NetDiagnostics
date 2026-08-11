// =============================================================================
// DiagnosticMeta.h — Per-test metadata registry (single source of truth)
//
// Replaces scattered per-test information across DeviceCapability, DiagNames,
// and DetailPage duck-typing.  Every diagnostic declares its platform
// availability + detail-page display profile in ONE place.
//
// Usage:
//   auto& m = diagnosticMeta(id);
//   if (m.platforms & Platform::Android) { ... }
//   r.data["showTerminal"] = m.detail.showTerminal;
// =============================================================================
#pragma once

#include "Common/Model/DiagId.h"
#include "Common/Model/DiagNames.h"
#include <cstdint>

// ── Platform bitmask ─────────────────────────────────────────────────────
namespace Platform {
    enum Flag : uint8_t {
        iOS     = 1 << 0,
        Android = 1 << 1,
        Windows = 1 << 2,
        macOS   = 1 << 3,
        Linux   = 1 << 4,
        Desktop = Windows | macOS | Linux,
        Mobile  = iOS | Android,
        All     = 0xFF,
    };
}

// ── Detail page display profile ──────────────────────────────────────────
struct DetailProfile {
    // Which body sections are visible on the detail page.
    bool showErrorOutput : 1;   // red error block (connection/protocol failures)
    bool showProperties  : 1;   // key-value property rows
    bool showCharts      : 1;   // BarChart / Gauge visualization
    bool showTerminal    : 1;   // terminal output (raw protocol data)

    // Key metric: which r.data field provides the headline number on the
    // MetricCard.  nullptr → no MetricCard (System template).
    const char* keyMetricField = nullptr;   // e.g. "rttAvgMs", "hopCount", "totalMs"
    const char* keyMetricUnit  = nullptr;   // e.g. "ms", "hops", "days"
    int         keyMetricPrecision = 0;     // decimal places for display

    // Chart configuration (when showCharts is true).
    enum ChartType { NoChart, BarChart, Gauge };
    ChartType   chartType = NoChart;

    // Terminal: whether to show typing animation (concise output only).
    bool terminalTypewriter : 1;
};

// ── Per-test metadata ────────────────────────────────────────────────────
struct DiagnosticMeta {
    DiagId       id;
    const char*  displayName;    // English display name
    const char*  iconName;       // SVG icon (without .svg)
    uint8_t      platforms;      // Platform::Flag bitmask
    DiagAnimType animType;       // L4 animation category
    DiagTemplateType tmplType;   // L5 detail page template
    DetailProfile detail;        // Detail page display configuration
};

// ── Registry accessor ────────────────────────────────────────────────────
// Returns the metadata for a given DiagId.  id must be a valid enum value.
const DiagnosticMeta& diagnosticMeta(DiagId id);
