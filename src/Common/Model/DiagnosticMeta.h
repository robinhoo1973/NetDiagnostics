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

    // 5WHY (2026-08-20 用户诉求 "属性卡一团混乱数据"): 多实例检测
    // （N 块网卡 × M 个字段）压进单层扁平 kv 列表无层级可读。
    // Grouped = 每实例一个子组（标题=实例名，子项=字段行）。
    enum PropLayout { Kv, Grouped };
    PropLayout propLayout = Kv;

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

// §6.1（5WHY DIAG-1）：meta.platforms 位掩码由 AdapterRegistry 启动时自动推导
// 并覆写（单一权威），不再手工维护两套平台表。编译期表仅作基线。
void setMetaPlatformOverride(DiagId id, unsigned flags);
unsigned effectiveMetaPlatforms(DiagId id);
