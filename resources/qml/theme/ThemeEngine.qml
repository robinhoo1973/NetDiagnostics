// =============================================================================
// ThemeEngine.qml — Runtime theme controller (singleton)
//
// Provides light/dark/system theme switching with Material Design 3 inspired
// design tokens.  All QML components reference ThemeEngine.colors.* instead
// of hardcoded color values.
//
// Usage: ThemeEngine.mode = ThemeEngine.Dark  // or Light / System
//        color: ThemeEngine.colors.surface
// =============================================================================
pragma Singleton
import QtQuick

QtObject {
    // ── Theme mode ────────────────────────────────────────────────────
    readonly property int System: 0
    readonly property int Light:  1
    readonly property int Dark:   2

    property int mode: Dark  // default to dark (current behavior)

    // OS color scheme detection disabled for stability.
    // Qt.styleHints.colorScheme access causes crashes on Windows static
    // and iOS builds (QML binding evaluation timing / static init order).
    // Default to dark — users can manually switch via Settings → Appearance.
    property bool systemIsDark: true

    readonly property int effectiveMode: mode === System ? (systemIsDark ? Dark : Light) : mode

    // ── Light palette (Material Design 3 tokens) ─────────────────────
    readonly property var light: ({
        // Surface
        surface: "#F8FAFC",
        surfaceVariant: "#F1F5F9",
        card: "#FFFFFF",
        input: "#F1F5F9",
        sidebar: "#FFFFFF",
        navBar: "#FFFFFF",

        // Primary
        primary: "#0EA5E9",
        primaryContainer: "#E0F2FE",
        onPrimary: "#FFFFFF",

        // Secondary
        secondary: "#6366F1",
        secondaryContainer: "#EEF2FF",

        // Text
        textPrimary: "#0F172A",
        textSecondary: "#475569",
        textMuted: "#94A3B8",
        textOnPrimary: "#FFFFFF",

        // Status
        accent: "#F43F5E",
        cyan: "#06B6D4",
        passGreen: "#10B981",
        warnYellow: "#F59E0B",
        failRed: "#EF4444",
        skipGray: "#9CA3AF",
        infoBlue: "#3B82F6",

        // Borders
        borderCard: "#E2E8F0",
        borderSubtle: "#F1F5F9",
        borderFocused: "#0EA5E9",

        // Overlay
        overlay: "#0F172A",
        hover: "#0F172A",
        ripple: "#0F172A",

        // Shadows
        shadow: "#000000"
    })

    // ── Dark palette (Material Design 3 tokens) ──────────────────────
    readonly property var dark: ({
        // Surface
        surface: "#0F172A",
        surfaceVariant: "#1E293B",
        card: "#1E293B",
        input: "#334155",
        sidebar: "#0F172A",
        navBar: "#0F172A",

        // Primary
        primary: "#38BDF8",
        primaryContainer: "#0C4A6E",
        onPrimary: "#0F172A",

        // Secondary
        secondary: "#818CF8",
        secondaryContainer: "#1E1B4B",

        // Text
        textPrimary: "#F1F5F9",
        textSecondary: "#94A3B8",
        textMuted: "#64748B",
        textOnPrimary: "#0F172A",

        // Status
        accent: "#FB7185",
        cyan: "#22D3EE",
        passGreen: "#4ADE80",
        warnYellow: "#FBBF24",
        failRed: "#F87171",
        skipGray: "#9CA3AF",
        infoBlue: "#60A5FA",

        // Borders
        borderCard: "#334155",
        borderSubtle: "#1E293B",
        borderFocused: "#38BDF8",

        // Overlay
        overlay: "#FFFFFF",
        hover: "#FFFFFF",
        ripple: "#FFFFFF",

        // Shadows
        shadow: "#000000"
    })

    // ── Active palette ───────────────────────────────────────────────
    readonly property var colors: effectiveMode === Light ? light : dark

    // ── Typography scale (Material Design 3 type scale) ──────────────
    readonly property string fontMono: "JetBrains Mono"
    readonly property string fontSans: "system-ui, -apple-system, sans-serif"

    readonly property var typography: ({
        displayLarge: 57, displayMedium: 45, displaySmall: 36,
        headlineLarge: 32, headlineMedium: 28, headlineSmall: 24,
        titleLarge: 22, titleMedium: 16, titleSmall: 14,
        bodyLarge: 16, bodyMedium: 14, bodySmall: 12,
        labelLarge: 14, labelMedium: 12, labelSmall: 11,
        monoCode: 12, monoLabel: 10
    })

    // ── Spacing scale (4px grid) ─────────────────────────────────────
    readonly property var spacing: ({
        xs: 4, sm: 8, md: 12, lg: 16, xl: 24, xxl: 32, xxxl: 48
    })

    // ── Corner radius scale ──────────────────────────────────────────
    readonly property var radius: ({
        xs: 4, sm: 6, md: 8, lg: 12, xl: 16, full: 9999
    })

    // ── Elevation / shadow ───────────────────────────────────────────
    readonly property var elevation: ({
        none: 0, low: 2, medium: 4, high: 8
    })

    // ── Convenience aliases (backward compat with existing code) ─────
    readonly property string bgDark:          colors.surface
    readonly property string bgSidebar:       colors.sidebar
    readonly property string bgCard:          colors.card
    readonly property string bgInput:         colors.input
    readonly property string textPrimary:     colors.textPrimary
    readonly property string textSecondary:   colors.textSecondary
    readonly property string textMuted:       colors.textMuted
    readonly property string accent:          colors.accent
    readonly property string accentBlue:      colors.secondary
    readonly property string cyan:            colors.cyan
    readonly property string passGreen:       colors.passGreen
    readonly property string warnYellow:      colors.warnYellow
    readonly property string failRed:         colors.failRed
    readonly property string skipGray:        colors.skipGray
    readonly property string infoBlue:        colors.infoBlue
    readonly property string borderCard:      colors.borderCard
    readonly property string borderSubtle:    colors.borderSubtle
    readonly property string borderFocused:   colors.borderFocused
    readonly property string monoFont:        fontMono
    readonly property real radiusCard:        radius.lg
    readonly property real radiusButton:      radius.md
    readonly property real radiusSmall:       radius.sm
    readonly property real sidebarWidth:      260
}
