// =============================================================================
// AppColors.h — Single source of truth for ALL brand/semantic/theme colors
//
// MIRROR FILE: keep in sync with src/Common/View/theme/Palette.js
//   When adding or changing a color, update BOTH files.
//   Naming conventions are identical between C++ and QML.
//
// USAGE:
//   C++:  QStringLiteral(APPC_SURFACE_LIGHT)  or  APPC_SURFACE_LIGHT
//   QML:  Palette.Light.surface               or  Palette.Dark.surface
//
// COLOR CATEGORIES:
//   Theme surface  — backgrounds, cards, inputs, nav bars    (light + dark)
//   Theme brand    — primary, secondary, accent, cyan        (light + dark)
//   Theme text     — text levels, border, focus              (light + dark)
//   Status         — semantic: green/yellow/red/gray/blue    (light + dark)
//   Report-only    — card backgrounds, code blocks, footers  (light + dark)
// =============================================================================
#pragma once

// ═══════════════════════════════════════════════════════════════════════════════
// THEME SURFACE COLORS
// ═══════════════════════════════════════════════════════════════════════════════

// ── Dark palette ──────────────────────────────────────────────────────────────
#define APPC_SURFACE_DARK            "#0F172A"
#define APPC_SIDEBAR_DARK            "#0F172A"
#define APPC_CARD_DARK               "#1E293B"
#define APPC_INPUT_DARK              "#334155"
#define APPC_NAVBAR_DARK             "#0F172A"

// ── Light palette ─────────────────────────────────────────────────────────────
#define APPC_SURFACE_LIGHT           "#F8FAFC"
#define APPC_SIDEBAR_LIGHT           "#FFFFFF"
#define APPC_CARD_LIGHT              "#FFFFFF"
#define APPC_INPUT_LIGHT             "#F1F5F9"
#define APPC_NAVBAR_LIGHT            "#FFFFFF"

// ═══════════════════════════════════════════════════════════════════════════════
// THEME BRAND COLORS
// ═══════════════════════════════════════════════════════════════════════════════

// ── Dark palette ──────────────────────────────────────────────────────────────
#define APPC_PRIMARY_DARK            "#60C8F8"
#define APPC_PRIMARY_CONTAINER_DARK  "#0C4A6E"
#define APPC_SECONDARY_DARK          "#818CF8"
#define APPC_ACCENT_DARK             "#FB7185"
#define APPC_CYAN_DARK               "#68E5F4"

// ── Light palette ─────────────────────────────────────────────────────────────
#define APPC_PRIMARY_LIGHT           "#0EA5E9"
#define APPC_PRIMARY_CONTAINER_LIGHT "#E0F2FE"
#define APPC_SECONDARY_LIGHT         "#6366F1"
#define APPC_ACCENT_LIGHT            "#F43F5E"
#define APPC_CYAN_LIGHT              "#06B6D4"

// ═══════════════════════════════════════════════════════════════════════════════
// THEME TEXT & BORDER COLORS
// ═══════════════════════════════════════════════════════════════════════════════

// ── Dark palette ──────────────────────────────────────────────────────────────
#define APPC_TEXT_PRIMARY_DARK       "#F1F5F9"
#define APPC_TEXT_SECONDARY_DARK     "#94A3B8"
#define APPC_TEXT_MUTED_DARK         "#94A3B8"
#define APPC_BORDER_CARD_DARK        "#334155"
#define APPC_BORDER_SUBTLE_DARK      "#1E293B"
#define APPC_BORDER_FOCUSED_DARK     "#60C8F8"
#define APPC_TEXT_ON_ACCENT          "#0F172A"  // invariant — dark text on colored bg

// ── Light palette ─────────────────────────────────────────────────────────────
#define APPC_TEXT_PRIMARY_LIGHT      "#0F172A"
#define APPC_TEXT_SECONDARY_LIGHT    "#475569"
#define APPC_TEXT_MUTED_LIGHT        "#64748B"
#define APPC_BORDER_CARD_LIGHT       "#E2E8F0"
#define APPC_BORDER_SUBTLE_LIGHT     "#F1F5F9"
#define APPC_BORDER_FOCUSED_LIGHT    "#0EA5E9"

// ═══════════════════════════════════════════════════════════════════════════════
// SEMANTIC STATUS COLORS
// ═══════════════════════════════════════════════════════════════════════════════

