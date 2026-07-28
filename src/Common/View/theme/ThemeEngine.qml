// =============================================================================
// ThemeEngine.qml — Runtime theme controller (singleton)
//
// STRICT RULE: minimize QML property count.  Too many properties (~97) in a
// singleton crashes the QML engine in static/cross-compiled builds.  JS objects
// are used for palette storage (2 properties vs 46 QML properties).
//
// Theme switching: JS assignment (NOT QML bindings) via applyTheme().
// Gated behind _ready flag to prevent cascading during singleton init.
//
// CANONICAL COLOR ACCESS: ThemeEngine.colors.xxx
//   All colors are accessed through the `colors` JS object.  The legacy
//   direct properties (bgDark, passGreen, etc.) below are deprecated
//   convenience aliases kept for backward compatibility.  New code MUST
//   use ThemeEngine.colors.xxx exclusively.
// =============================================================================
pragma Singleton
import QtQuick

QtObject {
    readonly property int litMode: 1
    readonly property int drkMode: 2
    property int mode: drkMode
    readonly property bool isDark: mode !== litMode
    readonly property bool isMobile: Qt.platform.os === "ios" || Qt.platform.os === "android"

    // ── Palettes as JS objects (2 properties — NOT 46 QML properties) ──
    readonly property var lightPalette: ({
        surface:          "#F8FAFC", sidebar:    "#FFFFFF",
        card:             "#FFFFFF", input:      "#F1F5F9",
        navBar:           "#FFFFFF",
        primary:          "#0EA5E9", primaryContainer: "#E0F2FE",
        secondary:        "#6366F1",
        textPrimary:      "#0F172A", textSecondary:   "#475569",
        textMuted:        "#64748B",
        accent:           "#F43F5E", cyan:            "#06B6D4",
        passGreen:        "#4ADE80", warnYellow:      "#FBBF24",
        failRed:          "#F87171", skipGray:        "#9CA3AF",
        infoBlue:         "#A5B4FC",
        borderCard:       "#E2E8F0", borderSubtle:    "#F1F5F9",
        borderFocused:    "#0EA5E9",
        textOnAccent:     "#0F172A"
    })

    readonly property var darkPalette: ({
        surface:          "#0F172A", sidebar:    "#0F172A",
        card:             "#1E293B", input:      "#334155",
        navBar:           "#0F172A",
        primary:          "#60C8F8", primaryContainer: "#0C4A6E",
        secondary:        "#818CF8",
        textPrimary:      "#F1F5F9", textSecondary:   "#94A3B8",
        textMuted:        "#94A3B8",
        accent:           "#FB7185", cyan:            "#68E5F4",
        passGreen:        "#4ADE80", warnYellow:      "#FBBF24",
        failRed:          "#F87171", skipGray:        "#9CA3AF",
        infoBlue:         "#A5B4FC",
        borderCard:       "#334155", borderSubtle:    "#1E293B",
        borderFocused:    "#60C8F8",
        textOnAccent:     "#0F172A"
    })

    // 5WHY: All 23 deprecated readonly alias properties were removed in
    // Cycle 28.  Every consumer (607 references) now uses the canonical
    // ThemeEngine.colors.xxx pattern.  Removing these reduces the QML
    // property count from ~85 to ~62 — safer against the ~97-property
    // crash threshold in static/cross-compiled builds.

    // ── Theme switching (imperative JS — gated to skip init) ──────────
    property bool _ready: false
    Component.onCompleted: {
        if (typeof appState !== 'undefined' && appState && appState.themeMode !== undefined) {
            mode = appState.themeMode
        }
        _ready = true
        applyTheme()
    }

    function applyTheme() {
        var p = (mode === litMode) ? lightPalette : darkPalette

        // 5WHY: Previously set 23 individual QML properties via direct
        // assignment (bgDark = p.surface, passGreen = p.passGreen, ...).
        // Now the direct properties are readonly aliases (colors.xxx), so
        // only the `colors` object needs updating.  This eliminates 23
        // property assignments per theme switch and makes the `colors`
        // object the single source of truth.
        colors = ({
            surface: p.surface,           sidebar: p.sidebar,
            card: p.card,                 input: p.input,
            navBar: p.navBar,             primary: p.primary,
            primaryContainer: p.primaryContainer,
            secondary: p.secondary,
            textPrimary: p.textPrimary,   textSecondary: p.textSecondary,
            textMuted: p.textMuted,       accent: p.accent,
            cyan: p.cyan,                 passGreen: p.passGreen,
            warnYellow: p.warnYellow,     failRed: p.failRed,
            skipGray: p.skipGray,         infoBlue: p.infoBlue,
            borderCard: p.borderCard,     borderSubtle: p.borderSubtle,
            borderFocused: p.borderFocused,
            textOnAccent: p.textOnAccent
        })
    }
    onModeChanged: { if (_ready) applyTheme() }

    // ── Canonical color access — single source of truth ────────────────
    // 5WHY: colors was initialized as empty {} — the deprecated readonly
    // aliases (bgDark: colors.surface) resolved to undefined between
    // singleton construction and Component.onCompleted→applyTheme().
    // Initializing with the dark palette default values ensures all
    // color properties are valid from t=0.
    property var colors: ({
        surface:          "#0F172A", sidebar:    "#0F172A",
        card:             "#1E293B", input:      "#334155",
        navBar:           "#0F172A", primary:    "#60C8F8",
        primaryContainer: "#0C4A6E", secondary:  "#818CF8",
        textPrimary:      "#F1F5F9", textSecondary: "#94A3B8",
        textMuted:        "#94A3B8", accent:     "#FB7185",
        cyan:             "#68E5F4", passGreen:  "#4ADE80",
        warnYellow:       "#FBBF24", failRed:    "#F87171",
        skipGray:         "#9CA3AF", infoBlue:   "#A5B4FC",
        borderCard:       "#334155", borderSubtle: "#1E293B",
        borderFocused:    "#60C8F8", textOnAccent: "#0F172A"
    })

    readonly property int toastDurationMs: 3500
    readonly property var radius: ({ xs: 4, sm: 6, md: 8, lg: 12, xl: 16, full: 9999 })
    readonly property string fontMono: "JetBrains Mono"
    readonly property string monoFont: fontMono

    function pad2(n) { return (n < 10 ? " " : "") + n }
}
