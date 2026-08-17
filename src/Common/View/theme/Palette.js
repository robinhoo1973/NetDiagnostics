// =============================================================================
// Palette.js — M3 semantic color roles (single source of truth for QML)
//
// 架构（2026-08-17 M3 化 + 精简）：对齐 Material 3 color roles 的精简子集
//   https://m3.material.io/styles/color/the-color-system/color-roles
//   · primary/secondary/tertiary/error 基础色 + on-* 文字配对（container
//     家族仅在组件实际采用时引入——零消费方角色已移除）
//   · surface + 三级容器（Low/Container/Highest）
//   · outline（3:1 边界）与 outlineVariant（装饰分隔）分离
// 扩展令牌（NetDiagnostics 专属）：status 状态色、accent、textMuted、
//   textPlaceholder、groupHues、iconPad*。
//
// 5WHY（2026-08-17 重命名轮）：颜色值一个不动 —— 本文件只改令牌名并补齐
//   M3 角色；新增派生令牌一律复用现有 hex（不引入新烘焙色）。已知对比度
//   偏差不在此修复，由 scripts/audit-palette-contrast.py 审计报告追踪
//   （review/theme-contrast-audit.md）。
// =============================================================================
.pragma library

var Dark = {
    // ── Accent roles (M3) ──
    primary:              "#60C8F8",
    onPrimary:            "#0F172A",   // ← 原 textOnAccent
    primaryContainer:     "#0C4A6E",
    secondary:            "#818CF8",
    onSecondary:          "#0F172A",
    tertiary:             "#68E5F4",   // ← 原 cyan
    onTertiary:           "#0F172A",
    error:                "#F472B6",   // ← 原 errorRed（Error≠Fail 语义保留）
    onError:              "#0F172A",

    // ── Neutral surfaces (M3) ──
    surface:              "#0F172A",
    onSurface:            "#F1F5F9",   // ← 原 textPrimary
    onSurfaceVariant:     "#94A3B8",   // ← 原 textSecondary
    surfaceContainerLow:       "#1E293B",   // ← 原 card
    surfaceContainer:          "#0F172A",   // ← 原 sidebar/navBar
    surfaceContainerHighest:   "#334155",   // ← 原 input
    outline:              "#64748B",
    outlineVariant:       "#334155",   // ← 原 borderCard
    scrim:                "#66000000",   // 浮层遮罩（不随主题）

    // ── Extended: brand & status tokens ──
    accent:               "#FB7185",
    textMuted:            "#8494A8",
    textPlaceholder:      "#64748B",
    success:              "#4ADE80",   // ← 原 passGreen
    warning:              "#F59E0B",   // ← 原 warnYellow
    warningStrong:        "#EA580C",   // ← 原 warnOrange
    fail:                 "#F87171",   // ← 原 failRed
    skip:                 "#9CA3AF",   // ← 原 skipGray
    info:                 "#A5B4FC",   // ← 原 infoBlue
    terminalText:         "#4ADE80",   // 终端输出文本（值=success，语义独立角色）
    onSuccessContainer:   "#4ADE80",   // success 系绿在 success 淡底上的 AA 文字（复用现有 hex）

    // 45 图标全彩常显：瓦片光晕垫令牌 + 5 组组色（G1..G5）——主题无关，
    // 单一来源在 Dark 块（5WHY review 2026-08-17：双块逐字复制会静默漂移，
    // Light 块以引用共享同一数组/数值）。
    iconPadAlpha:         0.12,
    iconPadBorderAlpha:   0.22,
    groupHues:            ["#818CF8", "#38BDF8", "#68E5F4", "#60C8F8", "#A78BFA"]
};

var Light = {
    // ── Accent roles (M3) ──
    primary:              "#0EA5E9",
    onPrimary:            "#0F172A",   // ← 原 textOnAccent
    primaryContainer:     "#E0F2FE",
    secondary:            "#6366F1",
    onSecondary:          "#FFFFFF",
    tertiary:             "#06B6D4",   // ← 原 cyan
    onTertiary:           "#0F172A",
    error:                "#BE185D",   // ← 原 errorRed
    onError:              "#FFFFFF",

    // ── Neutral surfaces (M3) ──
    surface:              "#F8FAFC",
    onSurface:            "#0F172A",   // ← 原 textPrimary
    onSurfaceVariant:     "#475569",   // ← 原 textSecondary
    surfaceContainerLow:       "#FFFFFF",   // ← 原 card
    surfaceContainer:          "#FFFFFF",   // ← 原 sidebar/navBar
    surfaceContainerHighest:   "#F1F5F9",   // ← 原 input
    outline:              "#64748B",
    outlineVariant:       "#E2E8F0",   // ← 原 borderCard
    scrim:                "#66000000",   // 浮层遮罩（不随主题）

    // ── Extended: brand & status tokens ──
    accent:               "#F43F5E",
    textMuted:            "#64748B",
    textPlaceholder:      "#94A3B8",
    success:              "#10B981",   // ← 原 passGreen
    warning:              "#EA580C",   // ← 原 warnYellow
    warningStrong:        "#EA580C",   // ← 原 warnOrange
    fail:                 "#DC2626",   // ← 原 failRed
    skip:                 "#6B7280",   // ← 原 skipGray
    info:                 "#2563EB",   // ← 原 infoBlue
    terminalText:         "#047857",   // 终端输出文本（深翡翠，浅底 ~4.5:1 WCAG AA）
    onSuccessContainer:   "#047857",   // 深翡翠对 success 淡底 ≈4.7:1（AA）

    // 45 图标全彩常显：引用 Dark 块单一来源（主题无关令牌，勿在本块改值）
    iconPadAlpha:         Dark.iconPadAlpha,
    iconPadBorderAlpha:   Dark.iconPadBorderAlpha,
    groupHues:            Dark.groupHues
};

Object.freeze(Dark);
Object.freeze(Light);