// ── Dark palette (used in dark-theme UI + HTML report dark mode) ──────────────
#define APPC_PASS_GREEN_DARK         "#4ADE80"
#define APPC_WARN_YELLOW_DARK        "#FBBF24"
#define APPC_FAIL_RED_DARK           "#F87171"
#define APPC_SKIP_GRAY_DARK          "#9CA3AF"
#define APPC_INFO_BLUE_DARK          "#A5B4FC"

// ── Light palette (used in light-theme UI + HTML report light mode) ───────────
// 5WHY: Dark-palette status colors (#4ADE80, #FBBF24, etc.) fail WCAG 3:1
// minimum for graphical objects on light #F8FAFC surfaces.  These values
// meet 3:1+, making status icons recognizable in light theme.
#define APPC_PASS_GREEN_LIGHT        "#059669"
#define APPC_WARN_YELLOW_LIGHT       "#EA580C"
#define APPC_FAIL_RED_LIGHT          "#DC2626"
#define APPC_SKIP_GRAY_LIGHT         "#6B7280"
#define APPC_INFO_BLUE_LIGHT         "#2563EB"

// ═══════════════════════════════════════════════════════════════════════════════
// REPORT-SPECIFIC COLORS (HTML/PDF report generation)
// ═══════════════════════════════════════════════════════════════════════════════

// ── Dark report theme ─────────────────────────────────────────────────────────
#define APPC_REPORT_DARK_BG_HEADER   "#0C4A6E"
#define APPC_REPORT_DARK_BG_SECTION  "#1E293B"
#define APPC_REPORT_DARK_BG_ROW_ALT  "#1E293B"
#define APPC_REPORT_DARK_BG_ROW      "#0F172A"
#define APPC_REPORT_DARK_BG_CARD_PASS "#16281b"
#define APPC_REPORT_DARK_BG_CARD_INFO "#141f33"
#define APPC_REPORT_DARK_BG_CARD_WARN "#2b2810"
#define APPC_REPORT_DARK_BG_CARD_FAIL "#2b1616"
#define APPC_REPORT_DARK_BG_CARD_SKIP "#1e1e2e"
#define APPC_REPORT_DARK_BORDER       "#334155"
#define APPC_REPORT_DARK_CODE_BG      "#0a0a14"
#define APPC_REPORT_DARK_CODE_FG      "#c0c0d0"
#define APPC_REPORT_DARK_DETAIL_BG    "#0f1629"
#define APPC_REPORT_DARK_FOOTER       "#5a5a72"
#define APPC_REPORT_DARK_HEADER_TARGET "#E2E8F0"
#define APPC_REPORT_DARK_HEADER_META  "#94A3B8"

// ── Light report theme ────────────────────────────────────────────────────────
#define APPC_REPORT_LIGHT_BG_HEADER  "#0F172A"
#define APPC_REPORT_LIGHT_BG_SECTION "#0F172A"
#define APPC_REPORT_LIGHT_BG_ROW_ALT "#F8FAFC"
#define APPC_REPORT_LIGHT_BG_ROW     "#FFFFFF"
#define APPC_REPORT_LIGHT_BG_CARD_PASS "#ECFDF5"
#define APPC_REPORT_LIGHT_BG_CARD_INFO "#EFF6FF"
#define APPC_REPORT_LIGHT_BG_CARD_WARN "#FFFBEB"
#define APPC_REPORT_LIGHT_BG_CARD_FAIL "#FEF2F2"
#define APPC_REPORT_LIGHT_BG_CARD_SKIP "#F1F5F9"
#define APPC_REPORT_LIGHT_BORDER      "#E2E8F0"
#define APPC_REPORT_LIGHT_CODE_BG     "#0F172A"
#define APPC_REPORT_LIGHT_CODE_FG     "#E2E8F0"
#define APPC_REPORT_LIGHT_DETAIL_BG   "#F8FAFC"
#define APPC_REPORT_LIGHT_FOOTER      "#94A3B8"
#define APPC_REPORT_LIGHT_HEADER_TARGET "#E2E8F0"
#define APPC_REPORT_LIGHT_HEADER_META  "#94A3B8"

// ── Status icon (renderStatusIcon) — QColor RGB integer components ────────────
// These match the light-palette hex colors above, expressed as 0xRRGGBB.
#define APPC_PASS_GREEN_RGB          0x059669
#define APPC_WARN_YELLOW_RGB         0xEA580C
#define APPC_FAIL_RED_RGB            0xDC2626
#define APPC_SKIP_GRAY_RGB           0x6B7280
#define APPC_INFO_BLUE_RGB           0x2563EB

// ── Progress bar thresholds ───────────────────────────────────────────────────
#define APPC_PROGRESS_BAR_BG         "#1E293B"  // track — always dark
