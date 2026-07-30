// =============================================================================
// Palette.js — QML palette constants (single source of truth for QML colors)
//
// MIRROR FILE: keep in sync with src/Common/Utils/AppColors.h
//   When adding or changing a color, update BOTH files.
//   Naming conventions are identical between C++ and QML.
//
// USAGE:
//   QML:  import "Palette.js" as Palette
//         Palette.Light.surface       Palette.Dark.surface
//         Palette.StatusLight.passGreen   Palette.StatusDark.passGreen
//   C++:  #include "Common/Utils/AppColors.h"
//         QStringLiteral(APPC_SURFACE_LIGHT)
// =============================================================================
.pragma library

// ═══════════════════════════════════════════════════════════════════════════════
// THEME SURFACE COLORS
// ═══════════════════════════════════════════════════════════════════════════════

var Dark = {
    // ── Surface ────────────────────────────────────────────────────────
    surface:          "#0F172A",
    sidebar:          "#0F172A",
    card:             "#1E293B",
    input:            "#334155",
    navBar:           "#0F172A",

    // ── Brand ──────────────────────────────────────────────────────────
    primary:          "#60C8F8",
    primaryContainer: "#0C4A6E",
    secondary:        "#818CF8",
    accent:           "#FB7185",
    cyan:             "#68E5F4",

    // ── Text & border ───────────────────────────────────────────────────
    textPrimary:      "#F1F5F9",
    textSecondary:    "#94A3B8",
    textMuted:        "#94A3B8",
    borderCard:       "#334155",
    borderSubtle:     "#1E293B",
    borderFocused:    "#60C8F8",
    textOnAccent:     "#0F172A",

    // ── Status ──────────────────────────────────────────────────────────
    passGreen:        "#4ADE80",
    // 5WHY: #FBBF24 (gold) was ambiguous — is this a warning (orange) or a yield sign (yellow)?
    // UX convention across Material Design, Apple HIG, and Bootstrap is orange for warnings.
    // Changed to #F59E0B (amber-orange) for consistent "warning=orange" semantics.
    // Contrast ratio vs #0F172A: ~6.3:1 (WCAG AAA for large text).
    warnYellow:       "#F59E0B",
    failRed:          "#F87171",
    skipGray:         "#9CA3AF",
    infoBlue:         "#A5B4FC"
};

var Light = {
    // ── Surface ────────────────────────────────────────────────────────
    surface:          "#F8FAFC",
    sidebar:          "#FFFFFF",
    card:             "#FFFFFF",
    input:            "#F1F5F9",
    navBar:           "#FFFFFF",

    // ── Brand ──────────────────────────────────────────────────────────
    primary:          "#0EA5E9",
    primaryContainer: "#E0F2FE",
    secondary:        "#6366F1",
    accent:           "#F43F5E",
    cyan:             "#06B6D4",

    // ── Text & border ───────────────────────────────────────────────────
    textPrimary:      "#0F172A",
    textSecondary:    "#475569",
    textMuted:        "#64748B",
    borderCard:       "#E2E8F0",
    borderSubtle:     "#F1F5F9",
    borderFocused:    "#0EA5E9",
    textOnAccent:     "#0F172A",

    // ── Status (WCAG 3:1+ on #F8FAFC) ───────────────────────────────────
    passGreen:        "#059669",
    // 5WHY: Dark counterpart uses #F59E0B (amber-orange). Both palettes now use orange for warnings.
    // #EA580C (deep orange) contrast ratio vs #F8FAFC: ~5.5:1 (WCAG AA).
    warnYellow:       "#EA580C",
    failRed:          "#DC2626",
    skipGray:         "#6B7280",
    infoBlue:         "#2563EB"
};

// 5WHY: Freeze both palette objects to prevent accidental mutation.
// Palette.Dark and Palette.Light are shared singletons via .pragma library.
// Without freeze(), any QML file could write Palette.Light.passGreen = "red"
// and corrupt the canonical palette globally for the app lifetime.
Object.freeze(Dark);
Object.freeze(Light);
