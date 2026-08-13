// =============================================================================
// DiagnosticMeta.h — Per-test metadata registry (contract layer, single source)
//
// Per diag-execution-architecture-guide.md §5/§8: DiagnosticMeta is the single
// store for tmplType + DetailProfile (+ durationProfile).  OutputContract is a
// read view derived from DetailProfile (A3).  platforms are the NEW-1 code-
// verified values (DiagCapability manifest + TaskFactory #if); at startup the
// AdapterRegistry derivation is authoritative and may overwrite this field.
// =============================================================================
#pragma once

#include "Common/Model/DiagId.h"
#include "Common/Model/DiagNames.h"
#include "Common/Platform/PlatformFlags.h"
#include <cstdint>

// ── Detail page display profile ──────────────────────────────────────────
struct DetailProfile {
    bool showErrorOutput  = true;   // PageErrorSection
    bool showProperties   = true;   // PagePropertiesSection
    bool showCharts       = false;  // PageChartsSection
    bool showTerminal     = true;   // PageTerminalSection

    const char* keyMetricField = nullptr;   // data 键 → MetricCard；nullptr=无
    const char* keyMetricUnit  = nullptr;
    int         keyMetricPrecision = 0;

    enum ChartType { NoChart, BarChart, Gauge };
    ChartType chartType = NoChart;
    const char* chartField = nullptr;   // 图表数据键：individualRtts / hops / waterfall（R1-1：Gauge 同 key 字段）

    bool terminalTypewriter = false;
};

// ── Per-test metadata ────────────────────────────────────────────────────
struct DiagnosticMeta {
    DiagId       id;
    const char*  displayName;
    const char*  iconName;
    unsigned     platforms;       // PlatformFlag::Flag（NEW-1 代码实证值）
    DiagAnimType animType;
    DiagTemplateType tmplType;
    DetailProfile detail;
    qint64       durationProfileMs = 60000; // P2 DIAG-14: 时长预算（沿用 Task 超时）
};

// ── Registry accessors ───────────────────────────────────────────────────
const DiagnosticMeta& diagnosticMeta(DiagId id);
