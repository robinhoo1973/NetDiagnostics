// =============================================================================
// Palette.js — QML palette constants (single source of truth for QML colors)
// =============================================================================
.pragma library

var Dark = {
    surface:          "#0F172A",
    sidebar:          "#0F172A",
    card:             "#1E293B",
    input:            "#334155",
    navBar:           "#0F172A",
    scrim:            "#66000000",   // 浮层遮罩（不随主题）

    primary:          "#60C8F8",
    primaryContainer: "#0C4A6E",
    secondary:        "#818CF8",
    accent:           "#FB7185",
    cyan:             "#68E5F4",

    textPrimary:      "#F1F5F9",
    textSecondary:    "#94A3B8",
    textMuted:        "#8494A8",
    textPlaceholder:  "#64748B",
    borderCard:       "#334155",
    borderSubtle:     "#1E293B",
    borderFocused:    "#60C8F8",
    textOnAccent:     "#0F172A",

    passGreen:        "#4ADE80",
    warnYellow:       "#F59E0B",
    warnOrange:       "#EA580C",   // 计时中档（token 化，替代硬编码）
    failRed:          "#F87171",
    errorRed:         "#F472B6",
    skipGray:         "#9CA3AF",
    infoBlue:         "#A5B4FC"
};

var Light = {
    surface:          "#F8FAFC",
    sidebar:          "#FFFFFF",
    card:             "#FFFFFF",
    input:            "#F1F5F9",
    navBar:           "#FFFFFF",
    scrim:            "#66000000",

    primary:          "#0EA5E9",
    primaryContainer: "#E0F2FE",
    secondary:        "#6366F1",
    accent:           "#F43F5E",
    cyan:             "#06B6D4",

    textPrimary:      "#0F172A",
    textSecondary:    "#475569",
    textMuted:        "#64748B",
    textPlaceholder:  "#94A3B8",
    borderCard:       "#E2E8F0",
    borderSubtle:     "#F1F5F9",
    borderFocused:    "#0EA5E9",
    textOnAccent:     "#0F172A",

    passGreen:        "#10B981",
    warnYellow:       "#EA580C",
    warnOrange:       "#EA580C",
    failRed:          "#DC2626",
    errorRed:         "#BE185D",
    skipGray:         "#6B7280",
    infoBlue:         "#2563EB"
};

Object.freeze(Dark);
Object.freeze(Light);
