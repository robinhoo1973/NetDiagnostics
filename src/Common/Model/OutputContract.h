// =============================================================================
// OutputContract.h — Result→detail-card mapping (contract layer, §8 / A3)
//
// A3: OutputContract is a READ VIEW over DiagnosticMeta::DetailProfile (the
// single store).  contractFor(id) derives from diagnosticMeta(id).detail, so
// there is exactly one source of truth for "result field → card" mapping.
// =============================================================================
#pragma once

#include "Common/Model/DiagId.h"
#include "Common/Model/DiagnosticMeta.h"

struct OutputContract {
    struct Metric { const char* field; const char* unit; int precision; };
    Metric metric = {nullptr, nullptr, 0};   // → PageMetricSection；null → 无 MetricCard

    bool showError   = true;                 // → PageErrorSection
    bool showProps   = true;                 // → PagePropertiesSection
    struct Chart { enum Type { None, BarChart, Gauge } type; const char* field; };
    Chart chart = {Chart::None, nullptr};    // → PageChartsSection
    bool showTerminal = true;                // → PageTerminalSection

    // ── Derive from DetailProfile (A3 single source) ──────────────────────
    static OutputContract from(const DetailProfile& d) {
        OutputContract c;
        c.metric = { d.keyMetricField, d.keyMetricUnit, d.keyMetricPrecision };
        c.showError = d.showErrorOutput;
        c.showProps = d.showProperties;
        c.showTerminal = d.showTerminal;
        const char* chartField = d.chartField ? d.chartField : d.keyMetricField;
        switch (d.chartType) {
            case DetailProfile::BarChart: c.chart = {Chart::BarChart, chartField}; break;
            case DetailProfile::Gauge:    c.chart = {Chart::Gauge,    chartField}; break;
            default:                      c.chart = {Chart::None, nullptr}; break;
        }
        return c;
    }
};

// Per-test contract = DetailProfile-derived view (A3).
inline OutputContract contractFor(DiagId id) {
    return OutputContract::from(diagnosticMeta(id).detail);
}

// Six template contracts kept for group docs readability; all derive from
// the same DetailProfile store at lookup time (no duplicated data).
